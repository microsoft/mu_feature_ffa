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

extern UINT16  gPartId;

/**
  This function is used to prepare a GUID for FF-A.

  The FF-A expects a GUID to be in a specific format. This function
  manipulates the input Guid to be understood by the FF-A.

  Note: This function is symmetric, i.e., calling it twice will return the
  original GUID. Thus, it can be used to prepare and restore the GUID.

  @param  Guid - Supplies the pointer for original GUID. This is the one you
  get from VS GUID creator.

  @retval None.
**/
VOID
FfaPrepareGuid (
  IN OUT EFI_GUID  *Guid
  )
{
  UINT32  TempData[4];

  if (Guid == NULL) {
    return;
  }

  //
  // Swap Data2 and Data3 of the input GUID.
  //

  Guid->Data2 += Guid->Data3;
  Guid->Data3  = Guid->Data2 - Guid->Data3;
  Guid->Data2  = Guid->Data2 - Guid->Data3;
  CopyMem (TempData, Guid, sizeof (EFI_GUID));

  //
  // Swap the bytes for TempData[2] and TempData[3].
  //

  TempData[2] = SwapBytes32 (TempData[2]);
  TempData[3] = SwapBytes32 (TempData[3]);
  CopyMem (Guid, TempData, sizeof (EFI_GUID));
}

STATIC
VOID
ArmCallSxc (
  ARM_SXC_ARGS  *Request,
  ARM_SXC_ARGS  *Response
  )
{
  ARM_SXC_ARGS  LocalParams;

  CopyMem (&LocalParams, Request, sizeof (ARM_SXC_ARGS));

  /*
   * The ArmCallSxc function is a wrapper around the ffa_params structure
   * which checks the current execution level and calls the appropriate
   * conduit.
   */
  if (PcdGetBool (PcdFfaLibConduitSmc) == 1) {
    ArmCallSmc ((ARM_SMC_ARGS *)&LocalParams);
  } else {
    ArmCallSvc ((ARM_SVC_ARGS *)&LocalParams);
  }

  CopyMem (Response, &LocalParams, sizeof (ARM_SXC_ARGS));
}

/*
 * Unpacks the content of the ffa instruction Response into an ffa_direct_msg structure.
 */
