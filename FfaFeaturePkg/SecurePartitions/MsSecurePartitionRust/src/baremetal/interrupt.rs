use embassy_aarch64_haf::{haf_interrupt_handler_impl, HafInterruptHandler};

pub struct QemuInterriptHandler;

impl HafInterruptHandler for QemuInterriptHandler {
    fn handle(&self, haf_interrupt_id: hafnium::InterruptId) {
        log::info!("QEMU Interrupt: {:?}", haf_interrupt_id);
    }
}
haf_interrupt_handler_impl!(static IRQ_HANDLER: QemuInterriptHandler = QemuInterriptHandler);

// Vector table for EL1
#[unsafe(no_mangle)]
#[unsafe(link_section = ".vectors")]
static vector_table_el1: [unsafe extern "C" fn(); 16] = [
    current_el_sp0_sync,        // Current EL with SP0, Synchronous
    current_el_sp0_irq,         // Current EL with SP0, IRQ
    current_el_sp0_fiq,         // Current EL with SP0, FIQ
    current_el_sp0_serror,      // Current EL with SP0, SError
    
    current_el_spx_sync,        // Current EL with SPx, Synchronous
    current_el_spx_irq,         // Current EL with SPx, IRQ
    current_el_spx_fiq,         // Current EL with SPx, FIQ
    current_el_spx_serror,      // Current EL with SPx, SError
    
    lower_el_aarch64_sync,      // Lower EL using AArch64, Synchronous
    lower_el_aarch64_irq,       // Lower EL using AArch64, IRQ
    lower_el_aarch64_fiq,       // Lower EL using AArch64, FIQ
    lower_el_aarch64_serror,    // Lower EL using AArch64, SError
    
    lower_el_aarch32_sync,      // Lower EL using AArch32, Synchronous
    lower_el_aarch32_irq,       // Lower EL using AArch32, IRQ
    lower_el_aarch32_fiq,       // Lower EL using AArch32, FIQ
    lower_el_aarch32_serror,    // Lower EL using AArch32, SError
];

// Exception handler stubs
#[unsafe(no_mangle)]
unsafe extern "C" fn current_el_sp0_sync() {
    log::error!("current_el_sp0_sync exception");
    loop { }
}

#[unsafe(no_mangle)]
unsafe extern "C" fn current_el_sp0_irq() {
    log::error!("current_el_sp0_irq exception");
    loop { }
}

#[unsafe(no_mangle)]
unsafe extern "C" fn current_el_sp0_fiq() {
    log::error!("current_el_sp0_fiq exception");
    loop { }
}

#[unsafe(no_mangle)]
unsafe extern "C" fn current_el_sp0_serror() {
    log::error!("current_el_sp0_serror exception");
    loop { }
}

#[unsafe(no_mangle)]
unsafe extern "C" fn current_el_spx_sync() {
    log::error!("current_el_spx_sync exception");
    loop { }
}

#[unsafe(no_mangle)]
unsafe extern "C" fn current_el_spx_irq() {
    log::error!("current_el_spx_irq exception");
    loop { }
}

#[unsafe(no_mangle)]
unsafe extern "C" fn current_el_spx_fiq() {
    log::error!("current_el_spx_fiq exception");
    loop { }
}

#[unsafe(no_mangle)]
unsafe extern "C" fn current_el_spx_serror() {
    log::error!("current_el_spx_serror exception");
    loop { }
}

#[unsafe(no_mangle)]
unsafe extern "C" fn lower_el_aarch64_sync() {
    log::error!("lower_el_aarch64_sync exception");
    loop { }
}

#[unsafe(no_mangle)]
unsafe extern "C" fn lower_el_aarch64_irq() {
    log::error!("lower_el_aarch64_irq exception");
    loop { }
}

#[unsafe(no_mangle)]
unsafe extern "C" fn lower_el_aarch64_fiq() {
    log::error!("lower_el_aarch64_fiq exception");
    loop { }
}

#[unsafe(no_mangle)]
unsafe extern "C" fn lower_el_aarch64_serror() {
    log::error!("lower_el_aarch64_serror exception");
    loop { }
}

#[unsafe(no_mangle)]
unsafe extern "C" fn lower_el_aarch32_sync() {
    log::error!("lower_el_aarch32_sync exception");
    loop { }
}

#[unsafe(no_mangle)]
unsafe extern "C" fn lower_el_aarch32_irq() {
    log::error!("lower_el_aarch32_irq exception");
    loop { }
}

#[unsafe(no_mangle)]
unsafe extern "C" fn lower_el_aarch32_fiq() {
    log::error!("lower_el_aarch32_fiq exception");
    loop { }
}

#[unsafe(no_mangle)]
unsafe extern "C" fn lower_el_aarch32_serror() {
    log::error!("lower_el_aarch32_serror exception");
    loop { }
}
