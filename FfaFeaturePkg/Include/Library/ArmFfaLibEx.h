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

/**
 * @brief      Sends a 32 bit partition message in parameter registers as a
 *             request and blocks until the response is available.
 * @note       The ffa_interrupt_handler function can be called during the
 *             execution of this function
 *
 * @param[in]  source            Source endpoint ID
 * @param[in]  dest              Destination endpoint ID
 * @param[in]  a0,a1,a2,a3,a4    Implementation defined message values
 * @param[out] msg               The response message
 *
 * @return     The FF-A error status code
 */
EFI_STATUS
EFIAPI
FfaMessageSendDirectReq2 (
  IN      UINT16              DestPartId,
  IN      EFI_GUID            *ServiceGuid OPTIONAL,
  IN OUT  DIRECT_MSG_ARGS_EX  *ImpDefArgs
  );

/**
 * @brief      Sends a 32 bit partition message in parameter registers as a
 *             response and blocks until the response is available.
 * @note       The ffa_interrupt_handler function can be called during the
 *             execution of this function
 *
 * @param[in]  source            Source endpoint ID
 * @param[in]  dest              Destination endpoint ID
 * @param[in]  a0,a1,a2,a3,a4    Implementation defined message values
 * @param[out] msg               The response message
 *
 * @return     The FF-A error status code
 */
EFI_STATUS
EFIAPI
FfaMessageSendDirectResp32 (
  DIRECT_MSG_ARGS_EX  *req,
  DIRECT_MSG_ARGS_EX  *resp
  );

/**
 * @brief      Sends a 64 bit partition message in parameter registers as a
 *             response and blocks until the response is available.
 * @note       The ffa_interrupt_handler function can be called during the
 *             execution of this function
 *
 * @param[in]  source            Source endpoint ID
 * @param[in]  dest              Destination endpoint ID
 * @param[in]  a0,a1,a2,a3,a4    Implementation defined message values
 * @param[out] msg               The response message
 *
 * @return     The FF-A error status code
 */
EFI_STATUS
EFIAPI
FfaMessageSendDirectResp64 (
  IN DIRECT_MSG_ARGS_EX   *Request,
  OUT DIRECT_MSG_ARGS_EX  *Response
  );

