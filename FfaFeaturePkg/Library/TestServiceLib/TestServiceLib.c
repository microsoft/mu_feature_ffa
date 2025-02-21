/** @file
  Implementation for the Test Service

  Copyright (c), Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/TestServiceLib.h>
#include <Library/NotificationServiceLib.h>
#include <Guid/TestServiceFfa.h>
#include <Guid/NotificationServiceFfa.h>

/* Test Service Defines */
#define DELAYED_SRI_BIT_POS  (1)

/**
  Handler for Test Notification command

  @param  Request   The incoming message
  @param  Response  The outgoing message

  @retval TEST_STATUS_SUCCESS           Success
  @retval TEST_STATUS_INVALID_PARAMETER Invalid Parameter

**/
STATIC
TestStatus
TestNotificationHandler (
  DIRECT_MSG_ARGS_EX  *Request,
  DIRECT_MSG_ARGS_EX  *Response
  )
{
  UINT8       Uuid[16]  = { 0 };
  TestStatus  ReturnVal = TEST_STATUS_INVALID_PARAMETER;

  /* Extract the UUID from the message */
  NotificationServiceExtractUuid (Request, Uuid);

  /* Set the notification set flag to be a delayed SRI */
  UINT32  Flag = (1 << DELAYED_SRI_BIT_POS);

  UINT16              LogicalId = Request->Arg3;
  NotificationStatus  Status    = NotificationServiceIdSet (LogicalId, Uuid, Flag);

  /* Check for a valid UUID and validate the input parameters */
  if (Status == NOTIFICATION_STATUS_SUCCESS) {
    ReturnVal = TEST_STATUS_SUCCESS;
  } else {
    DEBUG ((DEBUG_ERROR, "Test Notification Handler Failed\n"));
  }

  Response->Arg0 = ReturnVal;
  return ReturnVal;
}

/**
  Initializes the Test service

**/
VOID
TestServiceInit (
  VOID
  )
{
  /* Nothing to Init */
}

/**
  Deinitializes the Test service

**/
VOID
TestServiceDeInit (
  VOID
  )
{
  /* Nothing to Deinit */
}

/**
  Handler for Test service commands

  @param  Request   The incoming message
  @param  Response  The outgoing message

**/
VOID
TestServiceHandle (
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
    case TEST_OPCODE_TEST_NOTIFICATION:
      TestNotificationHandler (Request, Response);
      break;

    default:
      Response->Arg0 = TEST_STATUS_INVALID_PARAMETER;
      DEBUG ((DEBUG_ERROR, "Invalid Test Service Opcode\n"));
      break;
  }
}
