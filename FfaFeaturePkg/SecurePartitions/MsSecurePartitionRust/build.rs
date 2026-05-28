// Copyright 2025 Microsoft Corporation
// This project is dual-licensed under Apache 2.0 and MIT terms.
// See LICENSE-APACHE and LICENSE-MIT for details.

fn main() {
    if std::env::var("CARGO_CFG_TARGET_OS").unwrap() == "none" {
        // Platform-specific MEMORY layout. Default is QEMU SBSA-ref; the
        // `armvirt` feature switches to QEMU arm-virt.
        let qemu_linker = if std::env::var("CARGO_FEATURE_ARMVIRT").is_ok() {
            "FfaFeaturePkg/SecurePartitions/MsSecurePartitionRust/linker/qemu-armvirt.ld"
        } else {
            "FfaFeaturePkg/SecurePartitions/MsSecurePartitionRust/linker/qemu-sbsa.ld"
        };

        println!("cargo:rustc-env=BUILD_TIME={}", chrono::Utc::now().to_rfc3339());
        println!("cargo:rustc-link-arg=-TFfaFeaturePkg/SecurePartitions/MsSecurePartitionRust/linker/image.ld");
        println!("cargo:rustc-link-arg=-T{qemu_linker}");
        println!("cargo:rerun-if-changed={qemu_linker}");
        println!("cargo:rerun-if-changed=FfaFeaturePkg/SecurePartitions/MsSecurePartitionRust/linker/image.ld");
        println!("cargo:rerun-if-env-changed=CARGO_FEATURE_ARMVIRT");
    }
}