EFI_STATUS
EFIAPI
FfaMessageSendDirectResp2 (
  IN DIRECT_MSG_ARGS_EX   *Request,
  OUT DIRECT_MSG_ARGS_EX  *Response
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
FfaPartitionInfoGetRegs (
  IN EFI_GUID                 *ServiceGuid,
  IN UINT16                   StartIndex,
  IN OUT UINT16               *Tag OPTIONAL,
  IN OUT UINT32               *PartDescCount,
  OUT EFI_FFA_PART_INFO_DESC  *PartDesc OPTIONAL
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

/**
 * Memory management interfaces
 *
 * @note Functions with _rxtx suffix use the RX/TX buffers mapped by
 * ffa_rxtx_map to transmit memory descriptors instead of an distinct buffer
 * allocated by the owner.
 */

/**
 * @brief      Starts a transaction to transfer of ownership of a memory region
 *             from a Sender endpoint to a Receiver endpoint.
 *
 * @param[in]  total_length     Total length of the memory transaction
 *                              descriptor in bytes
 * @param[in]  fragment_length  Length in bytes of the memory transaction
 *                              descriptor passed in this ABI invocation
 * @param[in]  buffer_address   Base address of a buffer allocated by the Owner
 *                              and distinct from the TX buffer
 * @param[in]  page_count       Number of 4K pages in the buffer allocated by
 *                              the Owner and distinct from the TX buffer
 * @param[out] handle           Globally unique Handle to identify the memory
 *                              region upon successful transmission of the
 *                              transaction descriptor.
 *
 * @return     The FF-A error status code
 */
EFI_STATUS
ffa_mem_donate (
  UINT32  total_length,
  UINT32  fragment_length,
  VOID    *buffer_address,
  UINT32  page_count,
  UINT64  *handle
  );

EFI_STATUS
ffa_mem_donate_rxtx (
  UINT32  total_length,
  UINT32  fragment_length,
  UINT64  *handle
  );

/**
 * @brief      Starts a transaction to transfer an Owner’s access to a memory
 *             region and  grant access to it to one or more Borrowers.
 *
 * @param[in]  total_length     Total length of the memory transaction
 *                              descriptor in bytes
 * @param[in]  fragment_length  Length in bytes of the memory transaction
 *                              descriptor passed in this ABI invocation
 * @param[in]  buffer_address   Base address of a buffer allocated by the Owner
 *                              and distinct from the TX buffer
 * @param[in]  page_count       Number of 4K pages in the buffer allocated by
 *                              the Owner and distinct from the TX buffer
 * @param[out] handle           Globally unique Handle to identify the memory
 *                              region upon successful transmission of the
 *                              transaction descriptor.
 *
 * @return     The FF-A error status code
 */
EFI_STATUS
EFIAPI
FfaMemLend (
  UINT32  TotalLength,
  UINT32  FragmentLength,
  VOID    *BufferAddr,
  UINT32  PageCount,
  UINT64  *Handle
  );

EFI_STATUS
EFIAPI
FfaMemLendRxTx (
  UINT32  TotalLength,
  UINT32  FragmentLength,
  UINT64  *Handle
  );

/**
 * @brief      Starts a transaction to grant access to a memory region to one or
 *             more Borrowers.
 *
 * @param[in]  total_length     Total length of the memory transaction
 *                              descriptor in bytes
 * @param[in]  fragment_length  Length in bytes of the memory transaction
 *                              descriptor passed in this ABI invocation
 * @param[in]  buffer_address   Base address of a buffer allocated by the Owner
 *                              and distinct from the TX buffer
 * @param[in]  page_count       Number of 4K pages in the buffer allocated by
 *                              the Owner and distinct from the TX buffer
 * @param[out] handle           Globally unique Handle to identify the memory
 *                              region upon successful transmission of the
 *                              transaction descriptor.
 *
 * @return     The FF-A error status code
 */
EFI_STATUS
EFIAPI
FfaMemShare (
  UINT32  TotalLength,
  UINT32  FragmentLength,
  VOID    *BufferAddr,
  UINT32  PageCount,
  UINT64  *Handle
  );

EFI_STATUS
EFIAPI
FfaMemShareRxTx (
  UINT32  TotalLength,
  UINT32  FragmentLength,
  UINT64  *Handle
  );

/**
 * @brief      Requests completion of a donate, lend or share memory management
 *             transaction.
 *
 * @param[in]  total_length          Total length of the memory transaction
 *                                   descriptor in bytes
 * @param[in]  fragment_length       Length in bytes of the memory transaction
 *                                   descriptor passed in this ABI invocation
 * @param[in]  buffer_address        Base address of a buffer allocated by the
 *                                   Owner and distinct from the TX buffer
 * @param[in]  page_count            Number of 4K pages in the buffer allocated
 *                                   by the Owner and distinct from the TX
 *                                   buffer
 * @param[out] resp_total_length     Total length of the response memory
 *                                   transaction descriptor in bytes
 * @param[out] resp_fragment_length  Length in bytes of the response memory
 *                                   transaction descriptor passed in this ABI
 *                                   invocation
 *
 * @return     The FF-A error status code
 */
EFI_STATUS
EFIAPI
FfaMemRetrieveReq (
  UINT32  TotalLength,
  UINT32  FragmentLength,
  VOID    *BufferAddr,
  UINT32  PageCount,
  UINT32  *RespTotalLength,
  UINT32  *RespFragmentLength
  );

EFI_STATUS
EFIAPI
FfaMemRetrieveReqRxTx (
  UINT32  TotalLength,
  UINT32  FragmentLength,
  UINT32  *RespTotalLength,
  UINT32  *RespFragmentLength
  );

/**
 * @brief      Starts a transaction to transfer access to a shared or lent
 *             memory region from a Borrower back to its Owner.
 *
 * @return     The FF-A error status code
 */
EFI_STATUS
EFIAPI
FfaMemRelinquish (
  VOID
  );

/**
 * @brief      Restores exclusive access to a memory region back to its Owner.
 *
 * @param[in]  handle  Globally unique Handle to identify the memory region
 * @param[in]  flags   Flags for modifying the reclaim behavior
 *
 * @return     The FF-A error status code
 */
EFI_STATUS
EFIAPI
FfaMemReclaim (
  UINT64  Handle,
  UINT32  Flags
  );

/**
 * @brief       Queries the memory attributes of a memory region. This function
 *              can only access the regions of the SP's own translation regine.
 *              Moreover this interface is only available in the boot phase,
 *              i.e. before invoking FFA_MSG_WAIT interface.
 *
 * @param[in]   base_address    Base VA of a translation granule whose
 *                              permission attributes must be returned.
 * @param[out]  mem_perm        Permission attributes of the memory region
 *
 * @return      The FF-A error status code
 */
EFI_STATUS
EFIAPI
FfaMemPermGet (
  CONST VOID  *BaseAddr,
  UINT32      *MemoryPerm
  );

/**
 * @brief       Sets the memory attributes of a memory regions. This function
 *              can only access the regions of the SP's own translation regine.
 *              Moreover this interface is only available in the boot phase,
 *              i.e. before invoking FFA_MSG_WAIT interface.
 *
 * @param[in]   base_address    Base VA of a memory region whose permission
 *                              attributes must be set.
 * @param[in]   page_count      Number of translation granule size pages
 *                              starting from the Base address whose permissions
 *                              must be set.
 * @param[in]   mem_perm        Permission attributes to be set for the memory
 *                              region
 * @return      The FF-A error status code
 */
EFI_STATUS
EFIAPI
FfaMemPermSet (
  CONST VOID  *BaseAddr,
  UINT32      PageCount,
  UINT32      MemoryPerm
  );

/**
 * @brief       Allow an entity to provide debug logging to the console. Uses
 *              32 bit registers to pass characters.
 *
 * @param message       Message characters
 * @param length        Message length, max FFA_CONSOLE_LOG_32_MAX_LENGTH
 * @return              The FF-A error status code
 */
EFI_STATUS
EFIAPI
FfaConsoleLog32 (
  CONST CHAR8  *Message,
  UINTN        Length
  );

/**
 * @brief       Allow an entity to provide debug logging to the console. Uses
 *              64 bit registers to pass characters.
 *
 * @param message       Message characters
 * @param length        Message length, max FFA_CONSOLE_LOG_64_MAX_LENGTH
 * @return              The FF-A error status code
 */
EFI_STATUS
EFIAPI
FfaConsoleLog64 (
  CONST char  *Message,
  UINTN       Length
  );

#endif /* FF_A_HELPER_LIB_H_ */
