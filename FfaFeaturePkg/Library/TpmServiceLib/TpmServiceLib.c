/** @file
  Implementation for the TPM Service. This library is based
  off of the ARM spec: TPM Service Command Response Buffer
  Interface Over FF-A. The spec can be found here:

  https://developer.arm.com/documentation/den0138/0100/?lang=en

  The state flow is based off the TCG PC Client Specific Platform
  TPM Profile for TPM 2.0. The spec can be found here:

  https://trustedcomputinggroup.org/resource/pc-client-platform-tpm-profile-ptp-specification/

  Figure 4 - TPM State Diagram for CRB Interface

  Copyright (c), Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/TpmServiceLib.h>
#include <Library/TpmServiceStateTranslationLib.h>
#include <Guid/Tpm2ServiceFfa.h>
#include <IndustryStandard/TpmPtp.h>
#include <IndustryStandard/Tpm20.h>

/* TPM Service Defines */
#define TPM_MAJOR_VER  (0x1)
#define TPM_MINOR_VER  (0x0)

#define TPM_LOCALITY_OFFSET  (0x1000)

/* TPM Service States */
typedef enum {
  TPM_STATE_IDLE = 0,
  TPM_STATE_READY,
  TPM_STATE_COMPLETE,
  NUM_TPM_STATES
} TpmState;

/* TPM Locality States */
typedef enum {
  TPM_LOCALITY_CLOSED = 0,
  TPM_LOCALITY_OPEN,
  NUM_LOCALITY_STATES
} TpmLocalityState;

typedef UINTN TpmStatus;

/* TPM Service Variables */
STATIC TpmState                      mCurrentState;
STATIC UINT8                         mActiveLocality;
STATIC PTP_CRB_INTERFACE_IDENTIFIER  mInterfaceIdDefault;
STATIC TpmLocalityState              mLocalityStates[NUM_LOCALITIES] = { 0 };

/**
  Converts the passed in EFI_STATUS to a TPM_STATUS

  @param  Status   The EFI_STATUS to convert

  @retval TPM_STATUS_OK      Success
  @retval TPM_STATUS_INVARG  Invalid parameter
  @retval TPM_STATUS_DENIED  Access denied
  @retval TPM_STATUS_NOMEM   Insufficient memory

**/
STATIC
TpmStatus
ConvertEfiToTpmStatus (
  EFI_STATUS  Status
  )
{
  TpmStatus  ReturnVal;

  switch (Status) {
    case EFI_SUCCESS:
      ReturnVal = TPM2_FFA_SUCCESS_OK;
      break;

    case EFI_DEVICE_ERROR:
      ReturnVal = TPM2_FFA_ERROR_DENIED;
      break;

    case EFI_BUFFER_TOO_SMALL:
      ReturnVal = TPM2_FFA_ERROR_NOMEM;
      break;

    default:
      ReturnVal = TPM2_FFA_ERROR_DENIED;
      break;
  }

  return ReturnVal;
}

/**
  Initializes the internal CRB

  @param  Locality   The locality of the CRB

**/
STATIC
VOID
InitInternalCrb (
  UINT8  Locality
  )
{
  PTP_CRB_REGISTERS_PTR  InternalTpmCrb;

  InternalTpmCrb = (PTP_CRB_REGISTERS_PTR)(UINTN)(PcdGet64 (PcdTpmInternalBaseAddress) + (Locality * TPM_LOCALITY_OFFSET));
  DEBUG ((DEBUG_INFO, "Locality: %x - InternalTpmCrb Address: %lx\n", Locality, (UINTN)InternalTpmCrb));
  SetMem ((void *)InternalTpmCrb, sizeof (PTP_CRB_REGISTERS), 0x00);
  InternalTpmCrb->InterfaceId      = mInterfaceIdDefault.Uint32;
  InternalTpmCrb->CrbControlStatus = PTP_CRB_CONTROL_AREA_STATUS_TPM_IDLE;

  /* Set the CRB Command/Response buffer address + size. */
  InternalTpmCrb->CrbControlCommandAddressHigh = (UINT32)RShiftU64 ((UINTN)InternalTpmCrb->CrbDataBuffer, 32);
  InternalTpmCrb->CrbControlCommandAddressLow  = (UINT32)(UINTN)InternalTpmCrb->CrbDataBuffer;
  InternalTpmCrb->CrbControlCommandSize        = sizeof (InternalTpmCrb->CrbDataBuffer);
  InternalTpmCrb->CrbControlResponseAddrss     = (UINTN)InternalTpmCrb->CrbDataBuffer;
  InternalTpmCrb->CrbControlResponseSize       = sizeof (InternalTpmCrb->CrbDataBuffer);
}

