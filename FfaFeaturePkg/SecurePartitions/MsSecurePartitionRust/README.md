# QEMU Inter-Partition Secure Partition

An implementation of Inter-Partition secure partition based on QEMU haf-ec-service.

- `aarch64-rt` for the entry point and exception handling.

## Cargo features

- `tpm` — enable the TPM service and use the TPM CRB addresses for the selected target.

The four supported combinations produce one binary each: TPM-on / TPM-off.

## Building

Default build (ARM Virt, no TPM):

```bash
cargo build --target=aarch64-unknown-none
cargo objcopy --target=aarch64-unknown-none -- -O binary target/aarch64-unknown-none/debug/msft-sp-virt.bin
```

Other variants — add the relevant features:

```bash
cargo build --target=aarch64-unknown-none --features tpm
cargo objcopy --target=aarch64-unknown-none --features tpm -- -O binary target/aarch64-unknown-none/debug/msft-sp-virt-tpm.bin
```
