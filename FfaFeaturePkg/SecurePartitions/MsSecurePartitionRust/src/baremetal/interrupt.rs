use aarch64_haf::{haf_interrupt_handler_impl, HafInterruptHandler};

pub struct QemuInterruptHandler;

impl HafInterruptHandler for QemuInterruptHandler {
    fn handle(&self, haf_interrupt_id: hafnium::InterruptId) {
        log::info!("QEMU Interrupt: {:?}", haf_interrupt_id);
    }
}
haf_interrupt_handler_impl!(static IRQ_HANDLER: QemuInterruptHandler = QemuInterruptHandler);
