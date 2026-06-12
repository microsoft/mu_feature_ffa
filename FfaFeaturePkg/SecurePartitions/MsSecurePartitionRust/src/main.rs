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
        // Non-secure CRB region shared between non-secure world and secure world.
        // Secure CRB region only accessible by the TPM service.
        let (tpm_internal_crb_address, tpm_external_crb_address): (u64, u64) =
            (0x40200000, 0x0c000000);
        log::info!("TPM Internal CRB Address: {:X}", tpm_internal_crb_address);
        log::info!("TPM External CRB Address: {:X}", tpm_external_crb_address);
        // Initialize the TPM service with its state-translation backend.
        let mut svc = TpmService::new(TpmSst::new());

        // SAFETY: Writes to the memory-mapped internal CRB regions and initializes
        //         the SST layer for the external TPM device.
        unsafe { svc.init(tpm_internal_crb_address, tpm_external_crb_address) };
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
