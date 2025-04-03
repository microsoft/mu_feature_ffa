/** @file
  Implementation for the Notification Service

  Copyright (c), Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/NotificationServiceLib.h>
#include <Guid/NotificationServiceFfa.h>

/* Notification Service Defines */
#define NOTIFICATION_MAX_SERVICES  (16)
#define NOTIFICATION_MAX_IDS       (64)
#define NOTIFICATION_ID_NOT_FOUND  (-1)

/* Notification Service Structures */
typedef struct {
  UINT32     BitNum; // The OS translation bitmap value
  UINT32     IdNum;  // The logical ID for the service
  BOOLEAN    InUse;
} NotifBits;

typedef struct  {
  UINT8        ServiceUuid[16];
  NotifBits    ServiceBits[NOTIFICATION_MAX_IDS];
  BOOLEAN      InUse;
} NotifService;

/* Notification Service Variables */
STATIC BOOLEAN       IdsAcquired                                     = FALSE;
STATIC UINT16        SourceId                                        = 0;
STATIC UINT16        DestinationId                                   = 0;
STATIC UINT64        GlobalBitmask                                   = 0;
STATIC NotifService  NotificationServices[NOTIFICATION_MAX_SERVICES] = { 0 };

/**
  Checks if the ID passed in matches one stored within the service structure

  @param  IdNum   The ID number to look for
  @param  Service The service to search for the given ID

  @return The index of the ID if found, otherwise -1 (NOTIFICATION_ID_NOT_FOUND)

**/
STATIC
INT8
IsMatchingId (
  UINT32        IdNum,
  NotifService  *Service
  )
{
  UINT8  Index;

  /* Validate the incoming function parameters */
  if (Service == NULL) {
    return NOTIFICATION_ID_NOT_FOUND;
  }

  for (Index = 0; Index < NOTIFICATION_MAX_IDS; Index++) {
    if (Service->ServiceBits[Index].InUse && (Service->ServiceBits[Index].IdNum == IdNum)) {
      return Index;
    }
  }

  return NOTIFICATION_ID_NOT_FOUND;
}

