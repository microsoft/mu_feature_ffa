/** @file
  Implementation for the Notification Service

  Copyright (c), Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/NotificationServiceLib.h>

/* Notification Service Defines */
#define NOTIFICATION_MAX_SERVICES  (16)
#define NOTIFICATION_MAX_IDS       (64)
#define NOTIFICATION_ID_NOT_FOUND  (-1)

/* Notification Service Structures */
typedef struct {
  UINT16     BitNum; // The OS translation bitmap value
  UINT16     IdNum;  // The logical ID for the service
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
  UINT16        IdNum,
  NotifService  *Service
  )
{
  /* Validate the incoming function parameters */
  if (Service == NULL) {
    return NOTIFICATION_ID_NOT_FOUND;
  }

  for (UINT8 i = 0; i < NOTIFICATION_MAX_IDS; i++) {
    if (Service->ServiceBits[i].InUse && (Service->ServiceBits[i].IdNum == IdNum)) {
      return i;
    }
  }

  return NOTIFICATION_ID_NOT_FOUND;
}

/**
  Validates the incoming message parameters

  @param  Destroy   Whether or not we are adding or removing bit information
  @param  Request   The incoming message containing the bit information to validate
  @param  Service   The service we are updating bit information for

  @retval TRUE  Parameters are valid
  @retval FALSE Parameters are invalid

**/
STATIC
BOOLEAN
ValidParameters (
  BOOLEAN             Destroy,
  DIRECT_MSG_ARGS_EX  *Request,
  NotifService        *Service
  )
{
  /* Validate the incoming function parameters */
  if ((Request == NULL) || (Service == NULL)) {
    return FALSE;
  }

  /* Validate the setup and destroy parameters for the service. You must be setting/destroying at
    * least one bit and you can't set more than the service supports. */
  UINT8  NumBits = Request->Arg3;

  if ((NumBits <= 0) || (NumBits > NOTIFICATION_MAX_IDS)) {
    return FALSE;
  }

  UINTN  *Mappings = &Request->Arg4;

  for (UINT8 i = 0; i < NumBits; i++) {
    /* Validate that a user is not trying to setup an ID that has already been setup previously or
      * attempting to destroy an ID that doesn't exist. */
    UINT16  NotificationId = Mappings[i];
    INT8    IdIndex        = IsMatchingId (NotificationId, Service);
    if (!Destroy && (IdIndex != NOTIFICATION_ID_NOT_FOUND)) {
      return FALSE;
    } else if (Destroy && (IdIndex == NOTIFICATION_ID_NOT_FOUND)) {
      return FALSE;
    }

    /* Validate that a user is not trying to set a bit that is already in use or destroy a bit
      * they haven't previously set up. */
    UINT16  BitmapBitNum = (Mappings[i] >> 16);
    if (!Destroy && (GlobalBitmask & (1 << BitmapBitNum))) {
      return FALSE;
    } else if (Destroy && (Service->ServiceBits[IdIndex].BitNum != BitmapBitNum)) {
      return FALSE;
    }
  }

  return TRUE;
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
  /* Validate the incoming function parameters */
  if ((Request == NULL) || (Service == NULL)) {
    return NOTIFICATION_STATUS_INVALID_PARAMETER;
  }

  NotificationStatus  ReturnVal = NOTIFICATION_STATUS_INVALID_PARAMETER;
  UINT8               NumBits   = Request->Arg3;

  /* Need to go through all of the setup bits and update the structure */
  UINTN  *Mappings = &Request->Arg4;

  for (UINT8 i = 0; i < NumBits; i++) {
    UINT16  NotificationId = Mappings[i];
    UINT16  BitmapBitNum   = Mappings[i] >> 16;

    if (Destroy) {
      /* If we can not find the ID to destroy, it is an error */
      INT8  Index = IsMatchingId (NotificationId, Service);
      if (Index != NOTIFICATION_ID_NOT_FOUND) {
        Service->ServiceBits[Index].BitNum = 0;
        Service->ServiceBits[Index].IdNum  = 0;
        Service->ServiceBits[Index].InUse  = FALSE;

        /* Clear the global bitmask bit */
        GlobalBitmask &= ~(1 << BitmapBitNum);
        ReturnVal      = NOTIFICATION_STATUS_SUCCESS;
      }
    } else {
      /* Need to loop through the bits within the structure and find an empty location, if
        * unable to, we ran out of room, return an error */
      UINT8    Index;
      BOOLEAN  EmptyFound = FALSE;
      for (Index = 0; Index < NOTIFICATION_MAX_IDS; Index++) {
        if (!Service->ServiceBits[Index].InUse) {
          EmptyFound = TRUE;
          break;
        }
      }

      /* If we can not find an empty space, it is an error */
      if (EmptyFound) {
        Service->ServiceBits[Index].BitNum = BitmapBitNum;
        Service->ServiceBits[Index].IdNum  = NotificationId;
        Service->ServiceBits[Index].InUse  = TRUE;

        /* Set the global bitmask bit */
        GlobalBitmask |= (1 << BitmapBitNum);
        ReturnVal      = NOTIFICATION_STATUS_SUCCESS;
      }
    }
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

  NotifService        *Service  = NULL;
  UINT8               Uuid[16]  = { 0 };
  NotificationStatus  ReturnVal = NOTIFICATION_STATUS_INVALID_PARAMETER;

  /* Extract the UUID from the message */
  NotificationServiceExtractUuid (Request, Uuid);

  /* Attempt to find the UUID within our list */
  for (UINT8 i = 0; i < NOTIFICATION_MAX_SERVICES; i++) {
    if (!CompareMem (Uuid, NotificationServices[i].ServiceUuid, sizeof (Uuid))) {
      Service = &NotificationServices[i];
      break;
    }
  }

  /* Check for a valid UUID */
  if (Service != NULL) {
    /* Count the number of IDs that are in use */
    UINT8  NumIds = 0;
    for (UINT8 i = 0; i < NOTIFICATION_MAX_IDS; i++) {
      if (Service->ServiceBits[NumIds].InUse) {
        NumIds++;
      }
    }

    Response->Arg1 = NumIds;
    ReturnVal      = NOTIFICATION_STATUS_SUCCESS;
  } else {
    DEBUG ((DEBUG_ERROR, "Invalid Notification Service UUID\n"));
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
  NotifService        *Service  = NULL;
  UINT8               Uuid[16]  = { 0 };
  NotificationStatus  ReturnVal = NOTIFICATION_STATUS_INVALID_PARAMETER;

  /* Extract the UUID from the message */
  NotificationServiceExtractUuid (Request, Uuid);

  /* If we haven't stored the source and destination IDs, store them */
  if (!IdsAcquired) {
    SourceId      = Request->SourceId;
    DestinationId = Request->DestinationId;
    IdsAcquired   = TRUE;
  }

  /* Attempt to find the UUID within our list */
  for (UINT8 i = 0; i < NOTIFICATION_MAX_SERVICES; i++) {
    if (!CompareMem (Uuid, NotificationServices[i].ServiceUuid, sizeof (Uuid))) {
      Service = &NotificationServices[i];
      break;
    }
  }

  /* The UUID was not found in the list, attempt to find an empty location to add it */
  if (Service == NULL) {
    for (UINT8 i = 0; i < NOTIFICATION_MAX_SERVICES; i++) {
      if (!NotificationServices[i].InUse) {
        Service = &NotificationServices[i];
        CopyMem (Service->ServiceUuid, Uuid, sizeof (Uuid));
        Service->InUse = TRUE;
        break;
      }
    }
  }

  /* Check for a valid UUID and validate the input parameters */
  if ((Service != NULL) && (ValidParameters (FALSE, Request, Service))) {
    ReturnVal = UpdateServiceBits (FALSE, Request, Service);
  } else if (Service != NULL) {
    DEBUG ((DEBUG_ERROR, "Invalid Parameters\n"));
  } else {
    DEBUG ((DEBUG_ERROR, "Invalid Notification Service UUID\n"));
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
  NotifService        *Service  = NULL;
  UINT8               Uuid[16]  = { 0 };
  NotificationStatus  ReturnVal = NOTIFICATION_STATUS_INVALID_PARAMETER;

  /* Extract the UUID from the message */
  NotificationServiceExtractUuid (Request, Uuid);

  /* Attempt to find the UUID within our list */
  for (UINT8 i = 0; i < NOTIFICATION_MAX_SERVICES; i++) {
    if (!CompareMem (Uuid, NotificationServices[i].ServiceUuid, sizeof (Uuid))) {
      Service = &NotificationServices[i];
      break;
    }
  }

  /* Check for a valid UUID and validate the input parameters */
  if ((Service != NULL) && (ValidParameters (TRUE, Request, Service))) {
    ReturnVal = UpdateServiceBits (TRUE, Request, Service);
  } else if (Service != NULL) {
    DEBUG ((DEBUG_ERROR, "Invalid Parameters\n"));
  } else {
    DEBUG ((DEBUG_ERROR, "Invalid Notification Service UUID\n"));
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
  /* Nothing to Init */
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
  /* Validate the input parameters before attempting to dereference or pass them along */
  if ((Request == NULL) || (Response == NULL)) {
    return;
  }

  UINT64  Opcode = Request->Arg0;

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
  UINT16  Id,
  UINT8   *ServiceUuid,
  UINT32  Flag
  )
{
  /* Validate the incoming function parameters */
  if (ServiceUuid == NULL) {
    return NOTIFICATION_STATUS_INVALID_PARAMETER;
  }

  NotifService        *Service  = NULL;
  NotificationStatus  ReturnVal = NOTIFICATION_STATUS_INVALID_PARAMETER;

  /* Attempt to find the UUID within our list */
  for (UINT8 i = 0; i < NOTIFICATION_MAX_SERVICES; i++) {
    if (!CompareMem (ServiceUuid, NotificationServices[i].ServiceUuid, sizeof (NotificationServices[i].ServiceUuid))) {
      Service = &NotificationServices[i];
      break;
    }
  }

  /* UUID was found */
  if (Service != NULL) {
    /* Attempt to find the logical ID within the mapped list */
    for (UINT8 i = 0; i < NOTIFICATION_MAX_IDS; i++) {
      if (Service->ServiceBits[i].IdNum == Id) {
        UINT64      Bitmask = (1 << Service->ServiceBits[i].BitNum);
        EFI_STATUS  Status  = FfaNotificationSet (SourceId, Flag, Bitmask);
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
  /* Validate the incoming function parameters */
  if ((Request == NULL) || (Uuid == NULL)) {
    return;
  }

  UINT64  UuidLo = Request->Arg1;
  UINT64  UuidHi = Request->Arg2;

  /* Copy the upper 8 bytes */
  for (UINT8 i = 0; i < 8; i++) {
    UINT8  UuidHiByte = (UINT8)(UuidHi >> ((7 - i) * 8));
    Uuid[i] = UuidHiByte;
  }

  /* Copy the lower 8 bytes */
  for (UINT8 i = 8; i < 16; i++) {
    UINT8  UuidLoByte = (UINT8)(UuidLo >> ((15 - i) * 8));
    Uuid[i] = UuidLoByte;
  }
}