/**
  Cleans the internal CRB putting registers into a known good state

**/
STATIC
VOID
CleanInternalCrb (
  VOID
  )
{
  PTP_CRB_REGISTERS_PTR  InternalTpmCrb;

  /* If the user has never requested a locality, don't clean, no need.
   * We should only ever clean the active locality as when localities change
   * we clear the entire CRB region. */
  if (mActiveLocality == NUM_LOCALITIES) {
    return;
  }

  InternalTpmCrb = (PTP_CRB_REGISTERS_PTR)(UINTN)(PcdGet64 (PcdTpmInternalBaseAddress) + (mActiveLocality * TPM_LOCALITY_OFFSET));

  /* Set the locality state based on the active locality. */
  switch (mActiveLocality) {
    case 0:
      InternalTpmCrb->LocalityState |= PTP_CRB_LOCALITY_STATE_ACTIVE_LOCALITY_0;
      break;

    case 1:
      InternalTpmCrb->LocalityState |= PTP_CRB_LOCALITY_STATE_ACTIVE_LOCALITY_1;
      break;

    case 2:
      InternalTpmCrb->LocalityState |= PTP_CRB_LOCALITY_STATE_ACTIVE_LOCALITY_2;
      break;

    case 3:
      InternalTpmCrb->LocalityState |= PTP_CRB_LOCALITY_STATE_ACTIVE_LOCALITY_3;
      break;

    case 4:
      InternalTpmCrb->LocalityState |= PTP_CRB_LOCALITY_STATE_ACTIVE_LOCALITY_4;
      break;

    default:
      break;
  }

  InternalTpmCrb->LocalityState      |= PTP_CRB_LOCALITY_STATE_TPM_REG_VALID_STATUS;
  InternalTpmCrb->LocalityState      |= PTP_CRB_LOCALITY_STATE_LOCALITY_ASSIGNED;
  InternalTpmCrb->LocalityStatus     |= PTP_CRB_LOCALITY_STATUS_GRANTED;
  InternalTpmCrb->LocalityControl     = 0;
  InternalTpmCrb->InterfaceId         = mInterfaceIdDefault.Uint32;
  InternalTpmCrb->CrbControlExtension = 0;
  InternalTpmCrb->CrbControlRequest   = 0;
  InternalTpmCrb->CrbControlCancel    = 0;
  InternalTpmCrb->CrbControlStart     = 0;
  InternalTpmCrb->CrbInterruptEnable  = 0;
  InternalTpmCrb->CrbInterruptStatus  = 0;

  /* Set the current TPM Status based on the current state. */
  if (mCurrentState == TPM_STATE_IDLE) {
    InternalTpmCrb->CrbControlStatus = PTP_CRB_CONTROL_AREA_STATUS_TPM_IDLE;
  } else {
    InternalTpmCrb->CrbControlStatus = 0;
  }

  /* Set the CRB Command/Response buffer address + size. */
  InternalTpmCrb->CrbControlCommandAddressHigh = (UINT32)RShiftU64 ((UINTN)InternalTpmCrb->CrbDataBuffer, 32);
  InternalTpmCrb->CrbControlCommandAddressLow  = (UINT32)(UINTN)InternalTpmCrb->CrbDataBuffer;
  InternalTpmCrb->CrbControlCommandSize        = sizeof (InternalTpmCrb->CrbDataBuffer);
  InternalTpmCrb->CrbControlResponseAddrss     = (UINTN)InternalTpmCrb->CrbDataBuffer;
  InternalTpmCrb->CrbControlResponseSize       = sizeof (InternalTpmCrb->CrbDataBuffer);

  /* Remaining registers can be ignored. */
}

