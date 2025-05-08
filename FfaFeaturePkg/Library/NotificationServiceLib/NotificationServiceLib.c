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
#define NOTIFICATION_MAX_MAPPINGS  (64)
#define NOTIFICATION_NOT_FOUND     (-1)

#define MESSAGE_INFO_DIR_RESP  (0x100)
#define MESSAGE_INFO_ID_MASK   (0x03)

#define MAPPING_MIN  (0x01)
#define MAPPING_MAX  (0x07)

/* Notification Service Structures */
typedef struct {
  UINT32     Cookie;  // SW defined value
  UINT16     Id;      // Global bitmask value
  BOOLEAN    PerVcpu; // Notification flag
  UINT16     SourceId;
  BOOLEAN    InUse;
} NotifInfo;

typedef struct {
  UINT8        ServiceUuid[16];
  NotifInfo    ServiceInfo[NOTIFICATION_MAX_MAPPINGS];
  BOOLEAN      InUse;
} NotifService;

/* Notification Service Variables */
STATIC UINT64        GlobalBitmask;
STATIC NotifService  NotificationServices[NOTIFICATION_MAX_SERVICES];

/**
  Checks if the cookie passed in matches one stored within the service structure

  @param  Cookie   The cookie value to search for
  @param  Service  The service to search for the given ID

  @return The index of the cookie if found, otherwise -1 (NOTIFICATION_NOT_FOUND)

**/
STATIC
INT8
IsMatchingCookie (
  UINT32        Cookie,
  NotifService  *Service
  )
{
  UINT8  Index;

  /* Validate the incoming function parameters */
  if (Service == NULL) {
    return NOTIFICATION_NOT_FOUND;
  }

  for (Index = 0; Index < NOTIFICATION_MAX_MAPPINGS; Index++) {
    if (Service->ServiceInfo[Index].InUse && (Service->ServiceInfo[Index].Cookie == Cookie)) {
      return Index;
    }
  }

  return NOTIFICATION_NOT_FOUND;
}

