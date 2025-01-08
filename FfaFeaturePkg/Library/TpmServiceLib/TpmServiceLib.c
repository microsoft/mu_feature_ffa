/** @file
  Implementation for the TPM Service

  Copyright (c), Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/TpmServiceLib.h>
#include <Guid/Tpm2ServiceFfa.h>
#include <Library/Tpm2DeviceLib.h>
#include <IndustryStandard/TpmPtp.h>

/* TPM Service Defines */
#define TPM_MAJOR_VER  (1)
#define TPM_MINOR_VER  (0)

#define TPM_START_PROCESS_CMD      (0)
#define TPM_START_PROCESS_LOC_REQ  (1)

#define TPM_LOC_STATE_MASK   (0x9F)
#define TPM_LOC_STS_MASK     (0x01)
#define TPM_CTRL_START_MASK  (0x01)

// Default Value - tpmEstablished
#define TPM_LOC_STATE_DEFAULT  (0x01)
// Default Value - CRB Interface Selected, Only Locality0 Supported
#define TPM_INTERFACE_ID_DEFAULT  (0x4011)

typedef UINTN TpmStatus;

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

**/
STATIC
VOID
InitInternalCrb (
  VOID
  )
{
  /* TODO: If we end up supporting more than Locality0, this will need to init all localities supported */
  PTP_CRB_REGISTERS_PTR  InternalTpmCrb;

  InternalTpmCrb = (PTP_CRB_REGISTERS_PTR)(UINTN)PcdGet64 (PcdTpmInternalBaseAddress);
  DEBUG ((DEBUG_INFO, "PcdTpmInternalBaseAddress: %lx\n", PcdGet64 (PcdTpmInternalBaseAddress)));
  SetMem ((void *)InternalTpmCrb, sizeof (PTP_CRB_REGISTERS), 0x00);
  InternalTpmCrb->LocalityState = TPM_LOC_STATE_DEFAULT;
  InternalTpmCrb->InterfaceId   = TPM_INTERFACE_ID_DEFAULT;
}

/**
  Updates the internal CRB with the locality information for the locality requested

**/
STATIC
VOID
UpdateInternalCrb (
  VOID
  )
{
  /* TODO: If we end up supporting more than Locality0, this will need to update the correct locality registers */
  PTP_CRB_REGISTERS_PTR  InternalTpmCrb;

  InternalTpmCrb                 = (PTP_CRB_REGISTERS_PTR)(UINTN)PcdGet64 (PcdTpmInternalBaseAddress);
  InternalTpmCrb->LocalityState |= PTP_CRB_LOCALITY_STATE_TPM_REG_VALID_STATUS;
  InternalTpmCrb->LocalityState &= ~(PTP_CRB_LOCALITY_STATE_ACTIVE_LOCALITY_MASK);
  /* NOTE: Leaving the ActiveLocality as 0 as that is our only supported locality */
  InternalTpmCrb->LocalityState |= PTP_CRB_LOCALITY_STATE_LOCALITY_ASSIGNED;
  InternalTpmCrb->LocalityState |= PTP_CRB_LOCALITY_STATE_TPM_ESTABLISHED;
  DEBUG ((DEBUG_INFO, "LocalityState: %x\n", InternalTpmCrb->LocalityState));
  InternalTpmCrb->LocalityStatus |= PTP_CRB_LOCALITY_STATUS_GRANTED;
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
  /* TODO: If we end up supporting more than Locality0, this will need to clean the correct locality registers */
  PTP_CRB_REGISTERS_PTR  InternalTpmCrb;

  InternalTpmCrb                      = (PTP_CRB_REGISTERS_PTR)(UINTN)PcdGet64 (PcdTpmInternalBaseAddress);
  InternalTpmCrb->LocalityState      &= TPM_LOC_STATE_MASK;
  InternalTpmCrb->LocalityControl     = 0;
  InternalTpmCrb->LocalityStatus     &= TPM_LOC_STS_MASK;
  InternalTpmCrb->InterfaceId         = TPM_INTERFACE_ID_DEFAULT;
  InternalTpmCrb->CrbControlExtension = 0;
  InternalTpmCrb->CrbControlRequest   = 0;
  InternalTpmCrb->CrbControlStatus    = PTP_CRB_CONTROL_AREA_STATUS_TPM_IDLE;
  InternalTpmCrb->CrbControlCancel    = 0;
  InternalTpmCrb->CrbControlStart    &= TPM_CTRL_START_MASK;
  InternalTpmCrb->CrbInterruptEnable  = 0;
  InternalTpmCrb->CrbInterruptStatus  = 0;
  /* Remaining registers can be ignored */
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
  PTP_CRB_REGISTERS_PTR  InternalTpmCrb;

  DEBUG ((DEBUG_INFO, "Handle TPM Command\n"));
  InternalTpmCrb = (PTP_CRB_REGISTERS_PTR)(UINTN)PcdGet64 (PcdTpmInternalBaseAddress);

  /* Make sure the internal CRB was set to start a command */
  if (InternalTpmCrb->CrbControlStart != 1) {
    return TPM2_FFA_ERROR_DENIED;
  }

  /* Set the status to ready (i.e. not idle) */
  InternalTpmCrb->CrbControlStatus = 0;

  /* Copy the command data to the static buffer */
  UINT8   TpmCommandBuffer[sizeof (InternalTpmCrb->CrbDataBuffer)];
  UINT32  ResponseDataLen = InternalTpmCrb->CrbControlResponseSize;
  UINT32  CommandDataLen  = InternalTpmCrb->CrbControlCommandSize;

  CopyMem (TpmCommandBuffer, InternalTpmCrb->CrbDataBuffer, CommandDataLen);

  /* Submit the command to the TPM */
  EFI_STATUS  Status = Tpm2SubmitCommand (
                         CommandDataLen,
                         TpmCommandBuffer,
                         &ResponseDataLen,
                         TpmCommandBuffer
                         );

  /* Copy the response data from the static buffer */
  CopyMem (InternalTpmCrb->CrbDataBuffer, TpmCommandBuffer, ResponseDataLen);

  /* Clear the internal CRB start register to indicate successful completion and response ready */
  if (Status == EFI_SUCCESS) {
    InternalTpmCrb->CrbControlStart = 0;
  } else {
    DEBUG ((DEBUG_ERROR, "Command Failed w/ Status: %x\n", Status));
  }

  /* Set the status to idle */
  InternalTpmCrb->CrbControlStatus = PTP_CRB_CONTROL_AREA_STATUS_TPM_IDLE;

  return ConvertEfiToTpmStatus (Status);
}