/**
  Handles commands for the TPM service

  @retval TPM_STATUS_OK      Success
  @retval TPM_STATUS_INVARG  Invalid parameter
  @retval TPM_STATUS_DENIED  Access denied
  @retval TPM_STATUS_NOMEM   Insufficient memory

**/
STATIC
TpmStatus
HandleCommand (
  VOID
  )
{
  EFI_STATUS             Status;
  PTP_CRB_REGISTERS_PTR  InternalTpmCrb;

  InternalTpmCrb = (PTP_CRB_REGISTERS_PTR)(UINTN)(PcdGet64 (PcdTpmInternalBaseAddress) + (mActiveLocality * TPM_LOCALITY_OFFSET));

  /* Depending on our current state, we will investigate specific registers and
   * make state transitions or deny commands. */
  Status = EFI_ACCESS_DENIED;
  switch (mCurrentState) {
    /* The TPM can transition to IDLE from any state outside of command execution when the
     * SW sets the goIdle bit in the CrbControlRequest register. When the TPM transitions to
     * IDLE from COMPLETE it should clear the buffer. */
    case TPM_STATE_IDLE:
      /* Check the cmdReady bit in the CrbControlRequest register to see if we need to
       * transition to the READY state, otherwise, deny the request. */
      if (InternalTpmCrb->CrbControlRequest & PTP_CRB_CONTROL_AREA_REQUEST_COMMAND_READY) {
        DEBUG ((DEBUG_INFO, "IDLE State - Handle TPM Command cmdReady Request\n"));
        Status = TpmSstCmdReady (mActiveLocality);
        if (Status == EFI_SUCCESS) {
          mCurrentState = TPM_STATE_READY;
        }
      }

      break;

    /* The TPM can transition to READY from IDLE or COMPLETE when the SW sets the cmdReady bit
     * in the CrbControlRequest register. When the TPM transitions to READY from COMPLETE it
     * should clear the buffer. */
    case TPM_STATE_READY:
      /* Check the goIdle bit in the CrbControlRequest register to see if we need to
       * transition back to the IDLE state. */
      if (InternalTpmCrb->CrbControlRequest & PTP_CRB_CONTROL_AREA_REQUEST_GO_IDLE) {
        DEBUG ((DEBUG_INFO, "READY State - Handle TPM Command goIdle Request\n"));
        Status = TpmSstGoIdle (mActiveLocality);
        if (Status == EFI_SUCCESS) {
          mCurrentState = TPM_STATE_IDLE;
        }

        /* Check the cmdReady bit in the CrbControlRequest register, clear it if it has been
         * set again. */
      } else if (InternalTpmCrb->CrbControlRequest & PTP_CRB_CONTROL_AREA_REQUEST_COMMAND_READY) {
        DEBUG ((DEBUG_INFO, "READY State - Handle TPM Command cmdReady Request\n"));
        Status = TpmSstCmdReady (mActiveLocality);

        /* Check the CrbControlStart register to see if we need to start executing a command.
         * Once the command completes, transition to the COMPLETE state. */
      } else if (InternalTpmCrb->CrbControlStart & PTP_CRB_CONTROL_START) {
        DEBUG ((DEBUG_INFO, "READY State - Handle TPM Command Start Request\n"));
        Status = TpmSstStart (mActiveLocality, InternalTpmCrb);
        if (Status == EFI_SUCCESS) {
          mCurrentState = TPM_STATE_COMPLETE;
        }
      }

      break;

    /* The TPM can transition to COMPLETE only from READY when the SW writes a 1 to the
     * CrbControlStart register and the command execution finishes. The SW can write more
     * data to the buffer and set the register again to trigger another command execution;
     * this is only if TPM_CapCRBIdleBypass is 1. */
    case TPM_STATE_COMPLETE:
      /* Check the goIdle bit in the CrbControlRequest register to see if we need to
       * transition to the IDLE state. */
      if (InternalTpmCrb->CrbControlRequest & PTP_CRB_CONTROL_AREA_REQUEST_GO_IDLE) {
        DEBUG ((DEBUG_INFO, "COMPLETE State - Handle TPM Command goIdle Request\n"));
        Status = TpmSstGoIdle (mActiveLocality);
        if (Status == EFI_SUCCESS) {
          mCurrentState = TPM_STATE_IDLE;
          SetMem ((void *)InternalTpmCrb->CrbDataBuffer, sizeof (InternalTpmCrb->CrbDataBuffer), 0x00);
        }

        /* Check the cmdReady bit in the CrbControlRequest register to see if we need to
         * transition back to the READY state. */
      } else if (InternalTpmCrb->CrbControlRequest & PTP_CRB_CONTROL_AREA_REQUEST_COMMAND_READY) {
        /* Transition to READY from COMPLETE is only supported if TPM_CapCRBIdleBypass is 1.*/
        if (TpmSstIsIdleBypassSupported ()) {
          DEBUG ((DEBUG_INFO, "COMPLETE State - Handle TPM Command cmdReady Request\n"));
          Status = TpmSstCmdReady (mActiveLocality);
          if (Status == EFI_SUCCESS) {
            mCurrentState = TPM_STATE_READY;
            SetMem ((void *)InternalTpmCrb->CrbDataBuffer, sizeof (InternalTpmCrb->CrbDataBuffer), 0x00);
          }
        }

        /* Check the CrbControlStart register to see if we need to execute another command. */
      } else if (InternalTpmCrb->CrbControlStart & PTP_CRB_CONTROL_START) {
        /* Execution of another command from COMPLETE is only supported if TPM_CapCRBIdleBypass
         * is 1. */
        if (TpmSstIsIdleBypassSupported ()) {
          DEBUG ((DEBUG_INFO, "COMPLETE State - Handle TPM Command Start Request\n"));
          Status = TpmSstStart (mActiveLocality, InternalTpmCrb);
        }
      }

      break;

    /* The normal state flow should be: IDLE -> READY -> COMPLETE -> IDLE. */
    default:
      DEBUG ((DEBUG_ERROR, "INVALID State - Attempting to transition to IDLE State\n"));
      Status = TpmSstGoIdle (mActiveLocality);
      if (Status == EFI_SUCCESS) {
        mCurrentState = TPM_STATE_IDLE;
        SetMem ((void *)InternalTpmCrb->CrbDataBuffer, sizeof (InternalTpmCrb->CrbDataBuffer), 0x00);
      }

      break;
  }

  /* Clear the internal CRB start register to indicate successful completion and response ready */
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "Command Failed w/ Status: %x\n", Status));
  }

  return ConvertEfiToTpmStatus (Status);
}

