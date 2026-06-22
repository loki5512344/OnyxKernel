const PLIC_BASE: usize = 0x0C000000;
const PLIC_ENABLE: usize = 0x002000;
const PLIC_THRESHOLD: usize = 0x200000;
const PLIC_CLAIM: usize = 0x200004;

pub fn plic_init(uart_irq: u32, virtio_irq: u32) {
    let priority = PLIC_BASE as *mut u32;
    unsafe {
        priority.add(uart_irq as usize).write_volatile(1);
        priority.add(virtio_irq as usize).write_volatile(1);
    }

    let enable = (PLIC_BASE + PLIC_ENABLE) as *mut u32;
    unsafe {
        enable.write_volatile((1 << uart_irq) | (1 << virtio_irq));
    }

    let threshold = (PLIC_BASE + PLIC_THRESHOLD) as *mut u32;
    unsafe {
        threshold.write_volatile(0);
    }
}

pub fn plic_claim() -> u32 {
    let claim = (PLIC_BASE + PLIC_CLAIM) as *const u32;
    unsafe { claim.read_volatile() }
}

pub fn plic_complete(irq: u32) {
    let claim = (PLIC_BASE + PLIC_CLAIM) as *mut u32;
    unsafe {
        claim.write_volatile(irq);
    }
}
