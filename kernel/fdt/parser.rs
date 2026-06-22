use super::{DeviceInfo, FdtInfo};

fn read_be_u32(p: *const u8, off: usize) -> u32 {
    unsafe { u32::from_be(p.add(off).cast::<u32>().read_volatile()) }
}

fn str_match(p: *const u8, off: usize, s: &str) -> bool {
    let b = s.as_bytes();
    unsafe {
        for (i, &c) in b.iter().enumerate() {
            if p.add(off + i).read_volatile() != c {
                return false;
            }
        }
        p.add(off + b.len()).read_volatile() == 0
    }
}

pub fn fdt_parse(blob: *const u8) -> Option<FdtInfo> {
    let magic = read_be_u32(blob, 0);
    if magic != 0xD00DFEED {
        return None;
    }

    let totalsize = read_be_u32(blob, 4) as usize;
    let struct_off = read_be_u32(blob, 8) as usize;
    let strings_off = read_be_u32(blob, 12) as usize;

    if totalsize < 40 || struct_off >= totalsize || strings_off >= totalsize {
        return None;
    }

    let struct_blob = unsafe { blob.add(struct_off) };
    let strings_blob = unsafe { blob.add(strings_off) };
    let strings_limit = totalsize - strings_off;

    let mut info = FdtInfo {
        memory: None,
        uart: None,
        virtio: None,
    };

    let mut pos: usize = 0;
    let mut addr_cells: u32 = 2;
    let mut size_cells: u32 = 2;

    let mut cells_stack: [(u32, u32); 16] = [(2, 2); 16];
    let mut stack_depth: usize = 0;

    let mut current_compatible: u32 = 0;
    let mut current_device_type: u32 = 0;
    let mut current_reg_off: u32 = 0;
    let mut current_irq: u32 = 0;
    let mut got_compatible = false;
    let mut got_device_type = false;
    let mut got_reg = false;
    let mut got_irq = false;

    while pos + 4 < totalsize - struct_off {
        let token = read_be_u32(struct_blob, pos);
        pos += 4;

        if token == 0x01 {
            while pos < totalsize - struct_off {
                let b = unsafe { struct_blob.add(pos).read_volatile() };
                pos += 1;
                if b == 0 {
                    break;
                }
            }
            pos = (pos + 3) & !3;

            if stack_depth < 16 {
                cells_stack[stack_depth] = (addr_cells, size_cells);
            }
            stack_depth += 1;

            got_compatible = false;
            got_device_type = false;
            got_reg = false;
            got_irq = false;
            current_compatible = 0;
            current_device_type = 0;
            current_reg_off = 0;
            current_irq = 0;
        } else if token == 0x02 {
            if stack_depth > 0 {
                stack_depth -= 1;
                if stack_depth < 16 {
                    let (a, s) = cells_stack[stack_depth];
                    addr_cells = a;
                    size_cells = s;
                }
            }

            if got_compatible && got_reg && got_irq && current_compatible != 0 {
                let compat_ptr = unsafe { blob.add(strings_off + current_compatible as usize) };

                if str_match(compat_ptr, 0, "ns16550a") || str_match(compat_ptr, 0, "sifive,uart0") {
                    let reg_base_off = current_reg_off as usize;

                    let base = if addr_cells == 2 {
                        (read_be_u32(struct_blob, reg_base_off) as usize) << 32
                            | read_be_u32(struct_blob, reg_base_off + 4) as usize
                    } else {
                        read_be_u32(struct_blob, reg_base_off) as usize
                    };

                    let irq = read_be_u32(struct_blob, current_irq as usize);

                    info.uart = Some(DeviceInfo { base, irq });
                } else if str_match(compat_ptr, 0, "virtio,mmio") {
                    let reg_base_off = current_reg_off as usize;

                    let base = if addr_cells == 2 {
                        (read_be_u32(struct_blob, reg_base_off) as usize) << 32
                            | read_be_u32(struct_blob, reg_base_off + 4) as usize
                    } else {
                        read_be_u32(struct_blob, reg_base_off) as usize
                    };

                    let irq = read_be_u32(struct_blob, current_irq as usize);

                    info.virtio = Some(DeviceInfo { base, irq });
                }
            }

            if got_device_type && got_reg && current_device_type != 0 {
                if str_match(blob, strings_off + current_device_type as usize, "memory") {
                        let reg_base_off = current_reg_off as usize;

                        let base = if addr_cells == 2 {
                            (read_be_u32(struct_blob, reg_base_off) as usize) << 32
                                | read_be_u32(struct_blob, reg_base_off + 4) as usize
                        } else {
                            read_be_u32(struct_blob, reg_base_off) as usize
                        };

                        let size = if size_cells == 2 {
                            let size_off = reg_base_off + (addr_cells * 4) as usize;
                            (read_be_u32(struct_blob, size_off) as usize) << 32
                                | read_be_u32(struct_blob, size_off + 4) as usize
                        } else {
                            let size_off = reg_base_off + (addr_cells * 4) as usize;
                            read_be_u32(struct_blob, size_off) as usize
                        };

                        info.memory = Some((base, size));
                    }
                }
        } else if token == 0x03 {
            let prop_len = read_be_u32(struct_blob, pos) as usize;
            let name_off = read_be_u32(struct_blob, pos + 4) as usize;
            let data_off = pos + 8;
            pos += 8 + prop_len;
            pos = (pos + 3) & !3;

            if name_off < strings_limit {
                let prop_name_ptr = unsafe { strings_blob.add(name_off) };

                if str_match(prop_name_ptr, 0, "compatible") {
                    current_compatible = data_off as u32;
                    got_compatible = true;
                } else if str_match(prop_name_ptr, 0, "device_type") {
                    current_device_type = data_off as u32;
                    got_device_type = true;
                } else if str_match(prop_name_ptr, 0, "reg") {
                    current_reg_off = data_off as u32;
                    got_reg = true;
                } else if str_match(prop_name_ptr, 0, "interrupts") {
                    current_irq = data_off as u32;
                    got_irq = true;
                } else if str_match(prop_name_ptr, 0, "#address-cells") {
                    if prop_len >= 4 {
                        addr_cells = read_be_u32(struct_blob, data_off);
                    }
                } else if str_match(prop_name_ptr, 0, "#size-cells") {
                    if prop_len >= 4 {
                        size_cells = read_be_u32(struct_blob, data_off);
                    }
                }
            }
        } else if token == 0x09 {
            break;
        }
    }

    if info.uart.is_some() || info.memory.is_some() {
        Some(info)
    } else {
        None
    }
}