/**
  Handles locality requests for the TPM service

  @param  Locality   The locality of the CRB

  @retval TPM_STATUS_OK      Success
  @retval TPM_STATUS_INVARG  Invalid parameter
  @retval TPM_STATUS_DENIED  Access denied
  @retval TPM_STATUS_NOMEM   Insufficient memory

**/
STATIC
TpmStatus
HandleLocalityRequest (
  UINT8  Locality
  )
{
  EFI_STATUS             Status;
  PTP_CRB_REGISTERS_PTR  InternalTpmCrb;
  UINT8                  ActiveLocality;

  InternalTpmCrb = (PTP_CRB_REGISTERS_PTR)(UINTN)(PcdGet64 (PcdTpmInternalBaseAddress) + (Locality * TPM_LOCALITY_OFFSET));

  /* Check if we are doing a locality relinquish */
  if (InternalTpmCrb->LocalityControl & PTP_CRB_LOCALITY_CONTROL_RELINQUISH) {
    /* Make sure the locality being relinquished is the active locality */
    if (Locality != mActiveLocality) {
      DEBUG ((DEBUG_ERROR, "Locality Relinquish Failed - Invalid Locality\n"));
      return TPM2_FFA_ERROR_DENIED;
    }

    DEBUG ((DEBUG_INFO, "Handle TPM Locality%x Relinquish\n", Locality));
    Status         = TpmSstLocalityRelinquish (Locality);
    ActiveLocality = NUM_LOCALITIES; // Invalid
    /* Check if we are doing a locality request */
  } else if (InternalTpmCrb->LocalityControl & PTP_CRB_LOCALITY_CONTROL_REQUEST_ACCESS) {
    /* Make sure there is no active locality if requesting a different locality */
    if ((mActiveLocality != NUM_LOCALITIES) && (mActiveLocality != Locality)) {
      DEBUG ((DEBUG_ERROR, "Locality Request Failed - Locality Not Relinquished\n"));
      return TPM2_FFA_ERROR_DENIED;
    }

    DEBUG ((DEBUG_INFO, "Handle TPM Locality%x Request\n", Locality));
    Status         = TpmSstLocalityRequest (Locality);
    ActiveLocality = Locality;
    /* Otherwise, the host didn't set the correct bits, invalid */
  } else {
    DEBUG ((DEBUG_ERROR, "Request/Relinquish Bit Not Set\n"));
    return TPM2_FFA_ERROR_DENIED;
  }

  /* Update the internal TPM CRB */
  if (Status == EFI_SUCCESS) {
    InitInternalCrb (Locality);
    mActiveLocality = ActiveLocality;
  } else {
    DEBUG ((DEBUG_ERROR, "Locality Request Failed w/ Status: %x\n", Status));
  }

  return ConvertEfiToTpmStatus (Status);
}

