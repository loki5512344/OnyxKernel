const BLOCK_SIZE: usize = 512;

pub struct SlipFS;

impl SlipFS {
    pub fn mount(&self) {
        // TODO: implement slipfs mount
    }

    pub fn read(&self, _block: usize, _buf: &mut [u8]) {
        // TODO: implement block read
    }

    pub fn write(&self, _block: usize, _buf: &[u8]) {
        // TODO: implement block write
    }
}
