use core::ptr::null_mut;

const PAGE_SIZE: usize = 4096;
const PAGE_FRAMES: usize = 32768;

unsafe extern "C" {
    static _end: u8;
}

static mut FREE_LIST: *mut Page = null_mut();

#[repr(C)]
struct Page {
    next: *mut Page,
}

pub struct PageAllocator;

impl PageAllocator {
    pub fn init(&self, start: usize, pages: usize) {
        unsafe {
            for i in 0..pages {
                let addr = (start + i * PAGE_SIZE) as *mut Page;
                (*addr).next = FREE_LIST;
                FREE_LIST = addr;
            }
        }
    }

    pub fn alloc(&self) -> Option<usize> {
        unsafe {
            if FREE_LIST.is_null() {
                return None;
            }
            let page = FREE_LIST;
            FREE_LIST = (*page).next;
            Some(page as usize)
        }
    }

    pub fn free(&self, addr: usize) {
        unsafe {
            let page = addr as *mut Page;
            (*page).next = FREE_LIST;
            FREE_LIST = page;
        }
    }
}

pub fn mm_init() {
    let kernel_end = unsafe { (&_end as *const u8) as usize };
    let heap_start = (kernel_end + PAGE_SIZE - 1) & !(PAGE_SIZE - 1);
    let kernel_pages = (heap_start - 0x80000000) / PAGE_SIZE;
    let allocator = PageAllocator;
    allocator.init(heap_start, PAGE_FRAMES - kernel_pages);
}
