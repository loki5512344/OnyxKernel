//! Filesystem syscalls (part 1) — `sys_write`, `sys_read`, `sys_open`,
//! `sys_close`, `sys_lseek`, `sys_stat`.
//!
//! All functions here are `pub(super) unsafe fn` so `handler::handle` can
//! dispatch to them. User-pointer validation goes through the shared
//! `super::handler::user_ptr_ok` helper.
use crate::arch::trap_frame::TrapFrame;
use crate::drivers::uart;
use crate::fs::vfs;
use crate::proc;
use onyx_core::errno::Errno;

use super::handler::user_ptr_ok;

pub(super) unsafe fn sys_write(tf: &mut TrapFrame, _fd: u64, buf: u64, len: u64) -> i64 {
    if !user_ptr_ok(buf, len) {
        return Errno::Inval.as_i64();
    }
    if _fd != 1 && _fd != 2 {
        return Errno::BadFd.as_i64();
    }
    let src = buf as *const u8;
    let mut written: i64 = 0;
    let mut i: u64 = 0;
    while i < len {
        let b = *src.add(i as usize);
        if b == b'\n' {
            uart::putc(b'\r');
        }
        uart::putc(b);
        written += 1;
        i += 1;
    }
    let _ = tf;
    written
}

pub(super) unsafe fn sys_read(tf: &mut TrapFrame, _fd: u64, buf: u64, len: u64) -> i64 {
    if !user_ptr_ok(buf, len) {
        return Errno::Inval.as_i64();
    }
    if _fd != 0 {
        return Errno::BadFd.as_i64();
    }
    if len == 0 {
        return 0;
    }
    let dst = buf as *mut u8;
    let mut n: usize = 0;
    let max = (len - 1) as usize;
    while n < max {
        match uart::getc() {
            None => {
                proc::sched_yield(tf);
                continue;
            }
            Some(b) => {
                if b == b'\r' || b == b'\n' {
                    *dst.add(n) = b'\n';
                    uart::putc(b'\r');
                    uart::putc(b'\n');
                    n += 1;
                    break;
                } else if b == 0x7F || b == 0x08 {
                    if n > 0 {
                        n -= 1;
                        uart::putc(0x08);
                        uart::putc(b' ');
                        uart::putc(0x08);
                    }
                } else {
                    *dst.add(n) = b;
                    uart::putc(b);
                    n += 1;
                }
            }
        }
    }
    *dst.add(n) = 0;
    n as i64
}

pub(super) unsafe fn sys_open(path: u64, _flags: u64, _mode: u64) -> i64 {
    if !user_ptr_ok(path, 1) {
        return Errno::Inval.as_i64();
    }
    let mut len = 0usize;
    let p = path as *const u8;
    while *p.add(len) != 0 && len < 256 {
        len += 1;
    }
    let path_bytes = core::slice::from_raw_parts(p, len);

    // Ring-aware path policy.
    let ring = proc::current_ring();
    if ring == proc::PROC_RING_USER {
        // User processes cannot open /service/* or /dev/uart*
        if path_bytes.starts_with(b"/service/") {
            return Errno::Perm.as_i64();
        }
    }

    match vfs::open(path_bytes, vfs::PERM_READ | vfs::PERM_SEEK) {
        Ok(token) => token as i64,
        Err(e) => e.as_i64(),
    }
}

pub(super) unsafe fn sys_close(token: u64) -> i64 {
    match vfs::close(token) {
        Ok(()) => 0,
        Err(e) => e.as_i64(),
    }
}

pub(super) unsafe fn sys_lseek(token: u64, off: i64, whence: u32) -> i64 {
    match vfs::lseek(token, off, whence) {
        Ok(pos) => pos as i64,
        Err(e) => e.as_i64(),
    }
}

pub(super) unsafe fn sys_stat(path: u64, _st: u64) -> i64 {
    if !user_ptr_ok(path, 1) {
        return Errno::Inval.as_i64();
    }
    let mut len = 0usize;
    let p = path as *const u8;
    while *p.add(len) != 0 && len < 256 {
        len += 1;
    }
    let path_bytes = core::slice::from_raw_parts(p, len);
    let token = match vfs::open(path_bytes, vfs::PERM_READ) {
        Ok(t) => t,
        Err(e) => return e.as_i64(),
    };
    let mut size = 0u32;
    let res = vfs::stat(token, &mut size);
    let _ = vfs::close(token);
    match res {
        Ok(()) => size as i64,
        Err(e) => e.as_i64(),
    }
}