/**
  Adds or removes service bit information to the local notification services struct array

  @param  Unregister  Whether or not we are adding or removing bit information
  @param  Request     The incoming message containing the bit information
  @param  Service     The service we are updating bit information for

  @retval NOTIFICATION_STATUS_SUCCESS           Success
  @retval NOTIFICATION_STATUS_INVALID_PARAMETER Invalid parameter

**/
STATIC
NotificationStatus
UpdateServiceInfo (
  BOOLEAN             Unregister,
  DIRECT_MSG_ARGS_EX  *Request,
  NotifService        *Service
  )
{
  NotificationStatus   ReturnVal;
  INT8                 FoundIndex;
  UINT8                ReqNumMappings;
  NotificationMapping  *ReqMappings;
  UINT8                ReqMappingIndex;
  UINT16               MappingId;
  UINT32               Cookie;
  UINT8                PerVcpu;
  UINT8                EmptyIndex;
  BOOLEAN              EmptyFound;
  NotifService         TempService;
  UINT64               TempBitmask;

  /* Validate the incoming function parameters */
  if ((Request == NULL) || (Service == NULL)) {
    return NOTIFICATION_STATUS_INVALID_PARAMETER;
  }

  /* Number of cookie/ID pairs = x10. Cookie/ID pairs = x11 (i.e. Arg6/Arg7) */
  ReqNumMappings = Request->Arg6;
  ReqMappings    = (NotificationMapping *)&Request->Arg7;

  /* You must be adding/removing at least one bit and no more than a transaction supports */
  if ((ReqNumMappings < MAPPING_MIN) || (ReqNumMappings > MAPPING_MAX)) {
    DEBUG ((DEBUG_ERROR, "Invalid Number of Mappings: %x\n", ReqNumMappings));
    return NOTIFICATION_STATUS_INVALID_PARAMETER;
  }

  /* Copy the current service structure and global bitmask to the temporaries */
  CopyMem (&TempService, Service, sizeof (NotifService));
  TempBitmask = GlobalBitmask;

  /* Need to go through all of the setup bits and update the structure */
  ReturnVal = NOTIFICATION_STATUS_SUCCESS;
  for (ReqMappingIndex = 0; ReqMappingIndex < ReqNumMappings; ReqMappingIndex++) {
    MappingId  = ReqMappings[ReqMappingIndex].Bits.Id;
    Cookie     = ReqMappings[ReqMappingIndex].Bits.Cookie;
    PerVcpu    = ReqMappings[ReqMappingIndex].Bits.PerVpcu;
    FoundIndex = IsMatchingCookie (Cookie, &TempService);

    /* Check if we are doing an unregister */
    if (Unregister) {
      /* If we can not find the cookie to unregister, it is an error */
      if (FoundIndex == NOTIFICATION_NOT_FOUND) {
        DEBUG ((DEBUG_ERROR, "Invalid Unregister - Cookie: %x Not Registered\n", Cookie));
        ReturnVal = NOTIFICATION_STATUS_INVALID_PARAMETER;
        break;
        /* If the IDs do not match, it is an error */
      } else if (TempService.ServiceInfo[FoundIndex].Id != MappingId) {
        DEBUG ((
          DEBUG_ERROR,
          "Invalid Unregister - ID Registered: %x Mismatch\n",
          TempService.ServiceInfo[FoundIndex].Id
          ));
        ReturnVal = NOTIFICATION_STATUS_INVALID_PARAMETER;
        break;
        /* If the Source IDs do not match, it is an error */
      } else if (TempService.ServiceInfo[FoundIndex].SourceId != Request->SourceId) {
        DEBUG ((
          DEBUG_ERROR,
          "Invalid Unregister - Source ID: %x Mismatch\n",
          TempService.ServiceInfo[FoundIndex].SourceId
          ));
        ReturnVal = NOTIFICATION_STATUS_INVALID_PARAMETER;
        break;
        /* Otherwise, clear the data */
      } else {
        TempService.ServiceInfo[FoundIndex].Cookie   = 0;
        TempService.ServiceInfo[FoundIndex].Id       = 0;
        TempService.ServiceInfo[FoundIndex].InUse    = FALSE;
        TempService.ServiceInfo[FoundIndex].PerVcpu  = FALSE;
        TempService.ServiceInfo[FoundIndex].SourceId = 0;
        TempBitmask                                 &= ~(1 << MappingId);
      }

      /* Otherwise, we are doing a register */
    } else {
      /* If we can find the cookie to register, it is an error */
      if (FoundIndex != NOTIFICATION_NOT_FOUND) {
        DEBUG ((DEBUG_ERROR, "Invalid Register - Cookie: %x Already Registered\n", Cookie));
        ReturnVal = NOTIFICATION_STATUS_INVALID_PARAMETER;
        break;
        /* If the Bitmask bit is set, it is an error */
      } else if (TempBitmask & (1 << MappingId)) {
        DEBUG ((DEBUG_ERROR, "Invalid Register - ID: %x Already Registered\n", MappingId));
        ReturnVal = NOTIFICATION_STATUS_INVALID_PARAMETER;
        break;
        /* Otherwise, set the data */
      } else {
        /* Need to loop through the bits within the structure and find an empty location */
        EmptyFound = FALSE;
        for (EmptyIndex = 0; EmptyIndex < NOTIFICATION_MAX_MAPPINGS; EmptyIndex++) {
          if (!TempService.ServiceInfo[EmptyIndex].InUse) {
            EmptyFound = TRUE;
            break;
          }
        }

        /* If we can not find an empty space, it is an error */
        if (!EmptyFound) {
          DEBUG ((DEBUG_ERROR, "Register Failed - No Memory Available\n"));
          ReturnVal = NOTIFICATION_STATUS_NO_MEM;
          break;
          /* Otherwise, update the data */
        } else {
          TempService.ServiceInfo[EmptyIndex].Cookie   = Cookie;
          TempService.ServiceInfo[EmptyIndex].Id       = MappingId;
          TempService.ServiceInfo[EmptyIndex].InUse    = TRUE;
          TempService.ServiceInfo[EmptyIndex].PerVcpu  = (PerVcpu) ? TRUE : FALSE;
          TempService.ServiceInfo[EmptyIndex].SourceId = Request->SourceId;
          TempBitmask                                 |= (1 << MappingId);
        }
      }
    }

    /* Copy the temporaries back if everything was successful */
    if (ReturnVal == NOTIFICATION_STATUS_SUCCESS) {
      CopyMem (Service, &TempService, sizeof (NotifService));
      GlobalBitmask = TempBitmask;
    }
  }

  return ReturnVal;
}

