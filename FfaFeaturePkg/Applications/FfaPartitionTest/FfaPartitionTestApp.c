/** @file
FfaPartitionTestApp.c

This is a Unit Test for the test FFA service library.

Copyright (C) Microsoft Corporation. All rights reserved.
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <IndustryStandard/ArmFfaSvc.h>
#include <IndustryStandard/ArmFfaBootInfo.h>
#include <IndustryStandard/ArmFfaPartInfo.h>
#include <Pi/PiMultiPhase.h>
#include <Protocol/HardwareInterrupt.h>

#include <Library/ArmSmcLib.h>
#include <Library/ArmSvcLib.h>
#include <Library/ArmLib.h>
#include <Library/ArmFfaLib.h>
#include <Library/ArmFfaLibEx.h>
#include <Library/ArmGicLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UnitTestLib.h>
#include <Guid/NotificationServiceFfa.h>
#include <Guid/TestServiceFfa.h>
#include <Guid/Tpm2ServiceFfa.h>

#define UNIT_TEST_APP_NAME     "FF-A Functional Test"
#define UNIT_TEST_APP_VERSION  "0.2"

UINT16                           FfaPartId;
EFI_HARDWARE_INTERRUPT_PROTOCOL  *gInterrupt;

/**
  EFI_CPU_INTERRUPT_HANDLER that is called when a processor interrupt occurs.

  @param  InterruptType    Defines the type of interrupt or exception that
                           occurred on the processor. This parameter is
                           processor architecture specific.
  @param  SystemContext    A pointer to the processor context when
                           the interrupt occurred on the processor.

  @return None

**/
VOID
EFIAPI
ApIrqInterruptHandler (
  IN  HARDWARE_INTERRUPT_SOURCE  Source,
  IN  EFI_SYSTEM_CONTEXT         SystemContext
  )
{
  EFI_STATUS  Status;
  UINT64      Bitmap;

  DEBUG ((DEBUG_INFO, "Received IRQ interrupt %d!\n", Source));

  // Then register this test app to receive notifications from the Ffa test SP
  Status = FfaNotificationGet (0, FFA_NOTIFICATIONS_FLAG_BITMAP_SP, &Bitmap);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Unable to notification get with FF-A Ffa test SP (%r).\n", Status));
  } else {
    DEBUG ((DEBUG_INFO, "Got notification from FF-A Ffa test SP with VM bitmap %x.\n", Bitmap));
  }

  gInterrupt->EndOfInterrupt (gInterrupt, Source);
}

/// ================================================================================================
/// ================================================================================================
///
/// TEST CASES
///
/// ================================================================================================
/// ================================================================================================

/**
  This routine queries the version of FF-A framework, it must be at least the version
  required by this UEFI codebase.
**/
UNIT_TEST_STATUS
EFIAPI
FfaMiscVerifyVersion (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UNIT_TEST_STATUS  utStatus = UNIT_TEST_RUNNING;
  EFI_STATUS        Status   = EFI_SUCCESS;
  UINT16            CurrentMajorVersion;
  UINT16            CurrentMinorVersion;

  DEBUG ((DEBUG_INFO, "%a: enter...\n", __func__));

  // Query FF-A version to make sure FF-A is supported
  Status = ArmFfaLibGetVersion (
             ARM_FFA_MAJOR_VERSION,
             ARM_FFA_MINOR_VERSION,
             &CurrentMajorVersion,
             &CurrentMinorVersion
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to get FF-A version. Status: %r\n", Status));
    UT_ASSERT_NOT_EFI_ERROR (Status);
  }

  DEBUG ((DEBUG_INFO, "%a FF-A version: %d.%d\n", __func__, CurrentMajorVersion, CurrentMinorVersion));

  UT_ASSERT_TRUE (
    (CurrentMajorVersion >= ARM_FFA_MAJOR_VERSION) &&
    (CurrentMinorVersion >= ARM_FFA_MINOR_VERSION)
    );

  utStatus = UNIT_TEST_PASSED;
  UT_LOG_INFO ("FF-A version is supported: %d.%d", CurrentMajorVersion, CurrentMinorVersion);
  return utStatus;
}

