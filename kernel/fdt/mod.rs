mod parser;

pub use parser::fdt_parse;

#[repr(C)]
pub struct DeviceInfo {
    pub base: usize,
    pub irq: u32,
}

#[repr(C)]
pub struct BootInfo {
    pub magic: u64,
    pub version: u32,
    pub memory_base: usize,
    pub memory_size: usize,
    pub uart_base: usize,
    pub uart_irq: u32,
    pub virtio_base: usize,
    pub virtio_irq: u32,
}

pub struct FdtInfo {
    pub memory: Option<(usize, usize)>,
    pub uart: Option<DeviceInfo>,
    pub virtio: Option<DeviceInfo>,
}

const BOOT_INFO_ADDR: usize = 0x801FF000;
const BOOT_INFO_MAGIC: u64 = u64::from_ne_bytes([b'S', b'L', b'I', b'P', 0, 0, 0, 0]);

pub fn boot_info() -> &'static BootInfo {
    let ptr = BOOT_INFO_ADDR as *const BootInfo;
    unsafe { &*ptr }
}

pub fn boot_info_valid() -> bool {
    let info = boot_info();
    info.magic == BOOT_INFO_MAGIC && info.version >= 1
}