/**
  Handler for Notification Register command

  @param  Request   The incoming message
  @param  Response  The outgoing message

  @retval NOTIFICATION_STATUS_SUCCESS           Success
  @retval NOTIFICATION_STATUS_INVALID_PARAMETER Invalid parameter
  @retval NOTIFICATION_STATUS_NO_MEM            Out of resources

**/
STATIC
NotificationStatus
RegisterHandler (
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

  /* Extract the UUID from the message x7-x8 (i.e. Arg3-Arg4) */
  NotificationServiceExtractUuid (Request->Arg3, Request->Arg4, Uuid);

  /* Attempt to find the UUID within our list */
  for (Index = 0; Index < NOTIFICATION_MAX_SERVICES; Index++) {
    /* Check if the service is marked as InUse */
    if (NotificationServices[Index].InUse) {
      /* Check if the UUID matches */
      if (!CompareMem (Uuid, NotificationServices[Index].ServiceUuid, sizeof (Uuid))) {
        Service = &NotificationServices[Index];
        break;
      }
    }
  }

  /* The UUID was not found in the list, attempt to find an empty location to add it */
  if (Service == NULL) {
    for (Index = 0; Index < NOTIFICATION_MAX_SERVICES; Index++) {
      if (!NotificationServices[Index].InUse) {
        /* Temporarily point to the empty location */
        Service = &NotificationServices[Index];
        break;
      }
    }
  }

  /* Check for a valid UUID */
  if (Service != NULL) {
    ReturnVal = UpdateServiceInfo (FALSE, Request, Service);
    /* Check if the update was successful and this was a new addition */
    if ((ReturnVal == NOTIFICATION_STATUS_SUCCESS) && (!Service->InUse))
    {
      /* Update the UUID and set the location to InUse */
      CopyMem (Service->ServiceUuid, Uuid, sizeof (Uuid));
      Service->InUse = TRUE;
    }
  } else {
    DEBUG ((DEBUG_ERROR, "Service Register Failed - Error Code: %d\n", ReturnVal));
  }

  /* Update the error code */
  Response->Arg6 = ReturnVal;
  return ReturnVal;
}