/**
  Handles locality requests for the TPM service

  @retval TPM_STATUS_OK      Success
  @retval TPM_STATUS_INVARG  Invalid parameter
  @retval TPM_STATUS_DENIED  Access denied
  @retval TPM_STATUS_NOMEM   Insufficient memory

**/
STATIC
TpmStatus
HandleLocalityRequest (
  VOID
  )
{
  EFI_STATUS  Status;

  DEBUG ((DEBUG_INFO, "Handle TPM Locality Request\n"));

  /* Request to use the TPM */
  Status = Tpm2RequestUseTpm ();

  /* Update the internal TPM CRB */
  if (Status == EFI_SUCCESS) {
    UpdateInternalCrb ();
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
  UINT8      Function;
  UINT8      Locality;

  ReturnVal = TPM2_FFA_SUCCESS_OK;
  Function  = Request->Arg1;
  Locality  = Request->Arg2;

  /* Based on the function we will need to read from the corresponding address
    * register in the tpm_crb to know where to pull the data from:
    * NOTE: function = 0, command is ready to be processed
    *       function = 1, locality request is ready to be processed
    *       locality = 0...4, the locality where the command or request is located */
  /* TODO: Currently only Locality0 is available, if this needs to change, we should
   *       update the PCD base address with the appropriate locality offset. */
  if (Locality != PTP_CRB_LOCALITY_STATE_ACTIVE_LOCALITY_0) {
    Response->Arg0 = TPM2_FFA_ERROR_INVARG;
    DEBUG ((DEBUG_ERROR, "Invalid Locality\n"));
    return TPM2_FFA_ERROR_INVARG;
  }

  if (Function == TPM_START_PROCESS_CMD) {
    ReturnVal = HandleCommand ();
  } else if (Function == TPM_START_PROCESS_LOC_REQ) {
    ReturnVal = HandleLocalityRequest ();
  } else {
    ReturnVal = TPM2_FFA_ERROR_INVARG;
    DEBUG ((DEBUG_ERROR, "Invalid Start Function\n"));
  }

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
  Initializes the TPM service

**/
VOID
TpmServiceInit (
  VOID
  )
{
  InitInternalCrb ();
}

/**
  Deinitializes the TPM service

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

    default:
      Response->Arg0 = TPM2_FFA_ERROR_NOFUNC;
      DEBUG ((DEBUG_ERROR, "Invalid TPM Service Opcode\n"));
      break;
  }
}
