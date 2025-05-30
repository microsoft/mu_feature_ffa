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
// #include <Library/ArmFfaLibEx.h>
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
#define UNIT_TEST_APP_VERSION  "0.1"

typedef struct {
  BOOLEAN  IsMmCommunicationServiceAvailable;
  BOOLEAN  IsTestServiceAvailable;
  BOOLEAN  IsTpm2ServiceAvailable;
  BOOLEAN  IsNotificationServiceAvailable;
  // Pending
  UINTN   SriIndex;
} FFA_TEST_CONTEXT;

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
  This routine retrieves the partition information of the Ffa test SP and prints it.
**/
UNIT_TEST_STATUS
EFIAPI
FfaMiscGetPartitionInfoRegs (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UNIT_TEST_STATUS          utStatus = UNIT_TEST_RUNNING;
  EFI_STATUS                Status   = EFI_SUCCESS;
  EFI_FFA_PART_INFO_DESC    FfaPartInfo;
  ARM_SMC_ARGS              SmcArgs;
  UINT32                    Count;
  UINT32                    Size;
  UINTN                     Index;
  EFI_GUID                  GuidsOfInterest[] = {
    gEfiMmCommunication2ProtocolGuid,
    gEfiTestServiceFfaGuid,
    gEfiTpm2ServiceFfaGuid
  };

  DEBUG ((DEBUG_INFO, "%a: enter...\n", __func__));

  // Given the complexity of potentially having multiple partitions, we will
  // just try to rerieve the partition information of the STMM SP, Test SP and TPM
  // SP. The non-STMM SP availability will be checked in the next test case.

  for (Index = 0; Index < ARRAY_SIZE (GuidsOfInterest); Index++) {
    // Get the partition information of the STMM SP
    ZeroMem (&SmcArgs, sizeof (SmcArgs));
    Count = 1; // We expect only one partition info
    Status = FfaPartitionInfoGetRegs (&GuidsOfInterest[Index], 0, NULL, &Count, (EFI_FFA_PART_INFO_DESC *)&SmcArgs.Arg3);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to get FF-A partition info. Status: %r\n", Status));
      if (CompareGuid (&GuidsOfInterest[Index], &gEfiMmCommunication2ProtocolGuid) == 0) {
        // If we are querying the MM Communication protocol, we need to bail.
        DEBUG ((DEBUG_INFO, "%a MM Communication protocol not found, fatal error.\n", __func__));
        UT_ASSERT_NOT_EFI_ERROR (Status);
        continue;
      } else if (CompareGuid (&GuidsOfInterest[Index], &gEfiTestServiceFfaGuid) == 0) {
        // If we are querying the Test Service, we can skip it.
        DEBUG ((DEBUG_INFO, "%a Test Service not found, skipping.\n", __func__));
        UT_LOG_WARNING ("Test Service not found, skipping.");
        continue;
      } else if (CompareGuid (&GuidsOfInterest[Index], &gEfiTpm2ServiceFfaGuid) == 0) {
        // If we are querying the TPM Service, we can skip it.
        DEBUG ((DEBUG_INFO, "%a TPM Service not found, skipping.\n", __func__));
        UT_LOG_WARNING ("TPM Service not found, skipping.");
        continue;
      }
    }

    CopyMem (&FfaPartInfo, &SmcArgs.Arg3, sizeof (EFI_FFA_PART_INFO_DESC));

    DEBUG ((DEBUG_INFO, "FF-A STMM Partition Info:\n"));
    DEBUG ((
      DEBUG_INFO,
      "\tID = 0x%lx, Execution contexts = %d, Properties = 0x%lx.\n",
      FfaPartInfo.PartitionId,
      FfaPartInfo.ExecContextCountOrProxyPartitionId,
      FfaPartInfo.PartitionProps
      ));

    DEBUG ((
      DEBUG_INFO,
      "\tSP Guid = %g.\n",
      FfaPartInfo.PartitionUuid
    ));
    UT_ASSERT_MEM_EQUAL (
      &FfaPartInfo.PartitionUuid,
      &GuidsOfInterest[Index],
      sizeof (EFI_GUID)
      );
  }

Done:
  utStatus = UNIT_TEST_PASSED;
  return utStatus;
}