/**
  Handler for TPM service GetInterfaceVersion command

  @param  Request   The incoming message
  @param  Response  The outgoing message

  @retval TPM_STATUS_OK Success

**/
STATIC
TpmStatus
GetInterfaceVersionHandler (
  DIRECT_MSG_ARGS_EX  *Request,
  DIRECT_MSG_ARGS_EX  *Response
  )
{
  TpmStatus  ReturnVal;

  ReturnVal      = TPM2_FFA_SUCCESS_OK;
  Response->Arg0 = TPM2_FFA_SUCCESS_OK_RESULTS_RETURNED;
  Response->Arg1 = (TPM_MAJOR_VER << 16) | TPM_MINOR_VER;
  return ReturnVal;
}

/**
  Handler for TPM service GetFeatureInfo command

  @param  Request   The incoming message
  @param  Response  The outgoing message

  @retval TPM_STATUS_OK Success

**/
STATIC
TpmStatus
GetFeatureInfoHandler (
  DIRECT_MSG_ARGS_EX  *Request,
  DIRECT_MSG_ARGS_EX  *Response
  )
{
  TpmStatus  ReturnVal;

  ReturnVal      = TPM2_FFA_SUCCESS_OK;
  Response->Arg0 = TPM2_FFA_ERROR_NOTSUP;
  DEBUG ((DEBUG_ERROR, "Unsupported Function\n"));
  return ReturnVal;
}

/**
  Handler for TPM service Start command

  @param  Request   The incoming message
  @param  Response  The outgoing message

  @retval TPM_STATUS_OK      Success
  @retval TPM_STATUS_INVARG  Invalid parameter
  @retval TPM_STATUS_DENIED  Access denied
  @retval TPM_STATUS_NOMEM   Insufficient memory

**/
STATIC
TpmStatus
StartHandler (
  DIRECT_MSG_ARGS_EX  *Request,
  DIRECT_MSG_ARGS_EX  *Response
  )
{
  TpmStatus  ReturnVal;
  UINT16     Function;
  UINT8      Locality;

  ReturnVal = TPM2_FFA_SUCCESS_OK;
  Function  = Request->Arg1;
  Locality  = Request->Arg2;

  /* Check to make sure we received a valid locality */
  if (Locality >= NUM_LOCALITIES) {
    DEBUG ((DEBUG_ERROR, "Invalid Locality\n"));
    ReturnVal = TPM2_FFA_ERROR_INVARG;
    goto exit;
  }

  /* Check if the locality is open */
  if (mLocalityStates[Locality] == TPM_LOCALITY_CLOSED) {
    DEBUG ((DEBUG_ERROR, "Locality Closed\n"));
    ReturnVal = TPM2_FFA_ERROR_DENIED;
    goto exit;
  }

  /* Check if we are processing a command */
  if (Function == TPM2_FFA_START_FUNC_QUALIFIER_COMMAND) {
    /* We should only proceed if the locality being requested matches that of the
     * current locality that is active. */
    if (Locality == mActiveLocality) {
      ReturnVal = HandleCommand ();
    } else {
      ReturnVal = TPM2_FFA_ERROR_INVARG;
      DEBUG ((DEBUG_ERROR, "Locality Mismatch\n"));
    }

    /* Check if we are processing a locality request */
  } else if (Function == TPM2_FFA_START_FUNC_QUALIFIER_LOCALITY) {
    ReturnVal = HandleLocalityRequest (Locality);
    /* Otherwise, invalid function ID */
  } else {
    ReturnVal = TPM2_FFA_ERROR_INVARG;
    DEBUG ((DEBUG_ERROR, "Invalid Start Function\n"));
  }

exit:
  /* Clean up the internal CRB at the given locality */
  CleanInternalCrb ();

  Response->Arg0 = ReturnVal;
  return ReturnVal;
}