STATIC
VOID
FfaUnpackDirectMessage (
  ARM_SXC_ARGS        *Response,
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
    FfaPrepareGuid (&Message->ServiceGuid);
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
 * Packs the content of the ffa_direct_msg into a Request message.
 */
STATIC
VOID
FfaPackDirectMessage (
  OUT ARM_SXC_ARGS       *Request,
  IN DIRECT_MSG_ARGS_EX  *Message
  )
{
  EFI_GUID  ServiceGuid;

  Request->Arg0 = Message->FunctionId;

  /* NOTE: There is a DIRECT_RESP define in ffa_api_defines.h, this may need to be updated
   *       in the future if the defines differ, for now they are identical. */
  Request->Arg1 = ((UINT32)Message->SourceId << 16) | Message->DestinationId;

  if ((Message->FunctionId == ARM_FID_FFA_MSG_SEND_DIRECT_REQ_AARCH32) ||
      (Message->FunctionId == ARM_FID_FFA_MSG_SEND_DIRECT_REQ_AARCH64) ||
      (Message->FunctionId == ARM_FID_FFA_MSG_SEND_DIRECT_RESP_AARCH32) ||
      (Message->FunctionId == ARM_FID_FFA_MSG_SEND_DIRECT_RESP_AARCH64))
  {
    Request->Arg2 = Message->Arg0;
    Request->Arg3 = Message->Arg1;
    Request->Arg4 = Message->Arg2;
    Request->Arg5 = Message->Arg3;
    Request->Arg6 = Message->Arg4;
    Request->Arg7 = Message->Arg5;
  } else {
    CopyMem (&ServiceGuid, &Message->ServiceGuid, sizeof (EFI_GUID));
    FfaPrepareGuid (&ServiceGuid);
    CopyMem (&Request->Arg2, &ServiceGuid, sizeof (EFI_GUID));
    Request->Arg4  = Message->Arg0;
    Request->Arg5  = Message->Arg1;
    Request->Arg6  = Message->Arg2;
    Request->Arg7  = Message->Arg3;
    Request->Arg8  = Message->Arg4;
    Request->Arg9  = Message->Arg5;
    Request->Arg10 = Message->Arg6;
    Request->Arg11 = Message->Arg7;
    Request->Arg12 = Message->Arg8;
    Request->Arg13 = Message->Arg9;
    Request->Arg14 = Message->Arg10;
    Request->Arg15 = Message->Arg11;
    Request->Arg16 = Message->Arg12;
    Request->Arg17 = Message->Arg13;
  }
}

/*
 * The end of the interrupt handler is indicated by an FFA_MSG_WAIT call.
 */
STATIC
VOID
FfaReturnFromInterrupt (
  ARM_SXC_ARGS  *Result
  )
{
  ARM_SXC_ARGS  Request = { 0 };

  Request.Arg0 = ARM_FID_FFA_WAIT;
  ArmCallSxc (&Request, Result);
}

EFI_STATUS
EFIAPI
FfaMessageWait (
  OUT DIRECT_MSG_ARGS_EX  *Message
  )
{
  ARM_SXC_ARGS  Request = { 0 };
  ARM_SXC_ARGS  Result  = { 0 };

  Request.Arg0 = ARM_FID_FFA_WAIT;

  ArmCallSxc (&Request, &Result);

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
FfaMessageSendDirectReq2 (
  IN      UINT16              DestPartId,
  IN      EFI_GUID            *ServiceGuid OPTIONAL,
  IN OUT  DIRECT_MSG_ARGS_EX  *ImpDefArgs
  )
{
  ARM_SXC_ARGS  InputArgs = { 0 };
  ARM_SXC_ARGS  Result    = { 0 };

  ImpDefArgs->FunctionId    = ARM_FID_FFA_MSG_SEND_DIRECT_REQ2;
  ImpDefArgs->SourceId      = gPartId;
  ImpDefArgs->DestinationId = DestPartId;
  if (ServiceGuid != NULL) {
    CopyMem (&(ImpDefArgs->ServiceGuid), ServiceGuid, sizeof (EFI_GUID));
  } else {
    ZeroMem (&(ImpDefArgs->ServiceGuid), sizeof (EFI_GUID));
  }

  FfaPackDirectMessage (&InputArgs, ImpDefArgs);

  ArmCallSxc (&InputArgs, &Result);

  while (Result.Arg0 == ARM_FID_FFA_INTERRUPT) {
    SecurePartitionInterruptHandler ((UINT32)Result.Arg2);
    FfaReturnFromInterrupt (&Result);
  }

  if (Result.Arg0 == ARM_FID_FFA_ERROR) {
    return FfaStatusToEfiStatus (Result.Arg2);
  } else if (Result.Arg0 == ARM_FID_FFA_MSG_SEND_DIRECT_RESP2) {
    FfaUnpackDirectMessage (&Result, ImpDefArgs);
  } else {
    ASSERT (Result.Arg0 == ARM_FID_FFA_SUCCESS_AARCH32);
    *ImpDefArgs = (DIRECT_MSG_ARGS_EX) {
      .FunctionId = Result.Arg0
    };
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
FfaMessageSendDirectResp (
  IN UINT32               FunctionId,
  IN DIRECT_MSG_ARGS_EX   *Request,
  OUT DIRECT_MSG_ARGS_EX  *Response
  )
{
  ARM_SXC_ARGS  InputArgs = { 0 };
  ARM_SXC_ARGS  Result    = { 0 };

  Request->FunctionId = FunctionId;
  FfaPackDirectMessage (&InputArgs, Request);

  ArmCallSxc (&InputArgs, &Result);

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
    FfaUnpackDirectMessage (&Result, Response);
  } else {
    ASSERT (Result.Arg0 == ARM_FID_FFA_SUCCESS_AARCH32);
    *Response = (DIRECT_MSG_ARGS_EX) {
      .FunctionId = Result.Arg0
    };
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
FfaMessageSendDirectResp32 (
  IN DIRECT_MSG_ARGS_EX   *Request,
  OUT DIRECT_MSG_ARGS_EX  *Response
  )
{
  return FfaMessageSendDirectResp (
           ARM_FID_FFA_MSG_SEND_DIRECT_RESP_AARCH32,
           Request,
           Response
           );
}

EFI_STATUS
EFIAPI
FfaMessageSendDirectResp64 (
  IN DIRECT_MSG_ARGS_EX   *Request,
  OUT DIRECT_MSG_ARGS_EX  *Response
  )
{
  return FfaMessageSendDirectResp (
           ARM_FID_FFA_MSG_SEND_DIRECT_RESP_AARCH64,
           Request,
           Response
           );
}

EFI_STATUS
EFIAPI
FfaMessageSendDirectResp2 (
  IN DIRECT_MSG_ARGS_EX   *Request,
  OUT DIRECT_MSG_ARGS_EX  *Response
  )
{
  return FfaMessageSendDirectResp (
           ARM_FID_FFA_MSG_SEND_DIRECT_RESP2,
           Request,
           Response
           );
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
  ARM_SXC_ARGS  Request = { 0 };
  ARM_SXC_ARGS  Result  = { 0 };

  Request.Arg0 = (BufferAddr) ? ARM_FID_FFA_MEM_DONATE_AARCH64 : ARM_FID_FFA_MEM_DONATE_AARCH32;
  Request.Arg1 = TotalLength;
  Request.Arg2 = FragmentLength;
  Request.Arg3 = (UINTN)BufferAddr;
  Request.Arg4 = PageCount;

  ArmCallSxc (&Request, &Result);

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
  ARM_SXC_ARGS  Request = { 0 };
  ARM_SXC_ARGS  Result  = { 0 };

  Request.Arg0 = ARM_FID_FFA_NOTIFICATION_SET;
  Request.Arg1 = ((UINT32)gPartId << 16) | DestinationId;
  Request.Arg2 = Flags;
  Request.Arg3 = (UINT32)NotificationBitmap;
  Request.Arg4 = (UINT32)(NotificationBitmap >> 32);

  ArmCallSxc (&Request, &Result);

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
  ARM_SXC_ARGS  Request = { 0 };
  ARM_SXC_ARGS  Result  = { 0 };

  Request.Arg0 = ARM_FID_FFA_NOTIFICATION_GET;
  Request.Arg1 = ((UINT32)VCpuId << 16) | gPartId;
  Request.Arg2 = Flags;

  ArmCallSxc (&Request, &Result);

  if (Result.Arg0 == ARM_FID_FFA_ERROR) {
    return FfaStatusToEfiStatus (Result.Arg2);
  }

  switch (Flags) {
    case FFA_NOTIFICATIONS_FLAG_BITMAP_SP:
      *NotificationBitmap = ((UINT64)Result.Arg3 << 32) | Result.Arg2;
      break;
    case FFA_NOTIFICATIONS_FLAG_BITMAP_VM:
      *NotificationBitmap = ((UINT64)Result.Arg5 << 32) | Result.Arg4;
      break;
    case FFA_NOTIFICATIONS_FLAG_BITMAP_HYP:
      *NotificationBitmap = ((UINT64)Result.Arg7 << 32) | Result.Arg6;
      break;
    default:
      return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
FfaPartitionInfoGetRegs (
  IN EFI_GUID                 *ServiceGuid,
  IN UINT16                   StartIndex,
  IN OUT UINT16               *Tag OPTIONAL,
  IN OUT UINT32               *PartDescCount,
  OUT EFI_FFA_PART_INFO_DESC  *PartDesc OPTIONAL
  )
{
  EFI_GUID  ServiceGuidMangled;
  UINT64    Metadata     = 0;
  UINT16    TagValue     = 0;
  UINT16    CurrentIndex = 0;
  UINT32    Count        = 0;

  ARM_SXC_ARGS  Request = { 0 };
  ARM_SXC_ARGS  Result  = { 0 };

  if (PartDesc == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (Tag != NULL) {
    TagValue = *Tag;
  }

  if (ServiceGuid != NULL) {
    CopyMem (&ServiceGuidMangled, ServiceGuid, sizeof (EFI_GUID));
  } else {
    ZeroMem (&ServiceGuidMangled, sizeof (EFI_GUID));
  }

  FfaPrepareGuid (&ServiceGuidMangled);
  Request.Arg0 = ARM_FID_FFA_PARTITION_INFO_GET_REGS;
  CopyMem (&Request.Arg1, &ServiceGuidMangled, sizeof (EFI_GUID));
  Request.Arg3 = ((UINT32)TagValue << 16) | StartIndex;

  ArmCallSxc (&Request, &Result);

  if (Result.Arg0 == ARM_FID_FFA_ERROR) {
    return FfaStatusToEfiStatus (Result.Arg2);
  }

  Metadata     = Result.Arg2;
  CurrentIndex = (UINT16)((Metadata >> 16) & 0xFFFF);
  Count        = CurrentIndex - StartIndex + 1;
  if ((PartDesc == NULL) ||
      (*PartDescCount < Count))
  {
    *PartDescCount = Count;
    return EFI_BUFFER_TOO_SMALL;
  }

  *PartDescCount = Count;
  for (UINTN Index = 0; Index < Count; Index++) {
    CopyMem (PartDesc + Index, (VOID *)((UINTN)(&Result.Arg3) + Index * sizeof (EFI_FFA_PART_INFO_DESC)), sizeof (EFI_FFA_PART_INFO_DESC));
    FfaPrepareGuid ((EFI_GUID *)PartDesc->PartitionUuid);
  }

  if (Tag != NULL) {
    *Tag = (UINT16)((Metadata >> 32) & 0xFFFF);
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
FfaNotificationBitmapCreate (
  IN UINT16  VCpuCount
  )
{
  ARM_SXC_ARGS  Request = { 0 };
  ARM_SXC_ARGS  Result  = { 0 };

  Request.Arg0 = ARM_FID_FFA_NOTIFICATION_BITMAP_CREATE;
  Request.Arg1 = gPartId;
  Request.Arg2 = VCpuCount;

  ArmCallSxc (&Request, &Result);

  if (Result.Arg0 == ARM_FID_FFA_ERROR) {
    return FfaStatusToEfiStatus (Result.Arg2);
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
FfaNotificationBitmapDestroy (
  VOID
  )
{
  ARM_SXC_ARGS  Request = { 0 };
  ARM_SXC_ARGS  Result  = { 0 };

  Request.Arg0 = ARM_FID_FFA_NOTIFICATION_BITMAP_DESTROY;
  Request.Arg1 = gPartId;

  ArmCallSxc (&Request, &Result);

  if (Result.Arg0 == ARM_FID_FFA_ERROR) {
    return FfaStatusToEfiStatus (Result.Arg2);
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
FfaNotificationBind (
  IN UINT16  DestinationId,
  IN UINT32  Flags,
  IN UINT64  NotificationBitmap
  )
{
  ARM_SXC_ARGS  Request = { 0 };
  ARM_SXC_ARGS  Result  = { 0 };

  Request.Arg0 = ARM_FID_FFA_NOTIFICATION_BIND;
  Request.Arg1 = ((UINT32)DestinationId << 16) | gPartId;
  Request.Arg2 = Flags;
  Request.Arg3 = (UINT32)NotificationBitmap;
  Request.Arg4 = (UINT32)(NotificationBitmap >> 32);

  ArmCallSxc (&Request, &Result);

  if (Result.Arg0 == ARM_FID_FFA_ERROR) {
    return FfaStatusToEfiStatus (Result.Arg2);
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
FfaNotificationUnbind (
  IN UINT16  DestinationId,
  IN UINT64  NotificationBitmap
  )
{
  ARM_SXC_ARGS  Request = { 0 };
  ARM_SXC_ARGS  Result  = { 0 };

  Request.Arg0 = ARM_FID_FFA_NOTIFICATION_UNBIND;
  Request.Arg1 = ((UINT32)DestinationId << 16) | gPartId;
  Request.Arg2 = 0;
  Request.Arg3 = (UINT32)NotificationBitmap;
  Request.Arg4 = (UINT32)(NotificationBitmap >> 32);

  ArmCallSxc (&Request, &Result);

  if (Result.Arg0 == ARM_FID_FFA_ERROR) {
    return FfaStatusToEfiStatus (Result.Arg2);
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
FfaMemDonateRxTx (
  UINT32  TotalLength,
  UINT32  FragmentLength,
  UINT64  *Handle
  )
{
  return FfaMemDonate (TotalLength, FragmentLength, NULL, 0, Handle);
}

EFI_STATUS
EFIAPI
FfaMemLend (
  UINT32  TotalLength,
  UINT32  FragmentLength,
  VOID    *BufferAddr,
  UINT32  PageCount,
  UINT64  *Handle
  )
{
  ARM_SXC_ARGS  Request = { 0 };
  ARM_SXC_ARGS  Result  = { 0 };

  Request.Arg0 = (BufferAddr) ? ARM_FID_FFA_MEM_LEND_AARCH64 : ARM_FID_FFA_MEM_LEND_AARCH32;
  Request.Arg1 = TotalLength;
  Request.Arg2 = FragmentLength;
  Request.Arg3 = (UINTN)BufferAddr;
  Request.Arg4 = PageCount;

  ArmCallSxc (&Request, &Result);

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
FfaMemLendRxTx (
  UINT32  TotalLength,
  UINT32  FragmentLength,
  UINT64  *Handle
  )
{
  return FfaMemLend (TotalLength, FragmentLength, NULL, 0, Handle);
}

EFI_STATUS
EFIAPI
FfaMemShare (
  UINT32  TotalLength,
  UINT32  FragmentLength,
  VOID    *BufferAddr,
  UINT32  PageCount,
  UINT64  *Handle
  )
{
  ARM_SXC_ARGS  Request = { 0 };
  ARM_SXC_ARGS  Result  = { 0 };

  Request.Arg0 = (BufferAddr) ? ARM_FID_FFA_MEM_SHARE_AARCH64 : ARM_FID_FFA_MEM_SHARE_AARCH32;
  Request.Arg1 = TotalLength;
  Request.Arg2 = FragmentLength;
  Request.Arg3 = (UINTN)BufferAddr;
  Request.Arg4 = PageCount;

  ArmCallSxc (&Request, &Result);

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
FfaMemShareRxTx (
  UINT32  TotalLength,
  UINT32  FragmentLength,
  UINT64  *Handle
  )
{
  return FfaMemShare (TotalLength, FragmentLength, NULL, 0, Handle);
}

EFI_STATUS
EFIAPI
FfaMemRetrieveReq (
  UINT32  TotalLength,
  UINT32  FragmentLength,
  VOID    *BufferAddr,
  UINT32  PageCount,
  UINT32  *RespTotalLength,
  UINT32  *RespFragmentLength
  )
{
  ARM_SXC_ARGS  Request = { 0 };
  ARM_SXC_ARGS  Result  = { 0 };

  Request.Arg0 = (BufferAddr) ? ARM_FID_FFA_MEM_RETRIEVE_REQ_AARCH64 : ARM_FID_FFA_MEM_RETRIEVE_REQ_AARCH32;
  Request.Arg1 = TotalLength;
  Request.Arg2 = FragmentLength;
  Request.Arg3 = (UINTN)BufferAddr;
  Request.Arg4 = PageCount;

  ArmCallSxc (&Request, &Result);

  if (Result.Arg0 == ARM_FID_FFA_ERROR) {
    *RespTotalLength    = 0U;
    *RespFragmentLength = 0U;
    return FfaStatusToEfiStatus (Result.Arg2);
  }

  ASSERT (Result.Arg0 == ARM_FID_FFA_MEM_RETRIEVE_RESP);
  *RespTotalLength    = Result.Arg1;
  *RespFragmentLength = Result.Arg2;
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
FfaMemRetrieveReqRxTx (
  UINT32  TotalLength,
  UINT32  FragmentLength,
  UINT32  *RespTotalLength,
  UINT32  *RespFragmentLength
  )
{
  return FfaMemRetrieveReq (
           TotalLength,
           FragmentLength,
           NULL,
           0,
           RespTotalLength,
           RespFragmentLength
           );
}

EFI_STATUS
EFIAPI
FfaMemRelinquish (
  VOID
  )
{
  ARM_SXC_ARGS  Request = { 0 };
  ARM_SXC_ARGS  Result  = { 0 };

  Request.Arg0 = ARM_FID_FFA_MEM_RETRIEVE_RELINQUISH;

  ArmCallSxc (&Request, &Result);

  if (Result.Arg0 == ARM_FID_FFA_ERROR) {
    return FfaStatusToEfiStatus (Result.Arg2);
  }

  ASSERT (Result.Arg0 == ARM_FID_FFA_SUCCESS_AARCH32);
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
FfaMemReclaim (
  UINT64  Handle,
  UINT32  Flags
  )
{
  ARM_SXC_ARGS  Request  = { 0 };
  ARM_SXC_ARGS  Result   = { 0 };
  UINT32        HandleHi = 0;
  UINT32        HandleLo = 0;

  HandleHi = (Handle >> 32) & MAX_UINT32;
  HandleLo = Handle & MAX_UINT32;

  Request.Arg0 = ARM_FID_FFA_MEM_RETRIEVE_RECLAIM;
  Request.Arg1 = HandleLo;
  Request.Arg2 = HandleHi;
  Request.Arg3 = Flags;

  ArmCallSxc (&Request, &Result);

  if (Result.Arg0 == ARM_FID_FFA_ERROR) {
    return FfaStatusToEfiStatus (Result.Arg2);
  }

  ASSERT (Result.Arg0 == ARM_FID_FFA_SUCCESS_AARCH32);
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
FfaMemPermGet (
  CONST VOID  *BaseAddr,
  UINT32      *MemoryPerm
  )
{
  ARM_SXC_ARGS  Request = { 0 };
  ARM_SXC_ARGS  Result  = { 0 };

  Request.Arg0 = ARM_FID_FFA_MEM_PERM_GET_AARCH32;
  Request.Arg1 = (UINTN)BaseAddr;

  ArmCallSxc (&Request, &Result);

  if (Result.Arg0 == ARM_FID_FFA_ERROR) {
    return FfaStatusToEfiStatus (Result.Arg2);
  }

  ASSERT (Result.Arg0 == ARM_FID_FFA_SUCCESS_AARCH32);
  *MemoryPerm = Result.Arg2;
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
FfaMemPermSet (
  CONST VOID  *BaseAddr,
  UINT32      PageCount,
  UINT32      MemoryPerm
  )
{
  ARM_SXC_ARGS  Request = { 0 };
  ARM_SXC_ARGS  Result  = { 0 };

  ASSERT ((MemoryPerm & ARM_FFA_MEM_PERM_RESERVED_MASK) == 0);

  Request.Arg0 = ARM_FID_FFA_MEM_PERM_SET_AARCH32;
  Request.Arg1 = (UINTN)BaseAddr;
  Request.Arg2 = PageCount;
  Request.Arg3 = MemoryPerm;

  ArmCallSxc (&Request, &Result);

  if (Result.Arg0 == ARM_FID_FFA_ERROR) {
    return FfaStatusToEfiStatus (Result.Arg2);
  }

  ASSERT (Result.Arg0 == ARM_FID_FFA_SUCCESS_AARCH32);
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
FfaConsoleLog32 (
  CONST CHAR8  *Message,
  UINTN        Length
  )
{
  ARM_SXC_ARGS  Request      = { 0 };
  ARM_SXC_ARGS  Result       = { 0 };
  UINT32        CharLists[6] = { 0 };

  ASSERT (Length > 0 && Length <= sizeof (CharLists));

  CopyMem (CharLists, Message, MIN (Length, sizeof (CharLists)));

  Request.Arg0 = ARM_FID_FFA_CONSOLE_LOG_AARCH32;
  Request.Arg1 = Length;
  Request.Arg2 = CharLists[0];
  Request.Arg3 = CharLists[1];
  Request.Arg4 = CharLists[2];
  Request.Arg5 = CharLists[3];
  Request.Arg6 = CharLists[4];
  Request.Arg7 = CharLists[5];

  ArmCallSxc (&Request, &Result);

  if (Result.Arg0 == ARM_FID_FFA_ERROR) {
    return FfaStatusToEfiStatus (Result.Arg2);
  }

  ASSERT (Result.Arg0 == ARM_FID_FFA_SUCCESS_AARCH32);
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
FfaConsoleLog64 (
  CONST char  *Message,
  UINTN       Length
  )
{
  ARM_SXC_ARGS  Request       = { 0 };
  ARM_SXC_ARGS  Result        = { 0 };
  UINT64        CharLists[16] = { 0 };

  ASSERT (Length > 0 && Length <= sizeof (CharLists));

  CopyMem (CharLists, Message, MIN (Length, sizeof (CharLists)));

  Request.Arg0  = ARM_FID_FFA_CONSOLE_LOG_AARCH64;
  Request.Arg1  = Length;
  Request.Arg2  = CharLists[0];
  Request.Arg3  = CharLists[1];
  Request.Arg4  = CharLists[2];
  Request.Arg5  = CharLists[3];
  Request.Arg6  = CharLists[4];
  Request.Arg7  = CharLists[5];
  Request.Arg8  = CharLists[6];
  Request.Arg9  = CharLists[7];
  Request.Arg10 = CharLists[8];
  Request.Arg11 = CharLists[9];
  Request.Arg12 = CharLists[10];
  Request.Arg13 = CharLists[11];
  Request.Arg14 = CharLists[12];
  Request.Arg15 = CharLists[13];
  Request.Arg16 = CharLists[14];
  Request.Arg17 = CharLists[15];

  ArmCallSxc (&Request, &Result);

  if (Result.Arg0 == ARM_FID_FFA_ERROR) {
    return FfaStatusToEfiStatus (Result.Arg2);
  }

  ASSERT (Result.Arg0 == ARM_FID_FFA_SUCCESS_AARCH32);
  return EFI_SUCCESS;
}
