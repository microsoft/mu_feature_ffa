// SPDX-License-Identifier: BSD-3-Clause

/*
 * Copyright (c) 2020-2022, Arm Limited and Contributors. All rights reserved.
 * Copyright (c), Microsoft Corporation.
 */

#include <Uefi.h>
#include <IndustryStandard/ArmFfaSvc.h>
#include <Library/DebugLib.h>
#include <Library/ArmFfaLib.h>
#include <Library/ArmFfaLibEx.h>

#define INVALID_SOURCE_ID  0xFFFF

STATIC UINT16  mPartitionId = INVALID_SOURCE_ID;

STATIC
EFI_STATUS
FfaGetPartitionId (
  VOID
  )
{
  EFI_STATUS  Status;
  UINT16      PartitionId;

  if (mPartitionId != INVALID_SOURCE_ID) {
    return EFI_SUCCESS;
  }

  Status = ArmFfaLibPartitionIdGet (&PartitionId);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get partition ID: %r\n", __func__, Status));
    return Status;
  }

  mPartitionId = PartitionId;
  return EFI_SUCCESS;
}

// Match ArmFfaLib's internal FfaArgsToEfiStatus helper.
STATIC
EFI_STATUS
FfaGetResponseStatus (
  IN CONST ARM_FFA_ARGS  *FfaArgs
  )
{
  UINT32  FfaStatus;

  if (FfaArgs == NULL) {
    FfaStatus = ARM_FFA_RET_INVALID_PARAMETERS;
  } else if (IS_FID_FFA_ERROR (FfaArgs->Arg0)) {
    FfaStatus = FfaArgs->Arg2;
  } else if (FfaArgs->Arg0 == ARM_FFA_RET_NOT_SUPPORTED) {
    FfaStatus = ARM_FFA_RET_NOT_SUPPORTED;
  } else if ((FfaArgs->Arg0 == ARM_FID_FFA_INTERRUPT) || (FfaArgs->Arg0 == ARM_FID_FFA_YIELD)) {
    FfaStatus = ARM_FFA_RET_INTERRUPTED;
  } else {
    FfaStatus = ARM_FFA_RET_SUCCESS;
  }

  return FfaStatusToEfiStatus (FfaStatus);
}

