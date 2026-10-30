# Secure Partition & Service Guide

## Secure Partitions Overview

Secure partitions are isolated environments which are used to instantiate management or security services
in a trusted environment. They are managed by a Secure Partition Manager (SPM) which ensures that each
partition operates independently and securely. A secure partition can have multiple services which it
manages. Secure partitions can be set to run at S-EL0 or S-EL1 with S-EL1 providing higher privileges.

## Secure Partitions Implementation

The QEMU Arm Virt platform packages Standalone MM (`stmm`) and the Rust services partition (`mssp-rust`) into TF-A's
Firmware Image Package (FIP). The previous C `MsSecurePartition` and its service libraries are no longer provided.

The following are the steps needed to create a secure partition:

1. Create a Rust crate under [SecurePartitions](../SecurePartitions), using
   [MsSecurePartitionRust](../SecurePartitions/MsSecurePartitionRust) as an example, and add it to the
   [Cargo workspace](../../Cargo.toml). To extend the existing partition instead, follow Services Implementation below.
2. Build the partition for `aarch64-unknown-none` and convert it to a raw `.bin` image as described in Rust Based Secure
   Partition below. Rust partitions are not EDK II drivers and do not require `.inf`, DSC, or FDF entries.
3. Generate a FIP package UUID for a new partition. This is separate from the service UUIDs advertised in the partition
   manifest. When replacing the existing Rust partition, retain its package UUID unless also updating TF-A's
   configuration.
4. Add a new partition entry to `GetSpLayoutData()` in the
   [platform build script][platform-build], using `mssp-rust` as a template.
   It supplies the image and partition manifest paths, their offsets, the package UUID, and the owner.
   The script generates `sp_layout.json` and passes it to TF-A through `SP_LAYOUT_FILE`.
   To replace the existing Rust partition, pass `MSSP_RUST_BIN_FILE=<absolute-path-to-bin>` and, if needed,
   `MSSP_RUST_DTS_FILE=<absolute-path-to-dts>` on the `stuart_build` command line instead of adding another entry.
5. Create the partition manifest in the [platform fdts directory][platform-fdts], using
   [the Rust manifest][rust-manifest] as an example.
   The current Rust partition runs at S-EL1 (`exception-level = <2>`), not S-EL0. Its load address is `0x0e700000`,
   image size is `0x400000`, and entrypoint offset is `0x2000`, matching the image offset in the SP layout and the
   `0x0e702000` origin in its [linker script](../SecurePartitions/MsSecurePartitionRust/linker/qemu-armvirt.ld).
   For a new partition, assign a distinct ID and boot order, and keep its memory and device mappings consistent with
   the linker script and SPMC configuration.
6. Update both TF-A configuration files for a new partition. The
   [trusted boot configuration][trusted-boot-config] is selected by
   `QEMU_TB_FW_CONFIG_DTS`. Its `secure-partitions` entries use the FIP package UUIDs from the SP layout:

   ```text
   secure-partitions {
    compatible = "arm,sp";
    stmm {
      uuid = "eaba83d8-baaf-4eaf-8144-f7fdcbe544a7";
      load-address = <0x0e400000>;
      owner = "Plat";
    };
    example_rust {
      uuid = "AFF0C73B-47E7-4A5B-AFFC-0052305A6520";
      load-address = <0x0e700000>;
      owner = "Plat";
    };
   };
   ```

   [The SPMC configuration][spmc-config] is selected by
   `QEMU_TOS_FW_CONFIG_DTS`. Add a corresponding `vmN` under `hypervisor` with `is_ffa_partition`, a load address,
   memory size, and vCPU count matching the partition manifest. Keep partition memory ranges non-overlapping and
   within the configured trusted memory.

7. [Build and run the firmware][platform-build-guide]; you should see Hafnium dispatching your
   secure partition. Use `HAF_TFA_BUILD=TRUE` to rebuild the FIP when changing manifests, package UUIDs, or the SP
   layout. `HAF_TFA_BUILD=FALSE` only patches images into a copy of the prebuilt FIP, using `fip_blob_manifest.json`
   from the selected `HAF_TFA_BINS` variant (`TPM` or `DEF`). It does not update manifests or add partitions, and
   replacement images must fit the existing allocation.

## Services Overview

