//! Stateful readdir — single active directory cursor (MVP).
use crate::fs::onyxfs;
use onyx_core::errno::KResult;

/// readdir: stateful per-process directory listing.
/// Uses a static cursor (MVP: single active readdir at a time).
pub(super) static mut G_DIR_CURSOR_INO: u32 = 0;
pub(super) static mut G_DIR_CURSOR_IDX: u32 = 0;
pub(super) static mut G_DIR_ACTIVE: bool = false;

pub unsafe fn readdir(dir_path: &[u8], name_out: *mut u8, name_len: usize) -> KResult<bool> {
    // Check if same directory as last call.
    let ino = onyxfs::resolve_dir(dir_path)?;
    if !G_DIR_ACTIVE || G_DIR_CURSOR_INO != ino {
        G_DIR_CURSOR_INO = ino;
        G_DIR_CURSOR_IDX = 0;
        G_DIR_ACTIVE = true;
    }
    // Read next entry.
    match onyxfs::readdir_entry(G_DIR_CURSOR_INO, G_DIR_CURSOR_IDX, name_out, name_len)? {
        Some(_ino) => {
            G_DIR_CURSOR_IDX += 1;
            Ok(true)
        }
        None => {
            G_DIR_ACTIVE = false;
            Ok(false)
        }
    }
}
