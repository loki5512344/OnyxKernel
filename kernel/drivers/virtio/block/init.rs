use super::{
    VirtioBlock, DESC, AVAIL, USED, LAST_USED_IDX,
    REG_MAGIC, REG_VERSION, REG_DEVICE_ID,
    REG_DEVICE_FEATURES_SEL, REG_DEVICE_FEATURES,
    REG_DRIVER_FEATURES_SEL, REG_DRIVER_FEATURES,
    REG_QUEUE_SEL, REG_QUEUE_NUM_MAX, REG_QUEUE_NUM,
    REG_QUEUE_DESC_LOW, REG_QUEUE_DESC_HIGH,
    REG_QUEUE_DRIVER_LOW, REG_QUEUE_DRIVER_HIGH,
    REG_QUEUE_DEVICE_LOW, REG_QUEUE_DEVICE_HIGH,
    REG_QUEUE_READY, QUEUE_NUM,
};

const STATUS_ACK: u8 = 1;
const STATUS_DRIVER: u8 = 2;
const STATUS_FEATURES_OK: u8 = 8;
const STATUS_DRIVER_OK: u8 = 16;

impl VirtioBlock {
    pub fn init(&self) -> bool {
        if self.base == 0 {
            return false;
        }

        let magic = self.read_reg(REG_MAGIC);
        if magic != 0x74726976 {
            return false;
        }

        let version = self.read_reg(REG_VERSION);
        if version != 2 {
            return false;
        }

        let dev_id = self.read_reg(REG_DEVICE_ID);
        if dev_id != 2 {
            return false;
        }

        self.write_status(0);
        self.write_status(STATUS_ACK);
        self.write_status(STATUS_ACK | STATUS_DRIVER);

        self.write_reg(REG_DEVICE_FEATURES_SEL, 1);
        let dev_features1 = self.read_reg(REG_DEVICE_FEATURES);
        if dev_features1 & 1 == 0 {
            return false;
        }

        self.write_reg(REG_DRIVER_FEATURES_SEL, 1);
        self.write_reg(REG_DRIVER_FEATURES, 1);
        self.write_reg(REG_DRIVER_FEATURES_SEL, 0);
        self.write_reg(REG_DRIVER_FEATURES, 0);

        self.write_status(STATUS_ACK | STATUS_DRIVER | STATUS_FEATURES_OK);

        let st = self.read_status();
        if st & STATUS_FEATURES_OK == 0 {
            return false;
        }

        self.write_reg(REG_QUEUE_SEL, 0);
        let num_max = self.read_reg(REG_QUEUE_NUM_MAX);
        let num = if num_max > QUEUE_NUM as u32 { QUEUE_NUM as u32 } else { num_max };
        if num < 3 {
            return false;
        }
        self.write_reg(REG_QUEUE_NUM, num);

        let desc_addr = core::ptr::addr_of!(DESC) as u64;
        self.write_reg(REG_QUEUE_DESC_LOW, desc_addr as u32);
        self.write_reg(REG_QUEUE_DESC_HIGH, (desc_addr >> 32) as u32);

        let avail_addr = core::ptr::addr_of!(AVAIL) as u64;
        self.write_reg(REG_QUEUE_DRIVER_LOW, avail_addr as u32);
        self.write_reg(REG_QUEUE_DRIVER_HIGH, (avail_addr >> 32) as u32);

        let used_addr = core::ptr::addr_of!(USED) as u64;
        self.write_reg(REG_QUEUE_DEVICE_LOW, used_addr as u32);
        self.write_reg(REG_QUEUE_DEVICE_HIGH, (used_addr >> 32) as u32);

        self.write_reg(REG_QUEUE_READY, 1);
        if self.read_reg(REG_QUEUE_READY) == 0 {
            return false;
        }

        self.write_status(STATUS_ACK | STATUS_DRIVER | STATUS_FEATURES_OK | STATUS_DRIVER_OK);

        if self.read_status() & STATUS_DRIVER_OK == 0 {
            return false;
        }

        unsafe {
            LAST_USED_IDX = 0;
        }

        true
    }
}