/**
  Adds or removes service bit information to the local notification services struct array

  @param  Destroy   Whether or not we are adding or removing bit information
  @param  Request   The incoming message containing the bit information
  @param  Service   The service we are updating bit information for

  @retval NOTIFICATION_STATUS_SUCCESS           Success
  @retval NOTIFICATION_STATUS_INVALID_PARAMETER Invalid parameter

**/
STATIC
NotificationStatus
UpdateServiceBits (
  BOOLEAN             Destroy,
  DIRECT_MSG_ARGS_EX  *Request,
  NotifService        *Service
  )
{
  NotificationStatus  ReturnVal;
  INT8                FoundIndex;
  UINT8               IdIndex;
  UINT8               NumBits;
  UINTN               *Mappings;
  UINT8               MappingIndex;
  UINT32              NotificationId;
  UINT32              BitmapBitNum;
  BOOLEAN             EmptyFound;
  NotifService        TempService;
  UINT64              TempBitmask;

  /* Validate the incoming function parameters */
  if ((Request == NULL) || (Service == NULL)) {
    return NOTIFICATION_STATUS_INVALID_PARAMETER;
  }

  NumBits  = Request->Arg3;
  Mappings = &Request->Arg4;

  /* You must be setting/destroying at least one bit and no more than the service supports */
  if ((NumBits <= 0) || (NumBits > NOTIFICATION_MAX_IDS)) {
    DEBUG ((DEBUG_ERROR, "Invalid NumBits: %x\n", NumBits));
    return NOTIFICATION_STATUS_INVALID_PARAMETER;
  }

  ReturnVal = NOTIFICATION_STATUS_SUCCESS;

  /* Copy the current service structure and global bitmask to the temporaries */
  CopyMem (&TempService, Service, sizeof (NotifService));
  TempBitmask = GlobalBitmask;

  /* Need to go through all of the setup bits and update the structure */
  for (MappingIndex = 0; MappingIndex < NumBits; MappingIndex++) {
    NotificationId = Mappings[MappingIndex];
    BitmapBitNum   = (UINT64)Mappings[MappingIndex] >> 32;
    FoundIndex     = IsMatchingId (NotificationId, &TempService);

    if (Destroy) {
      /* If we can not find the ID to destroy, it is an error */
      if (FoundIndex == NOTIFICATION_ID_NOT_FOUND) {
        DEBUG ((DEBUG_ERROR, "Invalid Destroy ID: %x Not Found\n", NotificationId));
        ReturnVal = NOTIFICATION_STATUS_INVALID_PARAMETER;
        break;
        /* If the bitmask bit is not set, it is an error */
      } else if (!(GlobalBitmask & (1 << BitmapBitNum))) {
        DEBUG ((DEBUG_ERROR, "Invalid Destroy Bitmap Bit: %x Not Set\n", BitmapBitNum));
        ReturnVal = NOTIFICATION_STATUS_INVALID_PARAMETER;
        break;
      } else {
        /* Clear the data */
        TempService.ServiceBits[FoundIndex].BitNum = 0;
        TempService.ServiceBits[FoundIndex].IdNum  = 0;
        TempService.ServiceBits[FoundIndex].InUse  = FALSE;
        TempBitmask                               &= ~(1 << BitmapBitNum);
      }
    } else {
      /* If we can find the ID to setup, it is an error */
      if (FoundIndex != NOTIFICATION_ID_NOT_FOUND) {
        DEBUG ((DEBUG_ERROR, "Invalid Setup ID: %x Found\n", NotificationId));
        ReturnVal = NOTIFICATION_STATUS_INVALID_PARAMETER;
        break;
        /* If the bitmas bit is set, it is an error */
      } else if (GlobalBitmask & (1 << BitmapBitNum)) {
        DEBUG ((DEBUG_ERROR, "Invalid Setup Bitmap Bit: %x Set\n", BitmapBitNum));
        ReturnVal = NOTIFICATION_STATUS_INVALID_PARAMETER;
        break;
      } else {
        /* Need to loop through the bits within the structure and find an empty location */
        EmptyFound = FALSE;
        for (IdIndex = 0; IdIndex < NOTIFICATION_MAX_IDS; IdIndex++) {
          if (!TempService.ServiceBits[IdIndex].InUse) {
            EmptyFound = TRUE;
            break;
          }
        }

        /* If we can not find an empty space, it is an error */
        if (!EmptyFound) {
          DEBUG ((DEBUG_ERROR, "Setup Failed - No Memory Available\n"));
          ReturnVal = NOTIFICATION_STATUS_NO_MEM;
          break;
        } else {
          /* Update the data */
          TempService.ServiceBits[IdIndex].BitNum = BitmapBitNum;
          TempService.ServiceBits[IdIndex].IdNum  = NotificationId;
          TempService.ServiceBits[IdIndex].InUse  = TRUE;
          TempBitmask                            |= (1 << BitmapBitNum);
        }
      }
    }
  }

  /* Copy the temporaries back if everything was successful */
  if (ReturnVal == NOTIFICATION_STATUS_SUCCESS) {
    CopyMem (Service, &TempService, sizeof (NotifService));
    GlobalBitmask = TempBitmask;
  }

  return ReturnVal;
}

/**
  Handler for Notification Query command

  @param  Request   The incoming message
  @param  Response  The outgoing message

  @retval NOTIFICATION_STATUS_GENERIC_ERROR Unsupported Function

**/
STATIC
NotificationStatus
QueryHandler (
  DIRECT_MSG_ARGS_EX  *Request,
  DIRECT_MSG_ARGS_EX  *Response
  )
{
  /* TODO: Remove when/if we decided to support Query */
  Response->Arg0 = NOTIFICATION_STATUS_GENERIC_ERROR;
  DEBUG ((DEBUG_ERROR, "Notification Service Query Unsupported\n"));
  return NOTIFICATION_STATUS_GENERIC_ERROR;

  UINT8               Index;
  UINT8               NumIds;
  NotifService        *Service;
  UINT8               Uuid[16];
  NotificationStatus  ReturnVal;

  Service   = NULL;
  ReturnVal = NOTIFICATION_STATUS_INVALID_PARAMETER;

  /* Extract the UUID from the message */
  NotificationServiceExtractUuid (Request, Uuid);

  /* Attempt to find the UUID within our list */
  for (Index = 0; Index < NOTIFICATION_MAX_SERVICES; Index++) {
    if (!CompareMem (Uuid, NotificationServices[Index].ServiceUuid, sizeof (Uuid))) {
      Service = &NotificationServices[Index];
      break;
    }
  }

  /* Check for a valid UUID */
  if (Service != NULL) {
    /* Count the number of IDs that are in use */
    NumIds = 0;
    for (Index = 0; Index < NOTIFICATION_MAX_IDS; Index++) {
      if (Service->ServiceBits[Index].InUse) {
        NumIds++;
      }
    }

    Response->Arg1 = NumIds;
    ReturnVal      = NOTIFICATION_STATUS_SUCCESS;
  } else {
    DEBUG ((DEBUG_ERROR, "Service Query Failed - Error Code: %x\n", ReturnVal));
  }

  Response->Arg0 = ReturnVal;
  return ReturnVal;
}