/**
  This routine retrieves the partition information of Ffa SPs through Rx/Tx buffer
  and prints them.
**/
UNIT_TEST_STATUS
EFIAPI
FfaMiscGetPartitionInfo (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UNIT_TEST_STATUS          utStatus = UNIT_TEST_RUNNING;
  EFI_STATUS                Status   = EFI_SUCCESS;
  EFI_FFA_PART_INFO_DESC    FfaPartInfo;
  UINT32                    Count;
  UINT32                    Size;

  // Discover the Ffa test SP after converting the EFI_GUID to a format TF-A will
  // understand.
  Status = ArmFfaLibPartitionInfoGet (&gEfiMmCommunication2ProtocolGuid, 0, &Count, &Size);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Unable to discover FF-A test SP (%r).\n", Status));
    goto Done;
  }

  // Retrieve the partition information from the returned registers
  CopyMem (&FfaPartInfo, (VOID *)PcdGet64 (PcdFfaRxBuffer), sizeof (EFI_FFA_PART_INFO_DESC));

  DEBUG ((DEBUG_INFO, "Discovered FF-A test SP.\n"));
  DEBUG ((
    DEBUG_INFO,
    "\tID = 0x%lx, Execution contexts = %d, Properties = 0x%lx.\n",
    FfaPartInfo.PartitionId,
    FfaPartInfo.ExecContextCountOrProxyPartitionId,
    FfaPartInfo.PartitionProps
    ));
  DEBUG ((
    DEBUG_INFO,
    "\tSP Guid = %g.\n",
    FfaPartInfo.PartitionUuid
    ));
  UT_ASSERT_MEM_EQUAL (
    &FfaPartInfo.PartitionUuid,
    &gEfiTestServiceFfaGuid,
    sizeof (EFI_GUID)
    );
}

/**
  This routine sets up the notifications for the Ffa test SP and verifies
  that the notifications are working correctly.
**/
UNIT_TEST_STATUS
EFIAPI
FfaMiscSetupNotifications (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UNIT_TEST_STATUS  utStatus = UNIT_TEST_RUNNING;
  EFI_STATUS        Status   = EFI_SUCCESS;
  UINT64            Bitmap;

  DEBUG ((DEBUG_INFO, "%a: enter...\n", __func__));

  //
  // Should be able to handle notification flow now.
  //
  // Now register UEFI to receive notifications by creating notification bitmaps
  Status = FfaNotificationBitmapCreate (1);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Unable to create notification bitmap with FF-A Ffa test SP (%r).\n", Status));
    goto Done;
  }

  // Then register this test app to receive notifications from the Ffa test SP
  BindBitPos = 0x02;
  FfaNotificationBind (FfaTestPartInfo.PartitionId, 0, (1 << BindBitPos));
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Unable to bind notification with FF-A Ffa test SP (%r).\n", Status));
    goto Done;
  }

  DEBUG ((DEBUG_INFO, "Binding Bit%x - Value: %x Successful.\n", BindBitPos, (1 << BindBitPos)));

Done:
  utStatus = UNIT_TEST_PASSED;
  return utStatus;
}

/**
  This routine registers the notifications for the Ffa test SP and verifies
  that the notifications are working correctly.
**/
UNIT_TEST_STATUS
EFIAPI
FfaMiscRegisterNotifications (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UNIT_TEST_STATUS  utStatus = UNIT_TEST_RUNNING;
  EFI_STATUS        Status   = EFI_SUCCESS;
  UINT64            Bitmap;
  UINTN             SriIndex;
  UINTN             Dummy;
  EFI_HARDWARE_INTERRUPT_PROTOCOL  *gInterrupt;

  DEBUG ((DEBUG_INFO, "%a: enter...\n", __func__));

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

  DEBUG ((DEBUG_INFO, "Registered notification with FF-A Ffa test SP with VM bitmap %x.\n", Bitmap));

Done:
  utStatus = UNIT_TEST_PASSED;
  return utStatus;
}

/**
  This routine tests the inter-partition communication with the Ffa test SP.
  It sends a message to the Ffa test SP and waits for a response.
**/
UNIT_TEST_STATUS
EFIAPI
FfaMiscTestInterPartitionNormal (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UNIT_TEST_STATUS  utStatus = UNIT_TEST_RUNNING;
  EFI_STATUS        Status   = EFI_SUCCESS;
  DIRECT_MSG_ARGS_EX DirectMsgArgsEx;

  DEBUG ((DEBUG_INFO, "%a: enter...\n", __func__));

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

Done:
  utStatus = UNIT_TEST_PASSED;
  return utStatus;
}

/**
  This routine tests the inter-partition communication with the Ffa test SP
  in a secondary service.
**/
UNIT_TEST_STATUS
EFIAPI
FfaMiscTestInterPartitionSecondary (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UNIT_TEST_STATUS  utStatus = UNIT_TEST_RUNNING;
  EFI_STATUS        Status   = EFI_SUCCESS;
  DIRECT_MSG_ARGS_EX DirectMsgArgsEx;

  DEBUG ((DEBUG_INFO, "%a: enter...\n", __func__));


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
}

