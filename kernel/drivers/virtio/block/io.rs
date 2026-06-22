use core::sync::atomic::{fence, Ordering};

use super::{
    VirtioBlock, DESC, AVAIL, USED, LAST_USED_IDX, HEADER, STATUS,
    Descriptor, BlockHeader, QUEUE_NUM, REG_QUEUE_NOTIFY,
};

const DESC_NEXT: u16 = 1;
const DESC_WRITE: u16 = 2;

impl VirtioBlock {
    pub fn read_sector(&self, lba: u64, buf: &mut [u8]) -> bool {
        if buf.len() < 512 {
            return false;
        }

        unsafe {
            HEADER = BlockHeader {
                type_: 0,
                reserved: 0,
                sector: lba,
            };
            STATUS = 0xFF;

            let desc = core::ptr::addr_of_mut!(DESC.0) as *mut Descriptor;

            (*desc.add(0)).addr = core::ptr::addr_of!(HEADER) as u64;
            (*desc.add(0)).len = 16;
            (*desc.add(0)).flags = DESC_NEXT;
            (*desc.add(0)).next = 1;

            (*desc.add(1)).addr = buf.as_mut_ptr() as u64;
            (*desc.add(1)).len = 512;
            (*desc.add(1)).flags = DESC_NEXT | DESC_WRITE;
            (*desc.add(1)).next = 2;

            (*desc.add(2)).addr = core::ptr::addr_of!(STATUS) as u64;
            (*desc.add(2)).len = 1;
            (*desc.add(2)).flags = DESC_WRITE;
            (*desc.add(2)).next = 0;

            fence(Ordering::SeqCst);

            let hd = core::ptr::addr_of_mut!(AVAIL);
            let idx = (*hd).idx;
            (*hd).ring[(idx as usize) % QUEUE_NUM] = 0;
            (*hd).idx = idx.wrapping_add(1);

            fence(Ordering::SeqCst);

            self.write_reg(REG_QUEUE_NOTIFY, 0);

            loop {
                fence(Ordering::SeqCst);
                let used = core::ptr::addr_of!(USED);
                if (*used).idx != LAST_USED_IDX {
                    break;
                }
            }

            fence(Ordering::SeqCst);

            LAST_USED_IDX = LAST_USED_IDX.wrapping_add(1);
        }

        unsafe { STATUS == 0 }
    }
}
