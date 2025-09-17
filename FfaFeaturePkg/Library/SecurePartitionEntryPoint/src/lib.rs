#![cfg_attr(target_os = "none", no_std)]
#![cfg_attr(target_os = "none", no_main)]

use core::arch::global_asm;

global_asm!(include_str!("entry.S"));

#[unsafe(no_mangle)]
extern "C" fn rust_entry(arg0: u64, arg1: u64, arg2: u64, arg3: u64) -> ! {
    __main(arg0, arg1, arg2, arg3)
}

unsafe extern "Rust" {
    /// Main function provided by the application using the `main!` macro.
    safe fn __main(arg0: u64, arg1: u64, arg2: u64, arg3: u64) -> !;
}

/// Marks the main function of the binary and reserves space for the boot stack.
///
/// Example:
///
/// ```rust
/// use aarch64_rt::entry;
///
/// entry!(main);
/// fn main() -> ! {
///     info!("Hello world");
/// }
/// ```
///
/// 40 pages (160 KiB) is reserved for the boot stack by default; a different size may be configured
/// by passing the number of pages as a second argument to the macro, e.g. `entry!(main, 10);` to
/// reserve only 10 pages.
#[macro_export]
macro_rules! entry {
    ($name:path) => {
        entry!($name, 40);
    };
    ($name:path, $boot_stack_pages:expr) => {
        #[unsafe(export_name = "boot_stack")]
        #[unsafe(link_section = ".stack.boot_stack")]
        static mut __BOOT_STACK: $crate::Stack<$boot_stack_pages> = $crate::Stack::new();

        // Export a symbol with a name matching the extern declaration above.
        #[unsafe(export_name = "__main")]
        fn __main(arg0: u64, arg1: u64, arg2: u64, arg3: u64) -> ! {
            // Ensure that the main function provided by the application has the correct type.
            $name(arg0, arg1, arg2, arg3)
        }
    };
}

/// A stack for some CPU core.
///
/// This is used by the [`entry!`] macro to reserve space for the boot stack.
#[repr(C, align(4096))]
pub struct Stack<const NUM_PAGES: usize>([StackPage; NUM_PAGES]);

impl<const NUM_PAGES: usize> Stack<NUM_PAGES> {
    /// Creates a new zero-initialised stack.
    pub const fn new() -> Self {
        Self([const { StackPage::new() }; NUM_PAGES])
    }
}

impl<const NUM_PAGES: usize> Default for Stack<NUM_PAGES> {
    fn default() -> Self {
        Self::new()
    }
}

#[repr(C, align(4096))]
struct StackPage([u8; 4096]);

impl StackPage {
    const fn new() -> Self {
        Self([0; 4096])
    }
}
