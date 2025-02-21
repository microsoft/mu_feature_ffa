/** @file
  Definitions for the Test Service

  Copyright (c), Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef TEST_SERVICE_LIB_H_
#define TEST_SERVICE_LIB_H_

#include <Base.h>
#include <IndustryStandard/ArmFfaPartInfo.h>
#include <Library/ArmSvcLib.h>
#include <Library/ArmFfaLibEx.h>

typedef INT32 TestStatus;

#define TEST_STATUS_SUCCESS            ((TestStatus)0)
#define TEST_STATUS_GENERIC_ERROR      ((TestStatus)-1)
#define TEST_STATUS_INVALID_PARAMETER  ((TestStatus)-2)

#define TEST_OPCODE_BASE               (0xDEF0)
#define TEST_OPCODE_TEST_NOTIFICATION  (TEST_OPCODE_BASE + 0x01)

#define TEST_SERVICE_UUID \
{ \
  0xe0fad9b3, 0x7f5c, 0x42c5, { 0xb2, 0xee, 0xb7, 0xa8, 0x23, 0x13, 0xcd, 0xb2 } \
}

/**
  Initializes the Test service

**/
VOID
TestServiceInit (
  VOID
  );

/**
  Deinitializes the Test service

**/
VOID
TestServiceDeInit (
  VOID
  );

/**
  Handler for Test service commands

  @param  Request   The incoming message
  @param  Response  The outgoing message

**/
VOID
TestServiceHandle (
  DIRECT_MSG_ARGS_EX  *Request,
  DIRECT_MSG_ARGS_EX  *Response
  );

#endif /* TEST_SERVICE_LIB_H_ */