/**
  Handler for TPM service Register command

  @param  Request   The incoming message
  @param  Response  The outgoing message

  @retval TPM_STATUS_OK Success

**/
STATIC
TpmStatus
RegisterHandler (
  DIRECT_MSG_ARGS_EX  *Request,
  DIRECT_MSG_ARGS_EX  *Response
  )
{
  TpmStatus  ReturnVal;

  ReturnVal      = TPM2_FFA_SUCCESS_OK;
  Response->Arg0 = TPM2_FFA_ERROR_NOTSUP;
  DEBUG ((DEBUG_ERROR, "Unsupported Function\n"));
  return ReturnVal;
}

/**
  Handler for TPM service Unregister command

  @param  Request   The incoming message
  @param  Response  The outgoing message

  @retval TPM_STATUS_OK Success

**/
STATIC
TpmStatus
UnregisterHandler (
  DIRECT_MSG_ARGS_EX  *Request,
  DIRECT_MSG_ARGS_EX  *Response
  )
{
  TpmStatus  ReturnVal;

  ReturnVal      = TPM2_FFA_SUCCESS_OK;
  Response->Arg0 = TPM2_FFA_ERROR_NOTSUP;
  DEBUG ((DEBUG_ERROR, "Unsupported Function\n"));
  return ReturnVal;
}

/**
  Handler for TPM service Finish command

  @param  Request   The incoming message
  @param  Response  The outgoing message

  @retval TPM_STATUS_OK Success

**/
STATIC
TpmStatus
FinishHandler (
  DIRECT_MSG_ARGS_EX  *Request,
  DIRECT_MSG_ARGS_EX  *Response
  )
{
  TpmStatus  ReturnVal;

  ReturnVal      = TPM2_FFA_SUCCESS_OK;
  Response->Arg0 = TPM2_FFA_ERROR_NOTSUP;
  DEBUG ((DEBUG_ERROR, "Unsupported Function\n"));
  return ReturnVal;
}

