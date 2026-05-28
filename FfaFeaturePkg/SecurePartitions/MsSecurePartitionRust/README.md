# QEMU Inter-Partition Secure Partition

An implementation of Inter-Partition secure partition based on QEMU haf-ec-service.

- `aarch64-rt` for the entry point and exception handling.

## Cargo features

- `armvirt` — target QEMU `arm-virt` instead of the default `sbsa-ref` (selects the
  matching `linker/qemu-*.ld` MEMORY layout and the platform's TPM CRB addresses).
- `tpm` — enable the TPM service.

The four supported combinations produce one binary each: SBSA / arm-virt × TPM-on / TPM-off.

## Building

Default build (SBSA-ref, no TPM):

```bash
cargo build --target=aarch64-unknown-none
cargo objcopy --target=aarch64-unknown-none -- -O binary target/aarch64-unknown-none/debug/msft-sp.bin
```

Other variants — add the relevant features:

```bash
cargo build --target=aarch64-unknown-none --features tpm              # sbsa + tpm
cargo build --target=aarch64-unknown-none --features armvirt          # arm-virt
cargo build --target=aarch64-unknown-none --features "armvirt tpm"    # arm-virt + tpm
```
