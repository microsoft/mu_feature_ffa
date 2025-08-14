// Copyright 2025 Microsoft Corporation
// This project is dual-licensed under Apache 2.0 and MIT terms.
// See LICENSE-APACHE and LICENSE-MIT for details.

fn main() {
    if std::env::var("CARGO_CFG_TARGET_OS").unwrap() == "none" {
        println!("cargo:rustc-env=BUILD_TIME={}", chrono::Utc::now().to_rfc3339());
        println!("cargo:rustc-link-arg=-TFfaFeaturePkg/SecurePartitions/MsSecurePartitionRust/linker/image.ld");
        println!("cargo:rustc-link-arg=-TFfaFeaturePkg/SecurePartitions/MsSecurePartitionRust/linker/qemu.ld");
        println!("cargo:rerun-if-changed=FfaFeaturePkg/SecurePartitions/MsSecurePartitionRust/linker/qemu.ld");
        println!("cargo:rerun-if-changed=FfaFeaturePkg/SecurePartitions/MsSecurePartitionRust/linker/image.ld");
    }
}
