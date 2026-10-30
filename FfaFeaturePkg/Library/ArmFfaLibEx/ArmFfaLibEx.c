// SPDX-License-Identifier: BSD-3-Clause

/*
 * Copyright (c) 2020-2022, Arm Limited and Contributors. All rights reserved.
 * Copyright (c), Microsoft Corporation.
 */

#include <Uefi.h>
#include <IndustryStandard/ArmFfaSvc.h>
#include <IndustryStandard/ArmFfaPartInfo.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/ArmFfaLib.h>
#include <Library/ArmSvcLib.h>
#include <Library/ArmSmcLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/ArmFfaLibEx.h>
#include <Library/PlatformFfaInterruptLib.h>

#define INVALID_SOURCE_ID  0xFFFF

STATIC UINT16  mPartitionId = INVALID_SOURCE_ID;

/*
 * Unpacks the content of the ffa instruction Response into an ffa_direct_msg structure.
 */
STATIC
VOID
FfaUnpackDirectMessage (
  ARM_FFA_ARGS        *Response,
  DIRECT_MSG_ARGS_EX  *Message
  )
{
  Message->FunctionId    = Response->Arg0;
  Message->SourceId      = (Response->Arg1 >> 16);
  Message->DestinationId = Response->Arg1;

  if ((Message->FunctionId == ARM_FID_FFA_MSG_SEND_DIRECT_REQ_AARCH32) ||
      (Message->FunctionId == ARM_FID_FFA_MSG_SEND_DIRECT_REQ_AARCH64))
  {
    Message->Arg0 = Response->Arg2;
    Message->Arg1 = Response->Arg3;
    Message->Arg2 = Response->Arg4;
    Message->Arg3 = Response->Arg5;
    Message->Arg4 = Response->Arg6;
    Message->Arg5 = Response->Arg7;
  } else {
    CopyMem (&Message->ServiceGuid, &Response->Arg2, sizeof (EFI_GUID));
    ConvertGuidToUuid (&Message->ServiceGuid, &Message->ServiceGuid);
    Message->Arg0  = Response->Arg4;
    Message->Arg1  = Response->Arg5;
    Message->Arg2  = Response->Arg6;
    Message->Arg3  = Response->Arg7;
    Message->Arg4  = Response->Arg8;
    Message->Arg5  = Response->Arg9;
    Message->Arg6  = Response->Arg10;
    Message->Arg7  = Response->Arg11;
    Message->Arg8  = Response->Arg12;
    Message->Arg9  = Response->Arg13;
    Message->Arg10 = Response->Arg14;
    Message->Arg11 = Response->Arg15;
    Message->Arg12 = Response->Arg16;
    Message->Arg13 = Response->Arg17;
  }
}

/*
 * The end of the interrupt handler is indicated by an FFA_MSG_WAIT call.
 */
STATIC
VOID
FfaReturnFromInterrupt (
  ARM_FFA_ARGS  *Result
  )
{
  ARM_FFA_ARGS  Request = { 0 };

  Request.Arg0 = ARM_FID_FFA_WAIT;
  ArmCallFfa (&Request, Result);
}

