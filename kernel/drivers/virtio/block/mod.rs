mod init;
mod io;

const REG_MAGIC: usize = 0x000;
const REG_VERSION: usize = 0x004;
const REG_DEVICE_ID: usize = 0x008;
const REG_DEVICE_FEATURES: usize = 0x010;
const REG_DEVICE_FEATURES_SEL: usize = 0x014;
const REG_DRIVER_FEATURES: usize = 0x020;
const REG_DRIVER_FEATURES_SEL: usize = 0x024;
const REG_QUEUE_SEL: usize = 0x030;
const REG_QUEUE_NUM_MAX: usize = 0x034;
const REG_QUEUE_NUM: usize = 0x038;
const REG_QUEUE_READY: usize = 0x044;
const REG_QUEUE_NOTIFY: usize = 0x050;
const REG_STATUS: usize = 0x070;
const REG_QUEUE_DESC_LOW: usize = 0x080;
const REG_QUEUE_DESC_HIGH: usize = 0x084;
const REG_QUEUE_DRIVER_LOW: usize = 0x088;
const REG_QUEUE_DRIVER_HIGH: usize = 0x08C;
const REG_QUEUE_DEVICE_LOW: usize = 0x090;
const REG_QUEUE_DEVICE_HIGH: usize = 0x094;

const QUEUE_NUM: usize = 16;

#[derive(Clone, Copy)]
#[repr(C)]
struct Descriptor {
    addr: u64,
    len: u32,
    flags: u16,
    next: u16,
}

#[repr(C, align(16))]
struct DescTable([Descriptor; QUEUE_NUM]);

#[repr(C, align(2))]
struct AvailRing {
    flags: u16,
    idx: u16,
    ring: [u16; QUEUE_NUM],
}

#[derive(Clone, Copy)]
#[repr(C)]
struct UsedElem {
    id: u32,
    len: u32,
}

#[repr(C, align(4))]
struct UsedRing {
    flags: u16,
    idx: u16,
    ring: [UsedElem; QUEUE_NUM],
}

#[repr(C)]
struct BlockHeader {
    type_: u32,
    reserved: u32,
    sector: u64,
}

const DESC_INIT: Descriptor = Descriptor { addr: 0, len: 0, flags: 0, next: 0 };
const USED_ELEM_INIT: UsedElem = UsedElem { id: 0, len: 0 };

static mut DESC: DescTable = DescTable([DESC_INIT; QUEUE_NUM]);
static mut AVAIL: AvailRing = AvailRing { flags: 0, idx: 0, ring: [0; QUEUE_NUM] };
static mut USED: UsedRing = UsedRing { flags: 0, idx: 0, ring: [USED_ELEM_INIT; QUEUE_NUM] };
static mut LAST_USED_IDX: u16 = 0;
static mut HEADER: BlockHeader = BlockHeader { type_: 0, reserved: 0, sector: 0 };
static mut STATUS: u8 = 0;

pub struct VirtioBlock {
    base: usize,
}

impl VirtioBlock {
    pub fn new(base: usize, _irq: u32) -> Self {
        VirtioBlock { base }
    }

    fn read_reg(&self, reg: usize) -> u32 {
        unsafe { ((self.base + reg) as *const u32).read_volatile() }
    }

    fn write_reg(&self, reg: usize, val: u32) {
        unsafe { ((self.base + reg) as *mut u32).write_volatile(val) }
    }

    fn write_status(&self, val: u8) {
        unsafe { ((self.base + REG_STATUS) as *mut u8).write_volatile(val) }
    }

    fn read_status(&self) -> u8 {
        unsafe { ((self.base + REG_STATUS) as *const u8).read_volatile() }
    }
}
