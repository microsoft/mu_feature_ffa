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