/**
  This routine tests the inter-partition communication with the Ffa test SP
  with a duplicate cookie.
**/
UNIT_TEST_STATUS
EFIAPI
FfaMiscTestInterPartitionDuplicateCookie (
  IN UNIT_TEST_CONTEXT  Context
  )
{

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
}

/**
  This routine tests the inter-partition communication with the Ffa test SP
  with an invalid duplicate ID.
**/
UNIT_TEST_STATUS
EFIAPI
FfaMiscTestInterPartitionInvalidDuplicateId (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  

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
}

/**
  This routine tests the inter-partition communication with the Ffa test SP
  with an invalid mapping count minimum value.
**/
UNIT_TEST_STATUS
EFIAPI
FfaMiscTestInterPartitionInvalidMappingCountMin (
  IN UNIT_TEST_CONTEXT  Context
  )
{
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
}

/**
  This routine tests the inter-partition communication with the Ffa test SP
  with an invalid mapping count maximum value.
**/
UNIT_TEST_STATUS
EFIAPI
FfaMiscTestInterPartitionInvalidMappingCountMax (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  
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
}

/**
  This routine tests the inter-partition communication with the Ffa test SP
  and unregisters the notifications.
**/
UNIT_TEST_STATUS
EFIAPI
FfaMiscTestInterPartitionUnregisterNormal (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  
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
}

/**
  This routine tests the inter-partition communication with the Ffa test SP
  and unregisters the notifications with an invalid cookie.
**/
UNIT_TEST_STATUS
EFIAPI
FfaMiscTestInterPartitionUnregisterInvalidCookie (
  IN UNIT_TEST_CONTEXT  Context
  )
{
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
}

/**
  This routine tests the inter-partition communication with the Ffa test SP
  and unregisters the notifications with an invalid ID.
**/
UNIT_TEST_STATUS
EFIAPI
FfaMiscTestInterPartitionUnregisterInvalidServiceId (
  IN UNIT_TEST_CONTEXT  Context
  )
{
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
}

/**
  This routine tests the inter-partition communication with the Ffa test SP
  and unregisters the notifications with an invalid UUID.
**/
UNIT_TEST_STATUS
EFIAPI
FfaMiscTestInterPartitionUnregisterInvalidUuid (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  
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
}

/**
  This routine tests the notification event with the Ffa test SP.
**/
UNIT_TEST_STATUS
EFIAPI
FfaMiscTestNotificationEvent (
  IN UNIT_TEST_CONTEXT  Context
  )
{
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
}

/**
  This routine tests the TPM version retrieval with the Ffa test SP.
**/
UNIT_TEST_STATUS
EFIAPI
FfaMiscTestTpmGetVersion (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UNIT_TEST_STATUS  utStatus = UNIT_TEST_RUNNING;
  EFI_STATUS        Status   = EFI_SUCCESS;

  DEBUG ((DEBUG_INFO, "%a: enter...\n", __func__));

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

Done:
  utStatus = UNIT_TEST_PASSED;
  return utStatus;
}

/**
  This routine tests the TPM Service to close locality with the Ffa test SP.
**/
UNIT_TEST_STATUS
EFIAPI
FfaMiscTestTpmCloseLocality (
  IN UNIT_TEST_CONTEXT  Context
  )
{

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
}

/**
  This routine tests the TPM Service to request locality with the Ffa test SP.
**/
UNIT_TEST_STATUS
EFIAPI
FfaMiscTestTpmRequestLocality (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  



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
}