/**
  Handler for Notification Setup command

  @param  Request   The incoming message
  @param  Response  The outgoing message

  @retval NOTIFICATION_STATUS_SUCCESS           Success
  @retval NOTIFICATION_STATUS_INVALID_PARAMETER Invalid parameter

**/
STATIC
NotificationStatus
SetupHandler (
  DIRECT_MSG_ARGS_EX  *Request,
  DIRECT_MSG_ARGS_EX  *Response
  )
{
  UINT8               Index;
  NotifService        *Service;
  UINT8               Uuid[16];
  NotificationStatus  ReturnVal;

  Service   = NULL;
  ReturnVal = NOTIFICATION_STATUS_NO_MEM;

  /* Extract the UUID from the message */
  NotificationServiceExtractUuid (Request, Uuid);

  /* If we haven't stored the source and destination IDs, store them */
  if (!IdsAcquired) {
    SourceId      = Request->SourceId;
    DestinationId = Request->DestinationId;
    IdsAcquired   = TRUE;
  }

  /* Attempt to find the UUID within our list */
  for (Index = 0; Index < NOTIFICATION_MAX_SERVICES; Index++) {
    if (!CompareMem (Uuid, NotificationServices[Index].ServiceUuid, sizeof (Uuid))) {
      Service = &NotificationServices[Index];
      break;
    }
  }

  /* The UUID was not found in the list, attempt to find an empty location to add it */
  if (Service == NULL) {
    for (Index = 0; Index < NOTIFICATION_MAX_SERVICES; Index++) {
      if (!NotificationServices[Index].InUse) {
        Service = &NotificationServices[Index];
        CopyMem (Service->ServiceUuid, Uuid, sizeof (Uuid));
        Service->InUse = TRUE;
        break;
      }
    }
  }

  /* Check for a valid UUID and validate the input parameters */
  if (Service != NULL) {
    ReturnVal = UpdateServiceBits (FALSE, Request, Service);
  } else {
    DEBUG ((DEBUG_ERROR, "Service Setup Failed - Error Code: %x\n", ReturnVal));
  }

  Response->Arg0 = ReturnVal;
  return ReturnVal;
}

/**
  Handler for Notification Destroy command

  @param  Request   The incoming message
  @param  Response  The outgoing message

  @retval NOTIFICATION_STATUS_SUCCESS           Success
  @retval NOTIFICATION_STATUS_INVALID_PARAMETER Invalid parameter

**/
STATIC
NotificationStatus
DestroyHandler (
  DIRECT_MSG_ARGS_EX  *Request,
  DIRECT_MSG_ARGS_EX  *Response
  )
{
  UINT8               Index;
  NotifService        *Service;
  UINT8               Uuid[16];
  NotificationStatus  ReturnVal;

  Service   = NULL;
  ReturnVal = NOTIFICATION_STATUS_INVALID_PARAMETER;

  /* Extract the UUID from the message */
  NotificationServiceExtractUuid (Request, Uuid);

  /* Attempt to find the UUID within our list */
  for (Index = 0; Index < NOTIFICATION_MAX_SERVICES; Index++) {
    if (!CompareMem (Uuid, NotificationServices[Index].ServiceUuid, sizeof (Uuid))) {
      Service = &NotificationServices[Index];
      break;
    }
  }

  /* Check for a valid UUID and validate the input parameters */
  if (Service != NULL) {
    ReturnVal = UpdateServiceBits (TRUE, Request, Service);
  } else {
    DEBUG ((DEBUG_ERROR, "Service Destroy Failed - Error Code: %x\n", ReturnVal));
  }

  Response->Arg0 = ReturnVal;
  return ReturnVal;
}

