//! SLAB allocator — fixed-size object pools for small allocations (64, 256,
//! 1024 bytes). Each SLAB page has a `SlabHeader` followed by an array of
//! equal-sized object slots tracked via a 64-bit free bitmap.
use super::bitmap::alloc;
use super::{G_PMM, PAGE_SIZE, SLAB_MAGIC, SLAB_SIZES};
use core::ptr;

#[repr(C)]
pub(in crate::mm) struct SlabHeader {
    pub(in crate::mm) magic: u32,
    pub(in crate::mm) size_idx: u32,
    pub(in crate::mm) free_bits: u64,
    pub(in crate::mm) capacity: u32,
    pub(in crate::mm) free_count: u32,
    pub(in crate::mm) next: *mut SlabHeader,
}

pub(super) const fn size_of_slab_header() -> usize {
    32
}

unsafe fn slab_class_for(size: usize) -> Option<usize> {
    for (i, &s) in SLAB_SIZES.iter().enumerate() {
        if size <= s {
            return Some(i);
        }
    }
    None
}

pub unsafe fn slab_alloc(size: usize) -> Option<*mut u8> {
    let class = slab_class_for(size)?;
    let obj_size = SLAB_SIZES[class];
    let hdr_size = size_of_slab_header();
    if PAGE_SIZE - hdr_size < obj_size {
        return None;
    }
    let pr = &raw const G_PMM;
    let head = (*pr).slab_heads[class];
    let mut page = head;
    while !page.is_null() {
        let hdr = &mut *page;
        if hdr.free_count > 0 {
            let mut slot = 0u32;
            while slot < hdr.capacity {
                if hdr.free_bits & (1u64 << slot) != 0 {
                    hdr.free_bits &= !(1u64 << slot);
                    hdr.free_count -= 1;
                    return Some((page as usize + hdr_size + slot as usize * obj_size) as *mut u8);
                }
                slot += 1;
            }
        }
        page = hdr.next;
    }
    let new_page_pa = alloc().ok()? as usize;
    let new_page = new_page_pa as *mut SlabHeader;
    let avail = PAGE_SIZE - hdr_size;
    let capacity = (avail / obj_size) as u32;
    let cap64 = capacity as u64;
    let all_free = if cap64 == 64 {
        !0u64
    } else {
        (1u64 << cap64) - 1
    };
    let hdr = &mut *new_page;
    hdr.magic = SLAB_MAGIC;
    hdr.size_idx = class as u32;
    hdr.free_bits = all_free;
    hdr.capacity = capacity;
    hdr.free_count = capacity;
    let pm = &raw const G_PMM;
    hdr.next = (*pm).slab_heads[class];
    (*(&raw mut G_PMM)).slab_heads[class] = new_page;
    hdr.free_bits &= !1;
    hdr.free_count -= 1;
    Some((new_page as usize + hdr_size) as *mut u8)
}

pub unsafe fn slab_free(ptr: *mut u8) -> bool {
    let page_addr = (ptr as usize) & !(PAGE_SIZE - 1);
    let page = page_addr as *mut SlabHeader;
    if page.is_null() {
        return false;
    }
    let hdr = &mut *page;
    if hdr.magic != SLAB_MAGIC {
        return false;
    }
    let obj_size = SLAB_SIZES[hdr.size_idx as usize];
    let hdr_size = size_of_slab_header();
    let offset = ptr as usize - page_addr - hdr_size;
    if !offset.is_multiple_of(obj_size) {
        return false;
    }
    let slot = (offset / obj_size) as u32;
    if slot >= hdr.capacity {
        return false;
    }
    hdr.free_bits |= 1u64 << slot;
    hdr.free_count += 1;
    if hdr.free_count == hdr.capacity {
        let class = hdr.size_idx as usize;
        let mut cur = (*(&raw const G_PMM)).slab_heads[class];
        let mut prev: *mut SlabHeader = ptr::null_mut();
        while !cur.is_null() {
            if cur == page {
                if prev.is_null() {
                    (*(&raw mut G_PMM)).slab_heads[class] = (*cur).next;
                } else {
                    (*prev).next = (*cur).next;
                }
                break;
            }
            prev = cur;
            cur = (*cur).next;
        }
        super::bitmap::free(page_addr as u64);
    }
    true
}
