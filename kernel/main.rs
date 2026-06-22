#![no_std]
#![no_main]

mod panic;

pub mod fdt;
pub mod drivers {
    pub mod uart;
    pub mod clint;
    pub mod plic;
    pub mod virtio {
        pub mod block;
        pub mod net;
    }
}
pub mod mm {
    pub mod bump;
    pub mod page;
    pub mod map;
}
pub mod proc {
    pub mod task;
    pub mod sched;
    pub mod context;
}
pub mod fs {
    pub mod slipfs;
}
pub mod shell {
    pub mod slip;
}

extern crate riscv;

use riscv::register::sstatus;

pub use fdt::*;
pub use drivers::uart::*;
pub use drivers::clint::*;
pub use drivers::plic::*;
pub use drivers::virtio::block::*;
pub use mm::bump::*;
pub use mm::page::*;
pub use mm::map::*;
pub use proc::sched::*;
pub use fs::slipfs::*;
pub use shell::slip::*;

#[unsafe(no_mangle)]
pub extern "C" fn kernel_main(_hart_id: usize, fdt_addr: usize) -> ! {
    unsafe { sstatus::set_spp(sstatus::SPP::Supervisor) }

    uart_init();
    print_log("BOOT", "SlipperOS v0.1 booting...");

    print_seal();

    let (uart_irq, virtio_base, virtio_irq) = if boot_info_valid() {
        let info = boot_info();
        (info.uart_irq, info.virtio_base, info.virtio_irq)
    } else if fdt_addr != 0 {
        if let Some(info) = fdt_parse(fdt_addr as *const u8) {
            let uart = info.uart.unwrap_or(DeviceInfo { base: 0x10000000, irq: 10 });
            let virtio = info.virtio.unwrap_or(DeviceInfo { base: 0x10001000, irq: 1 });
            (uart.irq, virtio.base, virtio.irq)
        } else {
            (10, 0x10001000, 1)
        }
    } else {
        (10, 0x10001000, 1)
    };

    plic_init(uart_irq, virtio_irq);
    print_log("BOOT", "plic ok");

    clint_init();
    print_log("BOOT", "clint ok");

    mm_init();
    print_log("MM", "page allocator ready");

    let block = VirtioBlock::new(virtio_base, virtio_irq);
    if block.init() {
        print_log("VIRTIO", "block device ready");
    } else {
        print_log("VIRTIO", "no block device");
    }

    sched_init();
    print_log("PROC", "scheduler started");

    print_log("SLIP", "ready");
    shell_start();
}
