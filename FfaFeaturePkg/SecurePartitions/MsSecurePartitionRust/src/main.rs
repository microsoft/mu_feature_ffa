// This project is dual-licensed under Apache 2.0 and MIT terms.
// See LICENSE-APACHE and LICENSE-MIT for details.

#![cfg_attr(target_os = "none", no_std)]
#![cfg_attr(target_os = "none", no_main)]
#![deny(clippy::undocumented_unsafe_blocks)]
#![deny(unsafe_op_in_unsafe_fn)]

#[cfg(target_os = "none")]
mod baremetal;

#[cfg(not(target_os = "none"))]
fn main() {
    println!("qemu-sp stub");
}

#[cfg(target_os = "none")]
fn main() -> ! {
    use ec_service_lib::MessageHandler;
    #[cfg(feature = "tpm")]
    use ec_service_lib::services::{TpmService, TpmSst};
    #[cfg(not(feature = "tpm"))]
    use ec_service_lib::services::TpmServiceStub;
    use test_service_lib::test_svc::Test;
    use odp_ffa::Function;

    log::info!("QEMU Secure Partition - build time: {}", env!("BUILD_TIME"));
    let version = odp_ffa::Version::new().exec().unwrap();
    log::info!("FFA version: {}.{}", version.major(), version.minor());

    #[cfg(feature = "tpm")]
    let tpm_service = {
        // Initialize the TPM service with its state-translation backend.
        let mut svc = TpmService::new(TpmSst::new());

        // SAFETY: Writes to the memory-mapped internal CRB regions and initializes
        //         the SST layer for the external TPM device.
        unsafe { svc.init(0x10000200000) };
        svc
    };
    #[cfg(not(feature = "tpm"))]
    let tpm_service = TpmServiceStub::new();

    MessageHandler::new()
        .append(ec_service_lib::services::FwMgmt::new())
        .append(ec_service_lib::services::Notify::new())
        .append(tpm_service)
        .append(Test::new())
        .run_message_loop()
        .expect("Error in run_message_loop");

    unreachable!()
}
