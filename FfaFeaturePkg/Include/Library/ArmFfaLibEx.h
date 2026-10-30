/** @file
  FF-A resource information and notification interfaces.

  Copyright (c) 2020 - 2022, Arm Ltd. All rights reserved.<BR>
  Copyright (c), Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#pragma once

#pragma pack(1)

/**
 * Resource Information Descriptor Header
 * Section 13.13: FFA_NS_RES_INFO_GET - Table 13.61
 */
typedef struct {
  UINT32    AmdSize;
  UINT32    AmdCount;
  UINT32    AmdOffset;
  UINT32    Reserved;
} FFA_RESOURCE_INFO_DESC_HEADER;

/**
 * Address Map Descriptor
 * Section 13.13: FFA_NS_RES_INFO_GET - Table 13.62
 */
typedef struct {
  UINT64    BaseAddress;
  UINT32    PageCount;
  UINT8     Permissions;
  UINT16    EndpointId;
  UINT8     Flags;
} FFA_ADDRESS_MAP_DESC;
#pragma pack()

EFI_STATUS
EFIAPI
FfaNsResInfoGet (
  IN UINT16   TargetId,
  IN UINT64   Flags,
  OUT UINT32  *WrittenSize,
  OUT UINT32  *RemainingSize
  );

EFI_STATUS
EFIAPI
FfaNotificationSet (
  IN UINT16  DestinationId,
  IN UINT64  Flags,
  IN UINT64  NotificationBitmap
  );

EFI_STATUS
EFIAPI
FfaNotificationGet (
  IN UINT16   VCpuId,
  IN UINT64   Flags,
  OUT UINT64  *NotificationBitmap
  );

EFI_STATUS
EFIAPI
FfaNotificationBitmapCreate (
  IN UINT16  VCpuCount
  );

EFI_STATUS
EFIAPI
FfaNotificationBitmapDestroy (
  VOID
  );

EFI_STATUS
EFIAPI
FfaNotificationBind (
  IN UINT16  DestinationId,
  IN UINT32  Flags,
  IN UINT64  NotificationBitmap
  );

EFI_STATUS
EFIAPI
FfaNotificationUnbind (
  IN UINT16  DestinationId,
  IN UINT64  NotificationBitmap
  );
