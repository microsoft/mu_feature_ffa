# QEMU Inter-Partition Secure Partition

An implementation of Inter-Partition secure partition based on QEMU haf-ec-service.

- `aarch64-rt` for the entry point and exception handling.

## Building

```bash
cargo build --target=aarch64-unknown-none
cargo objcopy --target=aarch64-unknown-none -- -O binary target/aarch64-unknown-none/debug/msft-sp.bin
```