/**
  This routine tests the TPM Service to reopen locality with the Ffa test SP.
**/
UNIT_TEST_STATUS
EFIAPI
FfaMiscTestTpmReopenLocality (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  

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
  UNIT_TEST_FRAMEWORK_HANDLE  Fw            = NULL;
  UNIT_TEST_SUITE_HANDLE      Misc          = NULL;
  FFA_TEST_CONTEXT           FfaTestContext = {0};

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

  AddTestCase (Misc, "Verify FF-A framework version", "Ffa.Miscellaneous.VerifyVersion",
    FfaMiscVerifyVersion, NULL, NULL, &FfaTestContext);

  AddTestCase (Misc, "Verify Partition Info via registers", "Ffa.Miscellaneous.VerifyPartitionInfoRegs",
    FfaMiscGetPartitionInfoRegs, NULL, NULL, &FfaTestContext);

  AddTestCase (Misc, "Verify Partition Info via Rx/Tx buffers", "Ffa.Miscellaneous.VerifyPartitionInfo",
    FfaMiscGetPartitionInfo, NULL, NULL, &FfaTestContext);

  AddTestCase (Misc, "Verify FF-A Ffa test SP notifications", "Ffa.Miscellaneous.SetupNotifications",
    FfaMiscSetupNotifications, NULL, NULL, &FfaTestContext);

  AddTestCase (Misc, "Verify FF-A Ffa test SP notifications", "Ffa.Miscellaneous.RegisterNotifications",
    FfaMiscRegisterNotifications, NULL, NULL, &FfaTestContext);

  //
  // Test interpartition communication with the Ffa test SP.
  // These tests will only run if the Ffa test SP is available.
  //
  AddTestCase (Misc, "Verify Ffa Inter Partition", "Ffa.Miscellaneous.FfaTestInterPartitionNormal",
    FfaMiscTestInterPartitionNormal, NULL, NULL, &FfaTestContext);

  AddTestCase (Misc, "Verify Ffa Inter Partition", "Ffa.Miscellaneous.FfaTestInterPartitionSecondary",
    FfaMiscTestInterPartitionSecondary, NULL, NULL, &FfaTestContext);

  AddTestCase (Misc, "Verify Ffa Inter Partition", "Ffa.Miscellaneous.FfaTestInterPartitionDuplicateCookie",
    FfaMiscTestInterPartitionDuplicateCookie, NULL, NULL, &FfaTestContext);

  AddTestCase (Misc, "Verify Ffa Inter Partition", "Ffa.Miscellaneous.FfaTestInterPartitionInvalidDuplicateId",
    FfaMiscTestInterPartitionInvalidDuplicateId, NULL, NULL, &FfaTestContext);

  AddTestCase (Misc, "Verify Ffa Inter Partition", "Ffa.Miscellaneous.FfaTestInterPartitionInvalidMappingCountMin",
    FfaMiscTestInterPartitionInvalidMappingCountMin, NULL, NULL, &FfaTestContext);

  AddTestCase (Misc, "Verify Ffa Inter Partition", "Ffa.Miscellaneous.FfaTestInterPartitionInvalidMappingCountMax",
    FfaMiscTestInterPartitionInvalidMappingCountMax, NULL, NULL, &FfaTestContext);

  AddTestCase (Misc, "Verify Ffa Inter Partition", "Ffa.Miscellaneous.FfaTestInterPartitionUnregisterNormal",
    FfaMiscTestInterPartitionUnregisterNormal, NULL, NULL, &FfaTestContext);

  AddTestCase (Misc, "Verify Ffa Inter Partition", "Ffa.Miscellaneous.FfaTestInterPartitionUnregisterInvalidCookie",
    FfaMiscTestInterPartitionUnregisterInvalidCookie, NULL, NULL, &FfaTestContext);

  AddTestCase (Misc, "Verify Ffa Inter Partition", "Ffa.Miscellaneous.FfaTestInterPartitionUnregisterInvalidId",
    FfaMiscTestInterPartitionUnregisterInvalidId, NULL, NULL, &FfaTestContext);

  AddTestCase (Misc, "Verify Ffa Inter Partition", "Ffa.Miscellaneous.FfaTestInterPartitionUnregisterInvalidUuid",
    FfaMiscTestInterPartitionUnregisterInvalidUuid, NULL, NULL, &FfaTestContext);

  AddTestCase (Misc, "Verify Ffa Notification Event", "Ffa.Miscellaneous.FfaTestNotificationEvent",
    FfaMiscTestNotificationEvent, NULL, NULL, &FfaTestContext);

  // Test the TPM Service over Ffa interface.
  AddTestCase (Misc, "Verify Ffa TPM Service", "Ffa.Miscellaneous.FfaTestTpmGetVersion",
    FfaMiscTestTpmGetVersion, NULL, NULL, &FfaTestContext);

  AddTestCase (Misc, "Verify Ffa TPM Service", "Ffa.Miscellaneous.FfaTestTpmCloseLocality",
    FfaMiscTestTpmCloseLocality, NULL, NULL, &FfaTestContext);

  AddTestCase (Misc, "Verify Ffa TPM Service", "Ffa.Miscellaneous.FfaTestTpmRequestLocality",
    FfaMiscTestTpmRequestLocality, NULL, NULL, &FfaTestContext);

  AddTestCase (Misc, "Verify Ffa TPM Service", "Ffa.Miscellaneous.FfaTestTpmReopenLocality",
    FfaMiscTestTpmReopenLocality, NULL, NULL, &FfaTestContext);

  //
  // Execute the tests.
  //
  Status = RunAllTestSuites (Fw);

Done:
  if (Fw != NULL) {
    FreeUnitTestFramework (Fw);
  }

  DEBUG ((DEBUG_INFO, "%a exit - %r\n", __FUNCTION__, Status));
  return Status;
}
