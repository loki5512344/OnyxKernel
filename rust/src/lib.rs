#![no_std]
#![no_builtins]

use core::panic::PanicInfo;

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {}
}

#[no_mangle]
pub unsafe extern "C" fn memcpy(dst: *mut u8, src: *const u8, n: usize) -> *mut u8 {
    for i in 0..n {
        *dst.add(i) = *src.add(i);
    }
    dst
}

#[no_mangle]
pub unsafe extern "C" fn memset(s: *mut u8, c: i32, n: usize) -> *mut u8 {
    for i in 0..n {
        *s.add(i) = c as u8;
    }
    s
}

#[no_mangle]
pub unsafe extern "C" fn memmove(dst: *mut u8, src: *const u8, n: usize) -> *mut u8 {
    if dst < src as *mut u8 {
        for i in 0..n {
            *dst.add(i) = *src.add(i);
        }
    } else if dst > src as *mut u8 {
        let mut i = n;
        while i > 0 {
            i -= 1;
            *dst.add(i) = *src.add(i);
        }
    }
    dst
}

#[no_mangle]
pub extern "C" fn strlen(s: *const u8) -> usize {
    let mut n = 0;
    unsafe {
        while *s.add(n) != 0 {
            n += 1;
        }
    }
    n
}

#[no_mangle]
pub extern "C" fn strcmp(a: *const u8, b: *const u8) -> i32 {
    unsafe {
        let mut i = 0;
        loop {
            let ca = *a.add(i);
            let cb = *b.add(i);
            if ca != cb {
                return ca as i32 - cb as i32;
            }
            if ca == 0 {
                return 0;
            }
            i += 1;
        }
    }
}
