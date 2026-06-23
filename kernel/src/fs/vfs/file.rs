//! File operations — open, close, read, write, stat, lseek.
use super::{
    alloc_fd, fd_check, fd_check_perm, fd_token, FdToken, Fs, G_FDS, G_ROOT_FS, PERM_READ,
    PERM_SEEK, PERM_WRITE,
};
use crate::fs::{fat32, onyxfs};
use onyx_core::errno::{Errno, KResult};

pub unsafe fn open(path: &[u8], perms: u32) -> KResult<FdToken> {
    if path.is_empty() || path[0] != b'/' {
        return Err(Errno::Inval);
    }
    let name = &path[1..];
    let idx = alloc_fd(perms)?;
    let pf = &raw mut G_FDS;
    let fd = &mut (*pf)[idx];
    let mut st = onyxfs::OnyfsStat::default();
    match *(&raw const G_ROOT_FS) {
        Fs::Onyx => {
            onyxfs::lookup(name, &mut st)?;
            fd.ino = st.ino;
            // OnyfsStat.size is u64 (v2); VfsFd.size is u32 — truncate.
            fd.size = st.size.min(u32::MAX as u64) as u32;
            fd.fs = Fs::Onyx;
            fd.pos = 0;
        }
        Fs::Fat32 => {
            let mut cluster = 0u32;
            let mut size = 0u32;
            fat32::lookup(name, &mut cluster, &mut size)?;
            fd.ino = cluster;
            fd.size = size;
            fd.fs = Fs::Fat32;
            fd.pos = 0;
        }
        Fs::None => return Err(Errno::Inval),
    }
    Ok(fd_token(idx, fd.epoch))
}

pub unsafe fn close(token: FdToken) -> KResult<()> {
    let fd = fd_check(token)?;
    fd.used = false;
    Ok(())
}

pub unsafe fn read(token: FdToken, buf: *mut u8, len: u32) -> KResult<u32> {
    let fd = fd_check_perm(token, PERM_READ)?;
    let avail = fd.size.saturating_sub(fd.pos);
    let to_read = len.min(avail);
    if to_read == 0 {
        return Ok(0);
    }
    let read_n = match fd.fs {
        Fs::Onyx => onyxfs::read(fd.ino, buf, fd.pos, to_read)?,
        Fs::Fat32 => fat32::read(fd.ino, buf, fd.pos, to_read)?,
        Fs::None => return Err(Errno::Inval),
    };
    fd.pos += read_n;
    Ok(read_n)
}

/// Write `len` bytes from `buf` to an open file at its current position.
/// Grows the file as needed. The fd must have been opened with PERM_WRITE.
/// Only OnyxFS is supported (FAT32 is read-only in this kernel).
pub unsafe fn write(token: FdToken, buf: *const u8, len: u32) -> KResult<u32> {
    let fd = fd_check_perm(token, PERM_WRITE)?;
    let written = match fd.fs {
        Fs::Onyx => onyxfs::write(fd.ino, buf, fd.pos, len)?,
        _ => return Err(Errno::NoSys),
    };
    fd.pos += written;
    if fd.pos > fd.size {
        fd.size = fd.pos;
    }
    Ok(written)
}

pub unsafe fn stat(token: FdToken, size_out: &mut u32) -> KResult<()> {
    let fd = fd_check(token)?;
    *size_out = fd.size;
    Ok(())
}

pub unsafe fn lseek(token: FdToken, off: i64, whence: u32) -> KResult<u32> {
    let fd = fd_check_perm(token, PERM_SEEK)?;
    let new_pos: i64 = match whence {
        0 => off,
        1 => fd.pos as i64 + off,
        2 => fd.size as i64 + off,
        _ => return Err(Errno::Inval),
    };
    if new_pos < 0 || new_pos > fd.size as i64 {
        return Err(Errno::Range);
    }
    fd.pos = new_pos as u32;
    Ok(fd.pos)
}