/**
  Handler for TPM service Manage Locality command

  @param  Request   The incoming message
  @param  Response  The outgoing message

  @retval TPM_STATUS_OK      Success
  @retval TPM_STATUS_INVARG  Invalid parameter
  @retval TPM_STATUS_DENIED  Access denied

**/
STATIC
TpmStatus
ManageLocalityHandler (
  DIRECT_MSG_ARGS_EX  *Request,
  DIRECT_MSG_ARGS_EX  *Response
  )
{
  TpmStatus  ReturnVal;
  UINT16     LocalityOperation;
  UINT8      Locality;

  ReturnVal         = TPM2_FFA_SUCCESS_OK;
  LocalityOperation = Request->Arg1;
  Locality          = Request->Arg2;

  /* NOTE: The following command should only be coming
   *       from a logical sp owned by TF-A. */
  if (!(Request->SourceId & 0xFF00)) {
    DEBUG ((DEBUG_ERROR, "Invalid Source ID\n"));
    ReturnVal = TPM2_FFA_ERROR_DENIED;
    goto exit;
  }

  /* Check to make sure we received a valid locality */
  if (Locality >= NUM_LOCALITIES) {
    DEBUG ((DEBUG_ERROR, "Invalid Locality\n"));
    ReturnVal = TPM2_FFA_ERROR_INVARG;
    goto exit;
  }

  /* Check if there was a request to open a locality */
  if (LocalityOperation == TPM2_FFA_MANAGE_LOCALITY_OPEN) {
    DEBUG ((DEBUG_INFO, "Locality: %d Opened\n", Locality));
    /* Set the locality state to OPEN */
    mLocalityStates[Locality] = TPM_LOCALITY_OPEN;
    /* Check if there was a request to close a locality */
  } else if (LocalityOperation == TPM2_FFA_MANAGE_LOCALITY_CLOSE) {
    DEBUG ((DEBUG_INFO, "Locality: %d Closed\n", Locality));
    /* Set the locality state to CLOSED */
    mLocalityStates[Locality] = TPM_LOCALITY_CLOSED;
  } else {
    DEBUG ((DEBUG_ERROR, "Invalid Locality Operation\n"));
    ReturnVal = TPM2_FFA_ERROR_INVARG;
  }

exit:
  Response->Arg0 = ReturnVal;
  return ReturnVal;
}

/**
  Initializes the TPM service

**/
VOID
TpmServiceInit (
  VOID
  )
{
  UINT8  Locality;

  /* Initialize the default interface ID. */
  mInterfaceIdDefault.Uint32                = 0;
  mInterfaceIdDefault.Bits.InterfaceType    = 1; // CRB active
  mInterfaceIdDefault.Bits.InterfaceVersion = 1; // CRB interface version
  mInterfaceIdDefault.Bits.CapLocality      = 1; // 5 localities supported
  mInterfaceIdDefault.Bits.CapCRB           = 1; // CRB supported

  /* Initializes all of the localities. */
  for (Locality = 0; Locality < NUM_LOCALITIES; Locality++) {
    InitInternalCrb (Locality);
  }

  /* Initialize the TPM Service State Translation Library. */
  TpmSstInit ();

  /* Initialize our default state information. */
  mCurrentState   = TPM_STATE_IDLE;
  mActiveLocality = NUM_LOCALITIES; // Invalid - No active locality
}

/**
  De-initializes the TPM service

**/
VOID
TpmServiceDeInit (
  VOID
  )
{
  /* Nothing to DeInit */
}

/**
  Handler for TPM service commands

  @param  Request   The incoming message
  @param  Response  The outgoing message

**/
VOID
TpmServiceHandle (
  DIRECT_MSG_ARGS_EX  *Request,
  DIRECT_MSG_ARGS_EX  *Response
  )
{
  UINT64  Opcode;

  /* Validate the input parameters before attempting to dereference or pass them along */
  if ((Request == NULL) || (Response == NULL)) {
    return;
  }

  Opcode = Request->Arg0;

  switch (Opcode) {
    case TPM2_FFA_GET_INTERFACE_VERSION:
      GetInterfaceVersionHandler (Request, Response);
      break;

    case TPM2_FFA_GET_FEATURE_INFO:
      GetFeatureInfoHandler (Request, Response);
      break;

    case TPM2_FFA_START:
      StartHandler (Request, Response);
      break;

    case TPM2_FFA_REGISTER_FOR_NOTIFICATION:
      RegisterHandler (Request, Response);
      break;

    case TPM2_FFA_UNREGISTER_FROM_NOTIFICATION:
      UnregisterHandler (Request, Response);
      break;

    case TPM2_FFA_FINISH_NOTIFIED:
      FinishHandler (Request, Response);
      break;

    case TPM2_FFA_MANAGE_LOCALITY:
      ManageLocalityHandler (Request, Response);
      break;

    default:
      Response->Arg0 = TPM2_FFA_ERROR_NOFUNC;
      DEBUG ((DEBUG_ERROR, "Invalid TPM Service Opcode\n"));
      break;
  }
}
