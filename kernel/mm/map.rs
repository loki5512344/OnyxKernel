const PAGE_SIZE: usize = 4096;
const PAGE_TABLE_ENTRIES: usize = 512;

#[repr(C, align(4096))]
struct PageTable {
    entries: [PageTableEntry; PAGE_TABLE_ENTRIES],
}

#[repr(transparent)]
#[derive(Clone, Copy)]
struct PageTableEntry(u64);

impl PageTableEntry {
    fn is_valid(&self) -> bool {
        self.0 & 0x1 != 0
    }

    fn paddr(&self) -> usize {
        (self.0 >> 10 & 0xFFFFFFFFFFF) as usize
    }

    fn set_paddr(&mut self, paddr: usize) {
        self.0 = (self.0 & 0x3FF) | ((paddr as u64 & 0xFFFFFFFFFFF) << 10);
    }
}

pub fn map_page(table: *mut PageTable, vaddr: usize, paddr: usize, flags: u64) {
    let vpn = vaddr >> 12;
    let idx2 = (vpn >> 18) & 0x1FF;
    let idx1 = (vpn >> 9) & 0x1FF;
    let idx0 = vpn & 0x1FF;
    unsafe {
        let pte = &mut (*table).entries[idx2];
        if !pte.is_valid() {
            let page = (*table).entries[idx2].paddr();
            if page == 0 {
                return;
            }
        }
        let pte1 = &mut (*(pte.paddr() as *mut PageTable)).entries[idx1];
        if !pte1.is_valid() {
            let page = (*table).entries[idx2].paddr();
            if page == 0 {
                return;
            }
        }
        let pte0 = &mut (*(pte1.paddr() as *mut PageTable)).entries[idx0];
        pte0.0 = (paddr as u64 & 0xFFFFFFFFFFF) << 10 | flags | 0x1;
    }
}
