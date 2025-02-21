/** @file
  Definitions for the Notification Service

  Copyright (c), Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef NOTIFICATION_SERVICE_LIB_H_
#define NOTIFICATION_SERVICE_LIB_H_

#include <Base.h>
#include <IndustryStandard/ArmFfaPartInfo.h>
#include <Library/ArmSvcLib.h>
#include <Library/ArmFfaLibEx.h>

typedef INT32 NotificationStatus;

/**
  Initializes the Notification service

**/
VOID
NotificationServiceInit (
  VOID
  );

/**
  Deinitializes the Notification service

**/
VOID
NotificationServiceDeInit (
  VOID
  );

/**
  Handler for Notification service commands

  @param  Request   The incoming message
  @param  Response  The outgoing message

**/
VOID
NotificationServiceHandle (
  DIRECT_MSG_ARGS_EX  *Request,
  DIRECT_MSG_ARGS_EX  *Response
  );

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
  );

/**
  Extracts the UUID from the message arguments

  @param  Request   The incoming message to extract the UUID from
  @param  Uuid      The UUID to populate

**/
VOID
NotificationServiceExtractUuid (
  DIRECT_MSG_ARGS_EX  *Request,
  UINT8               *Uuid
  );

#endif /* NOTIFICATION_SERVICE_LIB_H_ */
