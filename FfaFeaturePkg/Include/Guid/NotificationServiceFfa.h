/** @file
  Provides function interfaces to communicate with Notification service through FF-A.

  Copyright (c), Microsoft Corporation.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef NOTIFICATION_SERVICE_FFA_H_
#define NOTIFICATION_SERVICE_FFA_H_

#define NOTIFICATION_SERVICE_UUID \
  { 0xb510b3a3, 0x59f6, 0x4054, { 0xba, 0x7a, 0xff, 0x2e, 0xb1, 0xea, 0xc7, 0x65 } }

#define NOTIFICATION_STATUS_SUCCESS            (0)
#define NOTIFICATION_STATUS_GENERIC_ERROR      (-1)
#define NOTIFICATION_STATUS_INVALID_PARAMETER  (-2)

#define NOTIFICATION_OPCODE_BASE     (0)
#define NOTIFICATION_OPCODE_QUERY    (NOTIFICATION_OPCODE_BASE + 0)
#define NOTIFICATION_OPCODE_SETUP    (NOTIFICATION_OPCODE_BASE + 1)
#define NOTIFICATION_OPCODE_DESTROY  (NOTIFICATION_OPCODE_BASE + 2)

extern EFI_GUID  gEfiNotificationServiceFfaGuid;

#endif /* NOTIFICATION_SERVICE_FFA_H_ */
