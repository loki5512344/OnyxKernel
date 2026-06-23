//! VFS — Virtual File System with Capability FDs + opendir/readdir.
//!
//! This is the directory module root. It owns the global FD table and the
//! `Fs` enum, plus the constants and the `mount_root`/`init` entry points.
//! File operations (open/close/read/write/stat/lseek/create/mkdir) live in
//! `file.rs`; `readdir` lives in `dir.rs`.
use crate::fs::{fat32, onyxfs};
use onyx_core::errno::{Errno, KResult};

pub const VFS_MAX_FDS: usize = 16;
#[derive(Clone, Copy, PartialEq, Eq)]
pub enum Fs {
    None = 0,
    Onyx = 1,
    Fat32 = 2,
}
pub const PERM_READ: u32 = 1;
pub const PERM_WRITE: u32 = 2;
pub const PERM_SEEK: u32 = 4;
pub const PERM_EXEC: u32 = 8;
pub const PERM_ALL: u32 = PERM_READ | PERM_WRITE | PERM_SEEK | PERM_EXEC;

#[derive(Clone, Copy)]
pub struct VfsFd {
    pub ino: u32,
    pub size: u32,
    pub pos: u32,
    pub fs: Fs,
    pub used: bool,
    pub perms: u32,
    pub epoch: u32,
}
impl Default for VfsFd {
    fn default() -> Self {
        Self {
            ino: 0,
            size: 0,
            pos: 0,
            fs: Fs::None,
            used: false,
            perms: 0,
            epoch: 0,
        }
    }
}

pub type FdToken = u64;
pub const FD_TOKEN_NONE: FdToken = 0xFFFF_FFFF_FFFF_FFFF;
#[inline]
pub const fn fd_token(idx: usize, epoch: u32) -> FdToken {
    ((idx as u64) << 32) | (epoch as u64)
}
#[inline]
pub const fn fd_token_idx(token: FdToken) -> usize {
    (token >> 32) as usize
}
#[inline]
pub const fn fd_token_epoch(token: FdToken) -> u32 {
    token as u32
}

pub(super) static mut G_ROOT_FS: Fs = Fs::None;
pub(super) static mut G_FDS: [VfsFd; VFS_MAX_FDS] = [VfsFd {
    ino: 0,
    size: 0,
    pos: 0,
    fs: Fs::None,
    used: false,
    perms: 0,
    epoch: 0,
}; VFS_MAX_FDS];

pub unsafe fn init() {
    let pf = &raw mut G_FDS;
    for fd in (*pf).iter_mut() {
        *fd = VfsFd::default();
    }
}

pub unsafe fn mount_root(dev: usize, onyxfs_lba: u32) -> KResult<()> {
    if onyxfs::mount(dev, onyxfs_lba).is_ok() {
        *(&raw mut G_ROOT_FS) = Fs::Onyx;
        return Ok(());
    }
    if fat32::mount(dev).is_ok() {
        *(&raw mut G_ROOT_FS) = Fs::Fat32;
        return Ok(());
    }
    Err(Errno::Io)
}

pub fn root_fs() -> Fs {
    unsafe { *(&raw const G_ROOT_FS) }
}

pub(super) unsafe fn alloc_fd(perms: u32) -> KResult<usize> {
    let pf = &raw mut G_FDS;
    for i in 0..VFS_MAX_FDS {
        if !(*pf)[i].used {
            (*pf)[i].used = true;
            (*pf)[i].perms = perms;
            (*pf)[i].epoch = (*pf)[i].epoch.wrapping_add(1);
            if (*pf)[i].epoch == 0 {
                (*pf)[i].epoch = 1;
            }
            return Ok(i);
        }
    }
    Err(Errno::NoMem)
}

pub(super) unsafe fn fd_check(token: FdToken) -> KResult<&'static mut VfsFd> {
    let idx = fd_token_idx(token);
    if idx >= VFS_MAX_FDS {
        return Err(Errno::BadFd);
    }
    let pf = &raw mut G_FDS;
    let fd = &mut (*pf)[idx];
    if !fd.used || fd.epoch != fd_token_epoch(token) {
        return Err(Errno::BadFd);
    }
    Ok(fd)
}

pub(super) unsafe fn fd_check_perm(token: FdToken, perm: u32) -> KResult<&'static mut VfsFd> {
    let fd = fd_check(token)?;
    if fd.perms & perm == 0 {
        return Err(Errno::Perm);
    }
    Ok(fd)
}

pub mod create;
pub mod dir;
pub mod file;

pub use create::*;
pub use dir::*;
pub use file::*;