EFI_STATUS
EFIAPI
FfaMessageWait (
  OUT DIRECT_MSG_ARGS_EX  *Message
  )
{
  ARM_FFA_ARGS  Request = { 0 };
  ARM_FFA_ARGS  Result  = { 0 };

  Request.Arg0 = ARM_FID_FFA_WAIT;

  ArmCallFfa (&Request, &Result);

  while (Result.Arg0 == ARM_FID_FFA_INTERRUPT) {
    SecurePartitionInterruptHandler ((UINT32)Result.Arg2);
    FfaReturnFromInterrupt (&Result);
  }

  if (Result.Arg0 == ARM_FID_FFA_ERROR) {
    return FfaStatusToEfiStatus (Result.Arg2);
  } else if ((Result.Arg0 == ARM_FID_FFA_MSG_SEND_DIRECT_REQ_AARCH32) ||
             (Result.Arg0 == ARM_FID_FFA_MSG_SEND_DIRECT_REQ_AARCH64) ||
             (Result.Arg0 == ARM_FID_FFA_MSG_SEND_DIRECT_REQ2))
  {
    FfaUnpackDirectMessage (&Result, Message);
  } else {
    ASSERT (Result.Arg0 == ARM_FID_FFA_SUCCESS_AARCH32);
    *Message = (DIRECT_MSG_ARGS_EX) {
      .FunctionId = Result.Arg0
    };
  }

  return EFI_SUCCESS;
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
  ARM_FFA_ARGS  Request = { 0 };
  ARM_FFA_ARGS  Result  = { 0 };

  if ((WrittenSize == NULL) || (RemainingSize == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Request.Arg0 = ARM_FID_FFA_NS_RES_INFO_GET;
  Request.Arg1 = TargetId;
  Request.Arg2 = Flags;

  ArmCallFfa (&Request, &Result);

  if (Result.Arg0 == ARM_FID_FFA_ERROR) {
    return FfaStatusToEfiStatus (Result.Arg2);
  }

  *WrittenSize   = Result.Arg2 >> 32;
  *RemainingSize = Result.Arg2;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
FfaMemDonate (
  UINT32  TotalLength,
  UINT32  FragmentLength,
  VOID    *BufferAddr,
  UINT32  PageCount,
  UINT64  *Handle
  )
{
  ARM_FFA_ARGS  Request = { 0 };
  ARM_FFA_ARGS  Result  = { 0 };

  Request.Arg0 = (BufferAddr) ? ARM_FID_FFA_MEM_DONATE_AARCH64 : ARM_FID_FFA_MEM_DONATE_AARCH32;
  Request.Arg1 = TotalLength;
  Request.Arg2 = FragmentLength;
  Request.Arg3 = (UINTN)BufferAddr;
  Request.Arg4 = PageCount;

  ArmCallFfa (&Request, &Result);

  if (Result.Arg0 == ARM_FID_FFA_ERROR) {
    *Handle = 0U;
    return FfaStatusToEfiStatus (Result.Arg2);
  }

  /*
   * There are no 64-bit parameters returned with FFA_SUCCESS, the SPMC
   * will use the default 32-bit version.
   */
  ASSERT (Result.Arg0 == ARM_FID_FFA_SUCCESS_AARCH32);
  *Handle = ((UINT64)Result.Arg3 << 32) | Result.Arg2;
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
  ARM_FFA_ARGS  Request = { 0 };
  ARM_FFA_ARGS  Result  = { 0 };

  if (mPartitionId == INVALID_SOURCE_ID) {
    ArmFfaLibPartitionIdGet (&mPartitionId);
  }

  Request.Arg0 = ARM_FID_FFA_NOTIFICATION_SET;
  Request.Arg1 = ((UINT32)mPartitionId << 16) | DestinationId;
  Request.Arg2 = Flags;
  Request.Arg3 = (UINT32)NotificationBitmap;
  Request.Arg4 = (UINT32)(NotificationBitmap >> 32);

  ArmCallFfa (&Request, &Result);

  if (Result.Arg0 == ARM_FID_FFA_ERROR) {
    return FfaStatusToEfiStatus (Result.Arg2);
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
FfaNotificationGet (
  IN UINT16  VCpuId,
  IN UINT64  Flags,
  IN UINT64  *NotificationBitmap
  )
{
  ARM_FFA_ARGS  Request = { 0 };
  ARM_FFA_ARGS  Result  = { 0 };

  if (mPartitionId == INVALID_SOURCE_ID) {
    ArmFfaLibPartitionIdGet (&mPartitionId);
  }

  Request.Arg0 = ARM_FID_FFA_NOTIFICATION_GET;
  Request.Arg1 = ((UINT32)VCpuId << 16) | mPartitionId;
  Request.Arg2 = Flags;

  ArmCallFfa (&Request, &Result);

  if (Result.Arg0 == ARM_FID_FFA_ERROR) {
    return FfaStatusToEfiStatus (Result.Arg2);
  }

  switch (Flags) {
    case ARM_FFA_NOTIFICATION_FLAG_BITMAP_SP:
      *NotificationBitmap = ((UINT64)Result.Arg3 << 32) | Result.Arg2;
      break;
    case ARM_FFA_NOTIFICATION_FLAG_BITMAP_VM:
      *NotificationBitmap = ((UINT64)Result.Arg5 << 32) | Result.Arg4;
      break;
    case ARM_FFA_NOTIFICATION_FLAG_BITMAP_HYP:
      *NotificationBitmap = ((UINT64)Result.Arg7 << 32) | Result.Arg6;
      break;
    default:
      return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}
