/** @file
  Platform layer for the secure partition interrupt handler using FF-A.

        Copyright (c) 2020 - 2022, Arm Ltd. All rights reserved.<BR>
  Copyright (c), Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef FF_A_HELPER_LIB_H_
#define FF_A_HELPER_LIB_H_

#include <Base.h>

#if PcdGetBool (PcdFfaLibConduitSmc) == 1
typedef ARM_SMC_ARGS ARM_SXC_ARGS;
#else
typedef ARM_SVC_ARGS ARM_SXC_ARGS;
#endif

/**
 * @brief Direct message type
 */
typedef struct {
  UINT32      FunctionId;

  UINT16      SourceId;

  UINT16      DestinationId;

  /// Not applicable for v1
  EFI_GUID    ServiceGuid;

  /// Implementation define argument 0, this will be set to/from x4(v2)
  UINTN       Arg0;

  /// Implementation define argument 1, this will be set to/from x5(v2)
  UINTN       Arg1;

  /// Implementation define argument 2, this will be set to/from x6(v2)
  UINTN       Arg2;

  /// Implementation define argument 3, this will be set to/from x7(v2)
  UINTN       Arg3;

  /// Implementation define argument 4, this will be set to/from x8(v2)
  UINTN       Arg4;

  /// Implementation define argument 5, this will be set to/from x9(v2)
  UINTN       Arg5;

  /// Implementation define argument 6, this will be set to/from x10(v2)
  UINTN       Arg6;

  /// Implementation define argument 7, this will be set to/from x11(v2)
  UINTN       Arg7;

  /// Implementation define argument 8, this will be set to/from x12(v2)
  UINTN       Arg8;

  /// Implementation define argument 9, this will be set to/from x13(v2)
  UINTN       Arg9;

  /// Implementation define argument 10, this will be set to/from x14(v2)
  UINTN       Arg10;

  /// Implementation define argument 11, this will be set to/from x15(v2)
  UINTN       Arg11;

  /// Implementation define argument 12, this will be set to/from x16(v2)
  UINTN       Arg12;

  /// Implementation define argument 13, this will be set to/from x17(v2)
  UINTN       Arg13;
} DIRECT_MSG_ARGS_EX;

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

/**
 * CPU cycle management interfaces
 */

/**
 * @brief      Blocks the caller until a message is available or until an
 *             interrupt happens. It is also used to indicate the completion of
 *             the boot phase and the end of the interrupt handling.
 * @note       The ffa_interrupt_handler function can be called during the
 *             execution of this function.
 *
 * @param[out] msg   The incoming message
 *
 * @return     The FF-A error status code
 */
EFI_STATUS
EFIAPI
FfaMessageWait (
  OUT DIRECT_MSG_ARGS_EX  *Message
  );

/** Messaging interfaces */

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
  IN UINT16  VCpuId,
  IN UINT64  Flags,
  IN UINT64  *NotificationBitmap
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

#endif /* FF_A_HELPER_LIB_H_ */