/**
  Initializes the Notification service

**/
VOID
NotificationServiceInit (
  VOID
  )
{
  UINT8  Index;

  /* Initialize Global Values */
  IdsAcquired   = FALSE;
  SourceId      = 0;
  DestinationId = 0;
  GlobalBitmask = 0;

  for (Index = 0; Index < NOTIFICATION_MAX_SERVICES; Index++) {
    SetMem (&NotificationServices[Index], sizeof (NotifService), 0);
  }
}

/**
  Deinitializes the Notification service

**/
VOID
NotificationServiceDeInit (
  VOID
  )
{
  /* Nothing to DeInit */
}

/**
  Handler for Notification service commands

  @param  Request   The incoming message
  @param  Response  The outgoing message

**/
VOID
NotificationServiceHandle (
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
    case NOTIFICATION_OPCODE_QUERY:
      QueryHandler (Request, Response);
      break;

    case NOTIFICATION_OPCODE_SETUP:
      SetupHandler (Request, Response);
      break;

    case NOTIFICATION_OPCODE_DESTROY:
      DestroyHandler (Request, Response);
      break;

    default:
      Response->Arg0 = NOTIFICATION_STATUS_INVALID_PARAMETER;
      DEBUG ((DEBUG_ERROR, "Invalid Notification Service Opcode\n"));
      break;
  }
}

/**
  Calls NotificationSet on the given ID with the given flag

  @param  Id           The ID to trigger the event on
  @param  ServiceUuid  The service containing the ID to trigger
  @param  Flag         The NotificationSet flag to use

  @retval NOTIFICATION_STATUS_SUCCESS           Success
  @retval NOTIFICATION_STATUS_INVALID_PARAMETER Invalid parameter
  @retval NOTIFICATION_STATUS_GENERIC_ERROR     NotificationSet failed

**/
NotificationStatus
NotificationServiceIdSet (
  UINT32  Id,
  UINT8   *ServiceUuid,
  UINT32  Flag
  )
{
  NotifService        *Service;
  NotificationStatus  ReturnVal;
  UINT8               Index;
  UINT64              Bitmask;
  EFI_STATUS          Status;

  /* Validate the incoming function parameters */
  if (ServiceUuid == NULL) {
    return NOTIFICATION_STATUS_INVALID_PARAMETER;
  }

  Service   = NULL;
  ReturnVal = NOTIFICATION_STATUS_INVALID_PARAMETER;

  /* Attempt to find the UUID within our list */
  for (Index = 0; Index < NOTIFICATION_MAX_SERVICES; Index++) {
    if (!CompareMem (ServiceUuid, NotificationServices[Index].ServiceUuid, sizeof (NotificationServices[Index].ServiceUuid))) {
      Service = &NotificationServices[Index];
      break;
    }
  }

  /* UUID was found */
  if (Service != NULL) {
    /* Attempt to find the logical ID within the mapped list */
    for (Index = 0; Index < NOTIFICATION_MAX_IDS; Index++) {
      if (Service->ServiceBits[Index].IdNum == Id) {
        Bitmask = (1 << Service->ServiceBits[Index].BitNum);
        Status  = FfaNotificationSet (SourceId, Flag, Bitmask);
        if (EFI_ERROR (Status)) {
          ReturnVal = NOTIFICATION_STATUS_GENERIC_ERROR;
        } else {
          ReturnVal = NOTIFICATION_STATUS_SUCCESS;
        }

        break;
      }
    }
  }

  return ReturnVal;
}

/**
  Extracts the UUID from the message arguments

  @param  Request   The incoming message to extract the UUID from
  @param  Uuid      The UUID to populate

**/
VOID
NotificationServiceExtractUuid (
  DIRECT_MSG_ARGS_EX  *Request,
  UINT8               *Uuid
  )
{
  UINT64  UuidLo;
  UINT64  UuidHi;
  UINT8   Index;
  UINT8   UuidHiByte;
  UINT8   UuidLoByte;

  /* Validate the incoming function parameters */
  if ((Request == NULL) || (Uuid == NULL)) {
    return;
  }

  UuidLo = Request->Arg1;
  UuidHi = Request->Arg2;

  /* Copy the upper 8 bytes */
  for (Index = 0; Index < 8; Index++) {
    UuidHiByte  = (UINT8)(UuidHi >> ((7 - Index) * 8));
    Uuid[Index] = UuidHiByte;
  }

  /* Copy the lower 8 bytes */
  for (Index = 8; Index < 16; Index++) {
    UuidLoByte  = (UINT8)(UuidLo >> ((15 - Index) * 8));
    Uuid[Index] = UuidLoByte;
  }
}