/**
  Handler for Notification Unregister command

  @param  Request   The incoming message
  @param  Response  The outgoing message

  @retval NOTIFICATION_STATUS_SUCCESS           Success
  @retval NOTIFICATION_STATUS_INVALID_PARAMETER Invalid parameter

**/
STATIC
NotificationStatus
UnregisterHandler (
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

  /* Extract the UUID from the message x7-x8 (i.e. Arg3-Arg4) */
  NotificationServiceExtractUuid (Request->Arg3, Request->Arg4, Uuid);

  /* Attempt to find the UUID within our list */
  for (Index = 0; Index < NOTIFICATION_MAX_SERVICES; Index++) {
    /* Check if the service is marked as InUse */
    if (NotificationServices[Index].InUse) {
      /* Check if the UUID matches */
      if (!CompareMem (Uuid, NotificationServices[Index].ServiceUuid, sizeof (Uuid))) {
        Service = &NotificationServices[Index];
        break;
      }
    }
  }

  /* Check for a valid UUID */
  if (Service != NULL) {
    ReturnVal = UpdateServiceInfo (TRUE, Request, Service);
  } else {
    DEBUG ((DEBUG_ERROR, "Service Unregister Failed - Error Code: %d\n", ReturnVal));
  }

  /* Update the error code */
  Response->Arg6 = ReturnVal;
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

  /* Initialize Global Bitmask */
  GlobalBitmask = 0;

  /* Initialize the Notification Service structure */
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
  /* Validate the input parameters before attempting to dereference or pass them along */
  if ((Request == NULL) || (Response == NULL)) {
    return;
  }

  /* TODO: Figure out how to set x5-x8 */
  /* Set common response register values */
  Response->Arg1 = Request->Arg1;
  Response->Arg2 = Request->Arg2;
  Response->Arg3 = Request->Arg3;
  Response->Arg4 = Request->Arg4;
  Response->Arg5 = Request->Arg5 | MESSAGE_INFO_DIR_RESP;

  /* Message ID = Bits[0:1] of x9 (i.e. Arg5)*/
  switch (Request->Arg5 & MESSAGE_INFO_ID_MASK) {
    case NOTIFICATION_OPCODE_ADD:
    case NOTIFICATION_OPCODE_REMOVE:
      /* Update the error code */
      Response->Arg6 = NOTIFICATION_STATUS_NOT_SUPPORTED;
      DEBUG ((DEBUG_ERROR, "Add/Remove Unsupported\n"));
      break;

    case NOTIFICATION_OPCODE_MEM_ASSIGN:
    case NOTIFICATION_OPCODE_MEM_UNASSIGN:
      /* Update the error code */
      Response->Arg6 = NOTIFICATION_STATUS_NOT_SUPPORTED;
      DEBUG ((DEBUG_ERROR, "Memory Assign/Unassign Unsupported\n"));
      break;

    case NOTIFICATION_OPCODE_REGISTER:
      RegisterHandler (Request, Response);
      break;

    case NOTIFICATION_OPCODE_UNREGISTER:
      UnregisterHandler (Request, Response);
      break;

    default:
      /* Update the error code */
      Response->Arg6 = NOTIFICATION_STATUS_INVALID_PARAMETER;
      DEBUG ((DEBUG_ERROR, "Invalid Notification Service Opcode\n"));
      break;
  }

  /* If applicable, x10-x17 (i.e. Arg6-Arg13) will be set in the handler function */
}

/**
  Calls NotificationSet on the given ID with the given flag

  @param  Id           The ID to trigger the event on
  @param  ServiceUuid  The service containing the ID to trigger
  @param  Flag         The NotificationSet flag to use

  @retval NOTIFICATION_STATUS_SUCCESS           Success
  @retval NOTIFICATION_STATUS_INVALID_PARAMETER Invalid parameter

**/
NotificationStatus
NotificationServiceIdSet (
  UINT32  Cookie,
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
    /* Check if the service is marked as InUse */
    if (NotificationServices[Index].InUse) {
      /* Check if the UUID matches */
      if (!CompareMem (
             ServiceUuid,
             NotificationServices[Index].ServiceUuid,
             sizeof (NotificationServices[Index].ServiceUuid)
             ))
      {
        Service = &NotificationServices[Index];
        break;
      }
    }
  }

  /* UUID was found */
  if (Service != NULL) {
    /* Attempt to find the cookie within the mapped list */
    for (Index = 0; Index < NOTIFICATION_MAX_MAPPINGS; Index++) {
      if (Service->ServiceInfo[Index].Cookie == Cookie) {
        Bitmask = (1 << Service->ServiceInfo[Index].Id);
        /* TODO: Figure out how we are going to do PerVcpu setup */
        // Flag |= (Service->ServiceInfo[Index].PerVcpu);
        Status = FfaNotificationSet (Service->ServiceInfo[Index].SourceId, Flag, Bitmask);
        if (!EFI_ERROR (Status)) {
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

  @param  UuidLo  The lower bytes of the UUID
  @param  UuidHi  The higher bytes of the UUID
  @param  Uuid    The UUID to populate

**/
VOID
NotificationServiceExtractUuid (
  UINT64  UuidLo,
  UINT64  UuidHi,
  UINT8   *Uuid
  )
{
  UINT8  Index;
  UINT8  UuidHiByte;
  UINT8  UuidLoByte;

  /* Validate the incoming function parameters */
  if (Uuid == NULL) {
    return;
  }

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
