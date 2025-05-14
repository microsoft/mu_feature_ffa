/** @file
  Provides function interfaces to communicate with Notification service through FF-A.

  Copyright (c), Microsoft Corporation.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef NOTIFICATION_SERVICE_FFA_H_
#define NOTIFICATION_SERVICE_FFA_H_

#define NOTIFICATION_SERVICE_UUID \
  { 0xe474d87e, 0x5731, 0x4044, { 0xa7, 0x27, 0xcb, 0x3e, 0x8c, 0xf3, 0xc8, 0xdf } }

#define NOTIFICATION_STATUS_SUCCESS            (0)
#define NOTIFICATION_STATUS_NOT_SUPPORTED      (-1)
#define NOTIFICATION_STATUS_INVALID_PARAMETER  (-2)
#define NOTIFICATION_STATUS_NO_MEM             (-3)

#define NOTIFICATION_OPCODE_BASE          (0)
#define NOTIFICATION_OPCODE_ADD           (NOTIFICATION_OPCODE_BASE + 0)
#define NOTIFICATION_OPCODE_REMOVE        (NOTIFICATION_OPCODE_BASE + 1)
#define NOTIFICATION_OPCODE_REGISTER      (NOTIFICATION_OPCODE_BASE + 2)
#define NOTIFICATION_OPCODE_UNREGISTER    (NOTIFICATION_OPCODE_BASE + 3)
#define NOTIFICATION_OPCODE_MEM_ASSIGN    (NOTIFICATION_OPCODE_BASE + 4)
#define NOTIFICATION_OPCODE_MEM_UNASSIGN  (NOTIFICATION_OPCODE_BASE + 5)

#pragma pack (1)
typedef union {
  struct {
    UINTN    PerVcpu  : 1;
    UINTN    Reserved : 22;
    UINTN    Id       : 9;
    UINTN    Cookie   : 32;
  } Bits;
  UINTN    Uint64;
} NotificationMapping;
#pragma pack ()

extern EFI_GUID  gEfiNotificationServiceFfaGuid;

#endif /* NOTIFICATION_SERVICE_FFA_H_ */