EFI_STATUS
EFIAPI
FfaNsResInfoGet (
  IN UINT16   TargetId,
  IN UINT64   Flags,
  OUT UINT32  *WrittenSize,
  OUT UINT32  *RemainingSize
  )
{
  EFI_STATUS    Status;
  ARM_FFA_ARGS  FfaArgs = { 0 };

  if ((WrittenSize == NULL) || (RemainingSize == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: Size output is NULL\n", __func__));
    return EFI_INVALID_PARAMETER;
  }

  FfaArgs.Arg0 = ARM_FID_FFA_NS_RES_INFO_GET;
  FfaArgs.Arg1 = TargetId;
  FfaArgs.Arg2 = Flags;

  ArmCallFfa (&FfaArgs);

  Status = FfaGetResponseStatus (&FfaArgs);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  *WrittenSize   = (UINT32)((UINT64)FfaArgs.Arg2 >> 32);
  *RemainingSize = (UINT32)FfaArgs.Arg2;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
FfaNotificationSet (
  IN UINT16  DestinationId,
  IN UINT64  Flags,
  IN UINT64  NotificationBitmap
  )
{
  EFI_STATUS    Status;
  ARM_FFA_ARGS  FfaArgs = { 0 };

  Status = FfaGetPartitionId ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  FfaArgs.Arg0 = ARM_FID_FFA_NOTIFICATION_SET;
  FfaArgs.Arg1 = PACK_PARTITION_ID_INFO ((UINT32)mPartitionId, DestinationId);
  FfaArgs.Arg2 = Flags;
  FfaArgs.Arg3 = (UINT32)NotificationBitmap;
  FfaArgs.Arg4 = (UINT32)(NotificationBitmap >> 32);

  ArmCallFfa (&FfaArgs);

  return FfaGetResponseStatus (&FfaArgs);
}

EFI_STATUS
EFIAPI
FfaNotificationGet (
  IN UINT16   VCpuId,
  IN UINT64   Flags,
  OUT UINT64  *NotificationBitmap
  )
{
  EFI_STATUS    Status;
  ARM_FFA_ARGS  FfaArgs = { 0 };

  if (NotificationBitmap == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: NotificationBitmap is NULL\n", __func__));
    return EFI_INVALID_PARAMETER;
  }

  Status = FfaGetPartitionId ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  FfaArgs.Arg0 = ARM_FID_FFA_NOTIFICATION_GET;
  FfaArgs.Arg1 = PACK_PARTITION_ID_INFO ((UINT32)VCpuId, mPartitionId);
  FfaArgs.Arg2 = Flags;

  ArmCallFfa (&FfaArgs);

  Status = FfaGetResponseStatus (&FfaArgs);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  switch (Flags) {
    case ARM_FFA_NOTIFICATION_FLAG_BITMAP_SP:
      *NotificationBitmap = ((UINT64)(UINT32)FfaArgs.Arg3 << 32) | (UINT32)FfaArgs.Arg2;
      break;
    case ARM_FFA_NOTIFICATION_FLAG_BITMAP_VM:
      *NotificationBitmap = ((UINT64)(UINT32)FfaArgs.Arg5 << 32) | (UINT32)FfaArgs.Arg4;
      break;
    case ARM_FFA_NOTIFICATION_FLAG_BITMAP_HYP:
      *NotificationBitmap = ((UINT64)(UINT32)FfaArgs.Arg7 << 32) | (UINT32)FfaArgs.Arg6;
      break;
    default:
      return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
FfaNotificationBitmapCreate (
  IN UINT16  VCpuCount
  )
{
  EFI_STATUS    Status;
  ARM_FFA_ARGS  FfaArgs = { 0 };

  Status = FfaGetPartitionId ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  FfaArgs.Arg0 = ARM_FID_FFA_NOTIFICATION_BITMAP_CREATE;
  FfaArgs.Arg1 = mPartitionId;
  FfaArgs.Arg2 = VCpuCount;

  ArmCallFfa (&FfaArgs);
  return FfaGetResponseStatus (&FfaArgs);
}

EFI_STATUS
EFIAPI
FfaNotificationBitmapDestroy (
  VOID
  )
{
  EFI_STATUS    Status;
  ARM_FFA_ARGS  FfaArgs = { 0 };

  Status = FfaGetPartitionId ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  FfaArgs.Arg0 = ARM_FID_FFA_NOTIFICATION_BITMAP_DESTROY;
  FfaArgs.Arg1 = mPartitionId;

  ArmCallFfa (&FfaArgs);
  return FfaGetResponseStatus (&FfaArgs);
}

EFI_STATUS
EFIAPI
FfaNotificationBind (
  IN UINT16  DestinationId,
  IN UINT32  Flags,
  IN UINT64  NotificationBitmap
  )
{
  EFI_STATUS    Status;
  ARM_FFA_ARGS  FfaArgs = { 0 };

  Status = FfaGetPartitionId ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  FfaArgs.Arg0 = ARM_FID_FFA_NOTIFICATION_BIND;
  FfaArgs.Arg1 = PACK_PARTITION_ID_INFO ((UINT32)DestinationId, mPartitionId);
  FfaArgs.Arg2 = Flags;
  FfaArgs.Arg3 = (UINT32)NotificationBitmap;
  FfaArgs.Arg4 = (UINT32)(NotificationBitmap >> 32);

  ArmCallFfa (&FfaArgs);
  return FfaGetResponseStatus (&FfaArgs);
}

EFI_STATUS
EFIAPI
FfaNotificationUnbind (
  IN UINT16  DestinationId,
  IN UINT64  NotificationBitmap
  )
{
  EFI_STATUS    Status;
  ARM_FFA_ARGS  FfaArgs = { 0 };

  Status = FfaGetPartitionId ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  FfaArgs.Arg0 = ARM_FID_FFA_NOTIFICATION_UNBIND;
  FfaArgs.Arg1 = PACK_PARTITION_ID_INFO ((UINT32)DestinationId, mPartitionId);
  FfaArgs.Arg3 = (UINT32)NotificationBitmap;
  FfaArgs.Arg4 = (UINT32)(NotificationBitmap >> 32);

  ArmCallFfa (&FfaArgs);
  return FfaGetResponseStatus (&FfaArgs);
}
