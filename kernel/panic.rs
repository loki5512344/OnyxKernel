use core::fmt::Write;
use core::panic::PanicInfo;

use crate::drivers::uart::uart_base;
use crate::drivers::uart::Uart;

#[panic_handler]
fn panic(info: &PanicInfo) -> ! {
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
    let _ = write!(u, "\x1b[1;31m\n{}\n\n!! SLIPPED !!\x1b[0m\n", seal);
    let _ = write!(u, "cause: {}\n", info.message());
    if let Some(loc) = info.location() {
        let _ = write!(u, "at: {}:{}\n", loc.file(), loc.line());
    }
    let _ = write!(u, "\x1b[1;31mhalted.\x1b[0m\n");
    loop {
        unsafe {
            riscv::asm::wfi();
        }
    }
}
