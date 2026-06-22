use core::fmt::{self, Write};

const UART_THR: usize = 0;
const UART_RBR: usize = 0;
const UART_LSR: usize = 5;
const UART_LSR_THRE: u8 = 1 << 5;
const UART_LSR_DR: u8 = 1 << 0;

pub struct Uart {
    base: usize,
}

impl Uart {
    pub fn new(base: usize) -> Self {
        Uart { base }
    }

    pub fn init(&self) {
        let p = self.base as *mut u8;
        unsafe {
            p.add(3).write_volatile(0x03);
            p.add(2).write_volatile(0x07);
            p.add(1).write_volatile(0x01);
        }
    }

    pub fn putchar(&self, c: u8) {
        let p = self.base as *mut u8;
        unsafe {
            while (p.add(UART_LSR).read_volatile() & UART_LSR_THRE) == 0 {}
            p.add(UART_THR).write_volatile(c);
        }
    }

    pub fn getchar(&self) -> Option<u8> {
        let p = self.base as *mut u8;
        unsafe {
            if (p.add(UART_LSR).read_volatile() & UART_LSR_DR) != 0 {
                Some(p.add(UART_RBR).read_volatile())
            } else {
                None
            }
        }
    }
}

impl Write for Uart {
    fn write_str(&mut self, s: &str) -> fmt::Result {
        for &b in s.as_bytes() {
            if b == b'\n' {
                self.putchar(b'\r');
            }
            self.putchar(b);
        }
        Ok(())
    }
}

static mut UART_BASE: usize = 0;

pub fn uart_base() -> usize {
    unsafe {
        let base = UART_BASE;
        if base != 0 { base } else { 0x10000000 }
    }
}

pub fn uart_init() {
    let base = if crate::fdt::boot_info_valid() {
        crate::fdt::boot_info().uart_base
    } else {
        0x10000000
    };
    unsafe { UART_BASE = base };
    let u = Uart::new(base);
    u.init();
}

pub fn uart_putchar(c: u8) {
    let u = Uart::new(uart_base());
    u.putchar(c);
}

pub fn uart_getchar() -> Option<u8> {
    let u = Uart::new(uart_base());
    u.getchar()
}

pub fn print_log(tag: &str, msg: &str) {
    let color = match tag {
        "BOOT" => "\x1b[1;34m",
        "MM" => "\x1b[1;32m",
        "PROC" => "\x1b[1;33m",
        "SLIP" => "\x1b[1;36m",
        "VIRTIO" => "\x1b[1;35m",
        _ => "\x1b[0m",
    };
    let reset = "\x1b[0m";
    let mut u = Uart::new(uart_base());
    let _ = write!(u, "{color}[{tag}]{reset} {msg}\n");
}

pub fn print_seal() {
    let mut u = Uart::new(uart_base());
    let seal = "\
        ⠴⠋⠉⠙⠦
       ⠾     ⠷
       ⣿⠷⠷⠷⠾⣿
       ⠙⠦   ⠴⠋
         ⠴⠷
        ⠴⠁ ⠳
       ⠰⠁   ⠁⠦
       ⠾⠳  ⠻⠟ ⠈⠦
       ⣿ ⠳      ⠳
       ⠻        ⣿
       ⠘⠴       ⣿
        ⠻       ⣿
        ⠾       ⣿
       ⠰⠋       ⠙⠦
       ⠾          ⠈⠙⠓⠦
      ⠰⠋              ⠙⠓⠦
      ⠸                  ⠙⠓⠦
      ⠸                    ⠙⠦
       ⠻                    ⠙⠷
        ⠻                    ⣿⠔⠈⠈⠈
        ⠌⠻  ⠸     ⠎          ⠟
   ⠄⠂⠁⠈⠁ ⠘     ⠅⠋⠉⠉⠉⠙⠉⠉⠁  ⠈⠑⠠
 ⠴⠮--⠄⠠⠄⠈⠁   ⠣    ⠣          ⠈⠠⠄⠁
              ⠑⠄   ⠜
                ⠈⠠⠄⠁";
    let _ = write!(u, "\n\x1b[1;36m{}\x1b[0m\n\n", seal);
}