A service which runs in a secure partition is designed to provide specific functionality similar to a
library. A service will have a UUID associated with it which is used to distinguish it from other
services that may be running within a secure partition. In FF-A, a DIRECT_REQ2 message will contain
the service UUID that the message is meant for in the ARG2 and ARG3 registers. This is how the secure
partition is able to figure out which service to pass along the message to.

## Services Implementation

The following are steps needed to create a service for a secure partition:

1. Create a Rust service crate under [SecurePartitions](../SecurePartitions), using
   [TestServiceLibRust](../SecurePartitions/TestServiceLibRust) as an example. Add it to the
   [Cargo workspace](../../Cargo.toml) and declare it as a dependency of the partition in its
   [Cargo.toml](../SecurePartitions/MsSecurePartitionRust/Cargo.toml).
2. Generate a UUID for your service and define its supported opcodes. Implement `ec_service_lib::Service`, supplying
   `UUID`, `NAME`, and the `ffa_msg_send_direct_req2` handler. See the
   [Test service implementation](../SecurePartitions/TestServiceLibRust/src/test_svc.rs) for an example using
   `MsgSendDirectReq2` and `MsgSendDirectResp2`.
3. Add the service UUID to the `uuid` list in the partition manifest, retaining the existing entries. Each UUID is
   represented by four little-endian 32-bit words of its UUID bytes. For example, the Test service UUID
   `e0fad9b3-7f5c-42c5-b2ee-b7a82313cdb2` is the second entry in the current Rust manifest:

   ```dts
   uuid = <0x7ed874e4 0x44403157 0x3ecb27a7 0xdfc8f38c>, <0xb3d9fae0 0xc5425c7f 0xa8b7eeb2 0xb2cd1323>, <0xa462b817 0xaf4f0618 0x9a08b386 0x61383558>;
   ```

4. Instantiate and initialize the service in the partition's
   [main.rs](../SecurePartitions/MsSecurePartitionRust/src/main.rs), then register it with
   `MessageHandler::new().append(...)` before `run_message_loop()`. The message handler routes DIRECT_REQ2 messages to
   the service by UUID; no C service table, `.inf`, or `.dec` registration is needed.
5. Rebuild the partition binary and integrate it using `MSSP_RUST_BIN_FILE`. Rebuild TF-A with `HAF_TFA_BUILD=TRUE`
   when changing the advertised service UUIDs in the manifest.

## Rust Based Secure Partition

This repo provides a Rust based secure partition implementation
[example](../SecurePartitions/MsSecurePartitionRust/Cargo.toml),
which can be used to host FF-A services.

The Rust based services use `ec-service-lib` and `odp-ffa` from `odp-secure-services`, with `aarch64-rt` providing the
S-EL1 entry point and exception handling.

Build instructions are in the [partition README](../SecurePartitions/MsSecurePartitionRust/README.md).
Run these commands from the [FF-A workspace root](../..), since the linker script paths are workspace-relative.
The platform build does not compile this Rust crate; by default it uses binaries from the `SECURE_PARTITION_BINARIES`
external dependency. `BLD_*_TPM2_ENABLE=TRUE` selects `msft-sp-virt-tpm.bin`; otherwise it selects `msft-sp-virt.bin`.
For a local build, use the `tpm` Cargo feature when TPM is enabled and pass the matching binary through
`MSSP_RUST_BIN_FILE`. Use `MSSP_RUST_DTS_FILE` when supplying a custom manifest as well.

[platform-build]: https://github.com/OpenDevicePartnership/patina-qemu/blob/main/Platforms/QemuArmVirtPkg/PlatformBuild.py
[platform-fdts]: https://github.com/OpenDevicePartnership/patina-qemu/tree/main/Platforms/QemuArmVirtPkg/fdts
[rust-manifest]: https://github.com/OpenDevicePartnership/patina-qemu/blob/main/Platforms/QemuArmVirtPkg/fdts/qemu_virt_mssp_rust_config.dts
[trusted-boot-config]: https://github.com/OpenDevicePartnership/patina-qemu/blob/main/Platforms/QemuArmVirtPkg/fdts/qemu_virt_tb_fw_config.dts
[spmc-config]: https://github.com/OpenDevicePartnership/patina-qemu/blob/main/Platforms/QemuArmVirtPkg/fdts/qemu_virt_spmc_sp_manifest.dts
[platform-build-guide]: https://github.com/OpenDevicePartnership/patina-qemu/blob/main/docs/src/building/building.md
