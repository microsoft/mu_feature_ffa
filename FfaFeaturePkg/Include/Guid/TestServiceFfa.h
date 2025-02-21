/** @file
  Provides function interfaces to communicate with Test service through FF-A.

  Copyright (c), Microsoft Corporation.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef TEST_SERVICE_FFA_H_
#define TEST_SERVICE_FFA_H_

#define TEST_SERVICE_UUID \
  { 0xe0fad9b3, 0x7f5c, 0x42c5, { 0xb2, 0xee, 0xb7, 0xa8, 0x23, 0x13, 0xcd, 0xb2 } }

#define TEST_STATUS_SUCCESS            (0)
#define TEST_STATUS_GENERIC_ERROR      (-1)
#define TEST_STATUS_INVALID_PARAMETER  (-2)

#define TEST_OPCODE_BASE               (0xDEF0)
#define TEST_OPCODE_TEST_NOTIFICATION  (TEST_OPCODE_BASE + 0x01)

extern EFI_GUID  gEfiTestServiceFfaGuid;

#endif /* TEST_SERVICE_FFA_H_ */