/**
  FfaPartitionTestAppEntry

  @param[in] ImageHandle  The firmware allocated handle for the EFI image.
  @param[in] SystemTable  A pointer to the EFI System Table.

  @retval EFI_SUCCESS     The entry point executed successfully.
  @retval other           Some error occurred when executing this entry point.

**/
EFI_STATUS
EFIAPI
FfaPartitionTestAppEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS              Status;
  UNIT_TEST_FRAMEWORK_HANDLE  Fw             = NULL;
  UNIT_TEST_SUITE_HANDLE      Misc           = NULL;

  DEBUG ((DEBUG_ERROR, "%a %a v%a\n", __FUNCTION__, UNIT_TEST_APP_NAME, UNIT_TEST_APP_VERSION));

  // Start setting up the test framework for running the tests.
  Status = InitUnitTestFramework (&Fw, UNIT_TEST_APP_NAME, gEfiCallerBaseName, UNIT_TEST_APP_VERSION);
  if (EFI_ERROR (Status) != FALSE) {
    DEBUG ((DEBUG_ERROR, "%a Failed in InitUnitTestFramework. Status = %r\n", __FUNCTION__, Status));
    goto Cleanup;
  }

  // Misc test suite for all tests.
  CreateUnitTestSuite (&Misc, Fw, "FF-A Miscellaneous Test cases", "Ffa.Miscellaneous", NULL, NULL);

  if (Misc == NULL) {
    DEBUG ((DEBUG_ERROR, "%a Failed in CreateUnitTestSuite for TestSuite\n", __FUNCTION__));
    Status = EFI_OUT_OF_RESOURCES;
    goto Cleanup;
  }

  AddTestCase (
    Misc,
    "Verify FF-A framework version",
    "Ffa.Miscellaneous.VerifyVersion",
    FfaMiscVerifyVersion,
    NULL,
    NULL,
    NULL
    );

  AddTestCase (
    Misc,
    "Fatal error Ex report",
    "MsWhea.Miscellaneous.MsWheaFatalExEntries",
    FfaTestGetVersion,
    NULL,
    NULL,
    NULL
    );

  ARM_SMC_ARGS            SmcArgs;
  EFI_FFA_PART_INFO_DESC  FfaTestPartInfo;
  UINT32                  Count;
  UINT32                  Size;
  UINTN                   SriIndex;
  UINTN                   Dummy;
  DIRECT_MSG_ARGS_EX      DirectMsgArgsEx;
  UINT16                  CurrentMajorVersion;
  UINT16                  CurrentMinorVersion;
  NotificationMapping     Mapping;
  UINT8                   NumMappings;
  UINT16                  BindBitPos;
  INT8                    ResponseVal;

  // FF-A is supported, then discover the Ffa Test SP's presence, ID, our ID and
  // retrieve our RX/TX buffers.
  Status = EFI_UNSUPPORTED;

  // Get our ID
  Status = ArmFfaLibPartitionIdGet (&FfaPartId);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Unable to retrieve FF-A partition ID (%r).\n", Status));
    goto Done;
  }

  // Discover the Ffa all SPs through registers.
  ZeroMem (&SmcArgs, sizeof (SmcArgs));
  Count  = 5;
  Status = FfaPartitionInfoGetRegs (NULL, 0, NULL, &Count, (EFI_FFA_PART_INFO_DESC *)&SmcArgs.Arg3);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Unable to discover FF-A all SP (%r).\n", Status));
    goto Done;
  }

  DUMP_HEX (DEBUG_INFO, 0, &SmcArgs, sizeof (SmcArgs), "    ");

  // Retrieve the partition information from the returned registers
  CopyMem (&FfaTestPartInfo, &SmcArgs.Arg3, sizeof (EFI_FFA_PART_INFO_DESC));

  DEBUG ((DEBUG_INFO, "Discovered first FF-A Ffa SP.\n"));
  DEBUG ((
    DEBUG_INFO,
    "\tID = 0x%lx, Execution contexts = %d, Properties = 0x%lx.\n",
    FfaTestPartInfo.PartitionId,
    FfaTestPartInfo.ExecContextCountOrProxyPartitionId,
    FfaTestPartInfo.PartitionProps
    ));
  DEBUG ((
    DEBUG_INFO,
    "\tSP Guid = %g.\n",
    FfaTestPartInfo.PartitionUuid
    ));

  // Discover the Ffa test SP after converting the EFI_GUID to a format TF-A will
  // understand.
  Status = ArmFfaLibPartitionInfoGet (&gEfiTestServiceFfaGuid, 0, &Count, &Size);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Unable to discover FF-A test SP (%r).\n", Status));
    goto Done;
  }

  // Retrieve the partition information from the returned registers
  CopyMem (&FfaTestPartInfo, (VOID *)PcdGet64 (PcdFfaRxBuffer), sizeof (EFI_FFA_PART_INFO_DESC));

  DEBUG ((DEBUG_INFO, "Discovered FF-A test SP.\n"));
  DEBUG ((
    DEBUG_INFO,
    "\tID = 0x%lx, Execution contexts = %d, Properties = 0x%lx.\n",
    FfaTestPartInfo.PartitionId,
    FfaTestPartInfo.ExecContextCountOrProxyPartitionId,
    FfaTestPartInfo.PartitionProps
    ));
  DEBUG ((
    DEBUG_INFO,
    "\tSP Guid = %g.\n",
    FfaTestPartInfo.PartitionUuid
    ));

  //
  // Should be able to handle notification flow now.
  //
  // Now register UEFI to receive notifications by creating notification bitmaps
  Status = FfaNotificationBitmapCreate (1);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Unable to create notification bitmap with FF-A Ffa test SP (%r).\n", Status));
    goto Done;
  }

  // Followed by querying which notification ID is supported by the Ffa test SP
  Status = ArmFfaLibGetFeatures (ARM_FFA_FEATURE_ID_SCHEDULE_RECEIVER_INTERRUPT, 0, &SriIndex, &Dummy);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Unable to query feature SRI number with FF-A Ffa test SP (%r).\n", Status));
    goto Done;
  }

  DEBUG ((DEBUG_INFO, "Received feature SRI number with FF-A Ffa test SP (%d).\n", SriIndex));

  // Register the IRQ handler for the FF-A Ffa test SP
  Status = gBS->LocateProtocol (&gHardwareInterruptProtocolGuid, NULL, (VOID **)&gInterrupt);
  if (!EFI_ERROR (Status)) {
    Status = gInterrupt->RegisterInterruptSource (gInterrupt, SriIndex, ApIrqInterruptHandler);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Unable to register notification (%r).\n", Status));
      goto Done;
    }
  }

  // Then register this test app to receive notifications from the Ffa test SP
  BindBitPos = 0x02;
  FfaNotificationBind (FfaTestPartInfo.PartitionId, 0, (1 << BindBitPos));
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Unable to bind notification with FF-A Ffa test SP (%r).\n", Status));
    goto Done;
  }

  DEBUG ((DEBUG_INFO, "Binding Bit%x - Value: %x Successful.\n", BindBitPos, (1 << BindBitPos)));

  // Register the Battery Service Notification Mappings
  Mapping.Uint64 = 0;
  NumMappings    = 0x05;
  ZeroMem (&DirectMsgArgsEx, sizeof (DirectMsgArgsEx));
  /* Set the receiver service UUID */
  /* x4-x6 (i.e. Arg0-Arg2) should be 0 */
  DirectMsgArgsEx.Arg3 = 0xba7aff2eb1eac765;
  DirectMsgArgsEx.Arg4 = 0xb710b3a359f64054;
  DirectMsgArgsEx.Arg5 = NOTIFICATION_OPCODE_REGISTER;
  DirectMsgArgsEx.Arg6 = NumMappings;
  DEBUG ((DEBUG_INFO, "Registering %x Mappings:\n", NumMappings));
  Mapping.Bits.Cookie  = 0;
  Mapping.Bits.Id      = 0;
  DirectMsgArgsEx.Arg7 = Mapping.Uint64;
  DEBUG ((DEBUG_INFO, "Cookie: %x, Id: %x\n", Mapping.Bits.Cookie, Mapping.Bits.Id));
  DEBUG ((DEBUG_INFO, "Register Value: %lx\n", DirectMsgArgsEx.Arg7));
  Mapping.Bits.Cookie  = 1;
  Mapping.Bits.Id      = 1;
  DirectMsgArgsEx.Arg8 = Mapping.Uint64;
  DEBUG ((DEBUG_INFO, "Cookie: %x, Id: %x\n", Mapping.Bits.Cookie, Mapping.Bits.Id));
  DEBUG ((DEBUG_INFO, "Register Value: %lx\n", DirectMsgArgsEx.Arg8));
  Mapping.Bits.Cookie  = 2;
  Mapping.Bits.Id      = 2;
  DirectMsgArgsEx.Arg9 = Mapping.Uint64;
  DEBUG ((DEBUG_INFO, "Cookie: %x, Id: %x\n", Mapping.Bits.Cookie, Mapping.Bits.Id));
  DEBUG ((DEBUG_INFO, "Register Value: %lx\n", DirectMsgArgsEx.Arg9));
  Mapping.Bits.Cookie   = 3;
  Mapping.Bits.Id       = 3;
  DirectMsgArgsEx.Arg10 = Mapping.Uint64;
  DEBUG ((DEBUG_INFO, "Cookie: %x, Id: %x\n", Mapping.Bits.Cookie, Mapping.Bits.Id));
  DEBUG ((DEBUG_INFO, "Register Value: %lx\n", DirectMsgArgsEx.Arg10));
  Mapping.Bits.Cookie   = 4;
  Mapping.Bits.Id       = 4;
  DirectMsgArgsEx.Arg11 = Mapping.Uint64;
  DEBUG ((DEBUG_INFO, "Cookie: %x, Id: %x\n", Mapping.Bits.Cookie, Mapping.Bits.Id));
  DEBUG ((DEBUG_INFO, "Register Value: %lx\n", DirectMsgArgsEx.Arg11));
  Status = FfaMessageSendDirectReq2 (FfaTestPartInfo.PartitionId, &gEfiNotificationServiceFfaGuid, &DirectMsgArgsEx);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Unable to communicate direct req 2 with FF-A Ffa test SP (%r).\n", Status));
    goto Done;
  }

  ResponseVal = (INT8)DirectMsgArgsEx.Arg6;
  if (ResponseVal != NOTIFICATION_STATUS_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "Command Failed: %d\n", DirectMsgArgsEx.Arg6));
    goto Done;
  } else {
    DEBUG ((DEBUG_INFO, "Battery Service Register Success\n"));
  }

  // Register the Thermal Service Notification Mappings
  Mapping.Uint64 = 0;
  NumMappings    = 0x03;
  ZeroMem (&DirectMsgArgsEx, sizeof (DirectMsgArgsEx));
  /* Set the receiver service UUID */
  /* x4-x6 (i.e. Arg0-Arg2) should be 0 */
  DirectMsgArgsEx.Arg3 = 0xba7aff2eb1eac765;
  DirectMsgArgsEx.Arg4 = 0xb610b3a359f64054;
  DirectMsgArgsEx.Arg5 = NOTIFICATION_OPCODE_REGISTER;
  DirectMsgArgsEx.Arg6 = NumMappings;
  DEBUG ((DEBUG_INFO, "Registering %x Mappings:\n", NumMappings));
  Mapping.Bits.Cookie  = 0;
  Mapping.Bits.Id      = 5;
  DirectMsgArgsEx.Arg7 = Mapping.Uint64;
  DEBUG ((DEBUG_INFO, "Cookie: %x, Id: %x\n", Mapping.Bits.Cookie, Mapping.Bits.Id));
  DEBUG ((DEBUG_INFO, "Register Value: %lx\n", DirectMsgArgsEx.Arg7));
  Mapping.Bits.Cookie  = 1;
  Mapping.Bits.Id      = 6;
  DirectMsgArgsEx.Arg8 = Mapping.Uint64;
  DEBUG ((DEBUG_INFO, "Cookie: %x, Id: %x\n", Mapping.Bits.Cookie, Mapping.Bits.Id));
  DEBUG ((DEBUG_INFO, "Register Value: %lx\n", DirectMsgArgsEx.Arg8));
  Mapping.Bits.Cookie  = 2;
  Mapping.Bits.Id      = 7;
  DirectMsgArgsEx.Arg9 = Mapping.Uint64;
  DEBUG ((DEBUG_INFO, "Cookie: %x, Id: %x\n", Mapping.Bits.Cookie, Mapping.Bits.Id));
  DEBUG ((DEBUG_INFO, "Register Value: %lx\n", DirectMsgArgsEx.Arg9));
  Status = FfaMessageSendDirectReq2 (FfaTestPartInfo.PartitionId, &gEfiNotificationServiceFfaGuid, &DirectMsgArgsEx);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Unable to communicate direct req 2 with FF-A Ffa test SP (%r).\n", Status));
    goto Done;
  }

  ResponseVal = (INT8)DirectMsgArgsEx.Arg6;
  if (ResponseVal != NOTIFICATION_STATUS_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "Command Failed: %d\n", DirectMsgArgsEx.Arg6));
    goto Done;
  } else {
    DEBUG ((DEBUG_INFO, "Thermal Service Register Success\n"));
  }

  // Register the Thermal Service Notification Mapping Duplicate Cookie - Invalid
  Mapping.Uint64 = 0;
  NumMappings    = 0x01;
  ZeroMem (&DirectMsgArgsEx, sizeof (DirectMsgArgsEx));
  /* Set the receiver service UUID */
  /* x4-x6 (i.e. Arg0-Arg2) should be 0 */
  DirectMsgArgsEx.Arg3 = 0xba7aff2eb1eac765;
  DirectMsgArgsEx.Arg4 = 0xb610b3a359f64054;
  DirectMsgArgsEx.Arg5 = NOTIFICATION_OPCODE_REGISTER;
  DirectMsgArgsEx.Arg6 = NumMappings;
  DEBUG ((DEBUG_INFO, "Registering %x Mappings:\n", NumMappings));
  Mapping.Bits.Cookie  = 2; // Duplicate Cookie
  Mapping.Bits.Id      = 8; // Different ID
  DirectMsgArgsEx.Arg7 = Mapping.Uint64;
  DEBUG ((DEBUG_INFO, "Cookie: %x, Id: %x\n", Mapping.Bits.Cookie, Mapping.Bits.Id));
  DEBUG ((DEBUG_INFO, "Register Value: %lx\n", DirectMsgArgsEx.Arg7));
  Status = FfaMessageSendDirectReq2 (FfaTestPartInfo.PartitionId, &gEfiNotificationServiceFfaGuid, &DirectMsgArgsEx);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Unable to communicate direct req 2 with FF-A Ffa test SP (%r).\n", Status));
    goto Done;
  }

  ResponseVal = (INT8)DirectMsgArgsEx.Arg6;
  if (ResponseVal != NOTIFICATION_STATUS_INVALID_PARAMETER) {
    DEBUG ((DEBUG_ERROR, "Command Failed: %d\n", DirectMsgArgsEx.Arg6));
    goto Done;
  } else {
    DEBUG ((DEBUG_INFO, "Thermal Service Register Invalid Duplicate Cookie Success\n"));
  }

  // Register the Thermal Service Notification Mapping Duplicate ID - Invalid
  Mapping.Uint64 = 0;
  NumMappings    = 0x01;
  ZeroMem (&DirectMsgArgsEx, sizeof (DirectMsgArgsEx));
  /* Set the receiver service UUID */
  /* x4-x6 (i.e. Arg0-Arg2) should be 0 */
  DirectMsgArgsEx.Arg3 = 0xba7aff2eb1eac765;
  DirectMsgArgsEx.Arg4 = 0xb610b3a359f64054;
  DirectMsgArgsEx.Arg5 = NOTIFICATION_OPCODE_REGISTER;
  DirectMsgArgsEx.Arg6 = NumMappings;
  DEBUG ((DEBUG_INFO, "Registering %x Mappings:\n", NumMappings));
  Mapping.Bits.Cookie  = 3; // Different Cookie
  Mapping.Bits.Id      = 7; // Duplicate ID
  DirectMsgArgsEx.Arg7 = Mapping.Uint64;
  DEBUG ((DEBUG_INFO, "Cookie: %x, Id: %x\n", Mapping.Bits.Cookie, Mapping.Bits.Id));
  DEBUG ((DEBUG_INFO, "Register Value: %lx\n", DirectMsgArgsEx.Arg7));
  Status = FfaMessageSendDirectReq2 (FfaTestPartInfo.PartitionId, &gEfiNotificationServiceFfaGuid, &DirectMsgArgsEx);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Unable to communicate direct req 2 with FF-A Ffa test SP (%r).\n", Status));
    goto Done;
  }

  ResponseVal = (INT8)DirectMsgArgsEx.Arg6;
  if (ResponseVal != NOTIFICATION_STATUS_INVALID_PARAMETER) {
    DEBUG ((DEBUG_ERROR, "Command Failed: %d\n", DirectMsgArgsEx.Arg6));
    goto Done;
  } else {
    DEBUG ((DEBUG_INFO, "Thermal Service Register Invalid Duplicate ID Success\n"));
  }

  // Register the Thermal Service Notification Invalid Mapping Count Min Value
  Mapping.Uint64 = 0;
  ZeroMem (&DirectMsgArgsEx, sizeof (DirectMsgArgsEx));
  /* Set the receiver service UUID */
  /* x4-x6 (i.e. Arg0-Arg2) should be 0 */
  DirectMsgArgsEx.Arg3 = 0xba7aff2eb1eac765;
  DirectMsgArgsEx.Arg4 = 0xb610b3a359f64054;
  DirectMsgArgsEx.Arg5 = NOTIFICATION_OPCODE_REGISTER;
  DirectMsgArgsEx.Arg6 = 0x00; // Invalid
  Status               = FfaMessageSendDirectReq2 (FfaTestPartInfo.PartitionId, &gEfiNotificationServiceFfaGuid, &DirectMsgArgsEx);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Unable to communicate direct req 2 with FF-A Ffa test SP (%r).\n", Status));
    goto Done;
  }

  ResponseVal = (INT8)DirectMsgArgsEx.Arg6;
  if (ResponseVal != NOTIFICATION_STATUS_INVALID_PARAMETER) {
    DEBUG ((DEBUG_ERROR, "Command Failed: %d\n", DirectMsgArgsEx.Arg6));
    goto Done;
  } else {
    DEBUG ((DEBUG_INFO, "Thermal Service Register Invalid Mapping Count MIN Success\n"));
  }

  // Register the Thermal Service Notification Invalid Mapping Count Max Value
  Mapping.Uint64 = 0;
  ZeroMem (&DirectMsgArgsEx, sizeof (DirectMsgArgsEx));
  /* Set the receiver service UUID */
  /* x4-x6 (i.e. Arg0-Arg2) should be 0 */
  DirectMsgArgsEx.Arg3 = 0xba7aff2eb1eac765;
  DirectMsgArgsEx.Arg4 = 0xb610b3a359f64054;
  DirectMsgArgsEx.Arg5 = NOTIFICATION_OPCODE_REGISTER;
  DirectMsgArgsEx.Arg6 = 0x8; // Invalid
  Status               = FfaMessageSendDirectReq2 (FfaTestPartInfo.PartitionId, &gEfiNotificationServiceFfaGuid, &DirectMsgArgsEx);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Unable to communicate direct req 2 with FF-A Ffa test SP (%r).\n", Status));
    goto Done;
  }

  ResponseVal = (INT8)DirectMsgArgsEx.Arg6;
  if (ResponseVal != NOTIFICATION_STATUS_INVALID_PARAMETER) {
    DEBUG ((DEBUG_ERROR, "Command Failed: %d\n", DirectMsgArgsEx.Arg6));
    goto Done;
  } else {
    DEBUG ((DEBUG_INFO, "Thermal Service Register Invalid Mapping Count MAX Success\n"));
  }

  // Unregister the Thermal Service Notification Mapping Cookie1
  Mapping.Uint64 = 0;
  NumMappings    = 0x01;
  ZeroMem (&DirectMsgArgsEx, sizeof (DirectMsgArgsEx));
  /* Set the receiver service UUID */
  /* x4-x6 (i.e. Arg0-Arg2) should be 0 */
  DirectMsgArgsEx.Arg3 = 0xba7aff2eb1eac765;
  DirectMsgArgsEx.Arg4 = 0xb610b3a359f64054;
  DirectMsgArgsEx.Arg5 = NOTIFICATION_OPCODE_UNREGISTER;
  DirectMsgArgsEx.Arg6 = NumMappings;
  DEBUG ((DEBUG_INFO, "Unregistering %x Mappings:\n", NumMappings));
  Mapping.Bits.Cookie  = 1;
  Mapping.Bits.Id      = 6;
  DirectMsgArgsEx.Arg7 = Mapping.Uint64;
  DEBUG ((DEBUG_INFO, "Cookie: %x, Id: %x\n", Mapping.Bits.Cookie, Mapping.Bits.Id));
  DEBUG ((DEBUG_INFO, "Register Value: %lx\n", DirectMsgArgsEx.Arg7));
  Status = FfaMessageSendDirectReq2 (FfaTestPartInfo.PartitionId, &gEfiNotificationServiceFfaGuid, &DirectMsgArgsEx);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Unable to communicate direct req 2 with FF-A Ffa test SP (%r).\n", Status));
    goto Done;
  }

  ResponseVal = (INT8)DirectMsgArgsEx.Arg6;
  if (ResponseVal != NOTIFICATION_STATUS_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "Command Failed: %d\n", DirectMsgArgsEx.Arg6));
    goto Done;
  } else {
    DEBUG ((DEBUG_INFO, "Thermal Service Unregister Success\n"));
  }

  // Unregister the Thermal Service Notification Mapping Cookie1 - Invalid
  Mapping.Uint64 = 0;
  NumMappings    = 0x01;
  ZeroMem (&DirectMsgArgsEx, sizeof (DirectMsgArgsEx));
  /* Set the receiver service UUID */
  /* x4-x6 (i.e. Arg0-Arg2) should be 0 */
  DirectMsgArgsEx.Arg3 = 0xba7aff2eb1eac765;
  DirectMsgArgsEx.Arg4 = 0xb610b3a359f64054;
  DirectMsgArgsEx.Arg5 = NOTIFICATION_OPCODE_UNREGISTER;
  DirectMsgArgsEx.Arg6 = NumMappings;
  DEBUG ((DEBUG_INFO, "Unregistering %x Mappings:\n", NumMappings));
  Mapping.Bits.Cookie  = 1;
  Mapping.Bits.Id      = 6;
  DirectMsgArgsEx.Arg7 = Mapping.Uint64;
  DEBUG ((DEBUG_INFO, "Cookie: %x, Id: %x\n", Mapping.Bits.Cookie, Mapping.Bits.Id));
  DEBUG ((DEBUG_INFO, "Register Value: %lx\n", DirectMsgArgsEx.Arg7));
  Status = FfaMessageSendDirectReq2 (FfaTestPartInfo.PartitionId, &gEfiNotificationServiceFfaGuid, &DirectMsgArgsEx);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Unable to communicate direct req 2 with FF-A Ffa test SP (%r).\n", Status));
    goto Done;
  }

  ResponseVal = (INT8)DirectMsgArgsEx.Arg6;
  if (ResponseVal != NOTIFICATION_STATUS_INVALID_PARAMETER) {
    DEBUG ((DEBUG_ERROR, "Command Failed: %d\n", DirectMsgArgsEx.Arg6));
    goto Done;
  } else {
    DEBUG ((DEBUG_INFO, "Thermal Service Unregister Invalid Cookie Success\n"));
  }

  // Unregister the Thermal Service Notification Mapping Cookie/ID Mismatch - Invalid
  Mapping.Uint64 = 0;
  NumMappings    = 0x01;
  ZeroMem (&DirectMsgArgsEx, sizeof (DirectMsgArgsEx));
  /* Set the receiver service UUID */
  /* x4-x6 (i.e. Arg0-Arg2) should be 0 */
  DirectMsgArgsEx.Arg3 = 0xba7aff2eb1eac765;
  DirectMsgArgsEx.Arg4 = 0xb610b3a359f64054;
  DirectMsgArgsEx.Arg5 = NOTIFICATION_OPCODE_UNREGISTER;
  DirectMsgArgsEx.Arg6 = NumMappings;
  DEBUG ((DEBUG_INFO, "Unregistering %x Mappings:\n", NumMappings));
  Mapping.Bits.Cookie  = 0;
  Mapping.Bits.Id      = 0;
  DirectMsgArgsEx.Arg7 = Mapping.Uint64;
  DEBUG ((DEBUG_INFO, "Cookie: %x, Id: %x\n", Mapping.Bits.Cookie, Mapping.Bits.Id));
  DEBUG ((DEBUG_INFO, "Register Value: %lx\n", DirectMsgArgsEx.Arg7));
  Status = FfaMessageSendDirectReq2 (FfaTestPartInfo.PartitionId, &gEfiNotificationServiceFfaGuid, &DirectMsgArgsEx);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Unable to communicate direct req 2 with FF-A Ffa test SP (%r).\n", Status));
    goto Done;
  }

  ResponseVal = (INT8)DirectMsgArgsEx.Arg6;
  if (ResponseVal != NOTIFICATION_STATUS_INVALID_PARAMETER) {
    DEBUG ((DEBUG_ERROR, "Command Failed: %d\n", DirectMsgArgsEx.Arg6));
    goto Done;
  } else {
    DEBUG ((DEBUG_INFO, "Thermal Service Unregister Invalid Cookie/ID Mismatch Success\n"));
  }

  // Unregister Invalid Service UUID
  ZeroMem (&DirectMsgArgsEx, sizeof (DirectMsgArgsEx));
  /* Set the receiver service UUID */
  /* x4-x6 (i.e. Arg0-Arg2) should be 0 */
  /* Do not set UUID in x7-x8 (i.e. Arg3-Arg4) - Invalid */
  DirectMsgArgsEx.Arg5 = NOTIFICATION_OPCODE_UNREGISTER;
  Status               = FfaMessageSendDirectReq2 (FfaTestPartInfo.PartitionId, &gEfiNotificationServiceFfaGuid, &DirectMsgArgsEx);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Unable to communicate direct req 2 with FF-A Ffa test SP (%r).\n", Status));
    goto Done;
  }

  ResponseVal = (INT8)DirectMsgArgsEx.Arg6;
  if (ResponseVal != NOTIFICATION_STATUS_INVALID_PARAMETER) {
    DEBUG ((DEBUG_ERROR, "Command Failed: %d\n", DirectMsgArgsEx.Arg6));
    goto Done;
  } else {
    DEBUG ((DEBUG_INFO, "Unregister Invalid Service UUID Success\n"));
  }

  // Test a Notification Event
  ZeroMem (&DirectMsgArgsEx, sizeof (DirectMsgArgsEx));
  DirectMsgArgsEx.Arg0 = TEST_OPCODE_TEST_NOTIFICATION;
  DirectMsgArgsEx.Arg1 = 0xba7aff2eb1eac765;
  DirectMsgArgsEx.Arg2 = 0xb710b3a359f64054; // Battery Service
  /* IMPORTANT NOTE: Only bit 2 has been bound, test needs to match binding call */
  DirectMsgArgsEx.Arg3 = 0x02; // Cookie2 = ID2 = BitPos2
  Status               = FfaMessageSendDirectReq2 (FfaTestPartInfo.PartitionId, &gEfiTestServiceFfaGuid, &DirectMsgArgsEx);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Unable to communicate direct req 2 with FF-A Ffa test SP (%r).\n", Status));
    goto Done;
  }

  if (DirectMsgArgsEx.Arg0 != TEST_STATUS_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "Command Failed: %x\n", DirectMsgArgsEx.Arg0));
    goto Done;
  } else {
    DEBUG ((DEBUG_INFO, "Test Service Notification Event Success\n"));
  }

 #ifdef TPM2_ENABLE
  // Call the TPM Service get_interface_version
  ZeroMem (&DirectMsgArgsEx, sizeof (DirectMsgArgsEx));
  DirectMsgArgsEx.Arg0 = TPM2_FFA_GET_INTERFACE_VERSION;
  Status               = FfaMessageSendDirectReq2 (FfaTestPartInfo.PartitionId, &gEfiTpm2ServiceFfaGuid, &DirectMsgArgsEx);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Unable to communicate direct req 2 with FF-A Ffa test SP (%r).\n", Status));
    goto Done;
  }

  if (DirectMsgArgsEx.Arg0 != TPM2_FFA_SUCCESS_OK_RESULTS_RETURNED) {
    DEBUG ((DEBUG_ERROR, "Command Failed: %x\n", DirectMsgArgsEx.Arg0));
    goto Done;
  } else {
    DEBUG ((DEBUG_INFO, "TPM Service Interface Version: %d.%d\n", DirectMsgArgsEx.Arg1 >> 16, DirectMsgArgsEx.Arg1 & 0xFFFF));
  }

  // Call the TPM Service to close locality0
  ZeroMem (&DirectMsgArgsEx, sizeof (DirectMsgArgsEx));
  DirectMsgArgsEx.Arg0 = TPM2_FFA_START;
  DirectMsgArgsEx.Arg1 = 0x101; // Close Locality
  DirectMsgArgsEx.Arg2 = 0x00;  // Locality Qualifier
  Status               = FfaMessageSendDirectReq2 (FfaTestPartInfo.PartitionId, &gEfiTpm2ServiceFfaGuid, &DirectMsgArgsEx);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Unable to communicate direct req 2 with FF-A Ffa test SP (%r).\n", Status));
    goto Done;
  }

  if (DirectMsgArgsEx.Arg0 != TPM2_FFA_SUCCESS_OK) {
    DEBUG ((DEBUG_ERROR, "Command Failed: %x\n", DirectMsgArgsEx.Arg0));
    goto Done;
  } else {
    DEBUG ((DEBUG_INFO, "TPM Service Close Locality Success\n"));
  }

  // Call the TPM Service to request locality0
  ZeroMem (&DirectMsgArgsEx, sizeof (DirectMsgArgsEx));
  DirectMsgArgsEx.Arg0 = TPM2_FFA_START;
  DirectMsgArgsEx.Arg1 = TPM2_FFA_START_FUNC_QUALIFIER_LOCALITY;
  DirectMsgArgsEx.Arg2 = 0x00; // Locality Qualifier
  Status               = FfaMessageSendDirectReq2 (FfaTestPartInfo.PartitionId, &gEfiTpm2ServiceFfaGuid, &DirectMsgArgsEx);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Unable to communicate direct req 2 with FF-A Ffa test SP (%r).\n", Status));
    goto Done;
  }

  if (DirectMsgArgsEx.Arg0 != TPM2_FFA_ERROR_DENIED) {
    DEBUG ((DEBUG_ERROR, "Command Failed: %x\n", DirectMsgArgsEx.Arg0));
    goto Done;
  } else {
    DEBUG ((DEBUG_INFO, "TPM Service Rejected Request, Locality Closed\n"));
  }

  // Call the TPM Service to reopen locality0
  ZeroMem (&DirectMsgArgsEx, sizeof (DirectMsgArgsEx));
  DirectMsgArgsEx.Arg0 = TPM2_FFA_START;
  DirectMsgArgsEx.Arg1 = 0x100; // Open Locality
  DirectMsgArgsEx.Arg2 = 0x00;  // Locality Qualifier
  Status               = FfaMessageSendDirectReq2 (FfaTestPartInfo.PartitionId, &gEfiTpm2ServiceFfaGuid, &DirectMsgArgsEx);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Unable to communicate direct req 2 with FF-A Ffa test SP (%r).\n", Status));
    goto Done;
  }

  if (DirectMsgArgsEx.Arg0 != TPM2_FFA_SUCCESS_OK) {
    DEBUG ((DEBUG_ERROR, "Command Failed: %x\n", DirectMsgArgsEx.Arg0));
    goto Done;
  } else {
    DEBUG ((DEBUG_INFO, "TPM Service Open Locality Success\n"));
  }

 #endif

  return EFI_SUCCESS;

Done:
  return Status;
}
