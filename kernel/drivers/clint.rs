const CLINT_BASE: usize = 0x02000000;
const CLINT_MTIMECMP: usize = 0x4000;
const CLINT_MTIME: usize = 0xBFF8;
const TIMER_INTERVAL: u64 = 100_000;

pub fn clint_schedule(delay: u64) {
    let mtimecmp = (CLINT_BASE + CLINT_MTIMECMP) as *mut u64;
    let mtime = (CLINT_BASE + CLINT_MTIME) as *const u64;
    unsafe {
        let now = mtime.read_volatile();
        mtimecmp.write_volatile(now + delay);
    }
}

pub fn clint_init() {
    clint_schedule(TIMER_INTERVAL);
}

pub fn clint_clear() {
    clint_schedule(TIMER_INTERVAL);
}

pub fn clint_get_time() -> u64 {
    let mtime = (CLINT_BASE + CLINT_MTIME) as *const u64;
    unsafe { mtime.read_volatile() }
}
