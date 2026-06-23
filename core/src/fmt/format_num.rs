//! Numeric formatting helpers — `write_dec` and `write_hex_or_dec`.
use super::writer::FmtSpec;

pub(super) fn write_dec<W: super::writer::Write>(out: &mut W, v: i64, spec: FmtSpec) {
    let (neg, digits) = if v < 0 {
        let abs = if v == i64::MIN {
            1u64 << 63
        } else {
            (-v) as u64
        };
        (true, abs)
    } else {
        (false, v as u64)
    };
    let mut buf = [0u8; 20];
    let mut len = 0;
    let mut n = digits;
    if n == 0 {
        buf[0] = b'0';
        len = 1;
    } else {
        while n > 0 {
            buf[len] = b'0' + (n % 10) as u8;
            len += 1;
            n /= 10;
        }
    }
    let total = len + if neg { 1 } else { 0 };
    let pad = spec.width.saturating_sub(total);
    if spec.zero_pad {
        if neg {
            out.write_char(b'-');
        }
        for _ in 0..pad {
            out.write_char(b'0');
        }
    } else {
        for _ in 0..pad {
            out.write_char(b' ');
        }
        if neg {
            out.write_char(b'-');
        }
    }
    for k in (0..len).rev() {
        out.write_char(buf[k]);
    }
}

pub(super) fn write_hex_or_dec<W: super::writer::Write>(
    out: &mut W,
    v: u64,
    spec: FmtSpec,
    hex: bool,
) {
    let mut buf = [0u8; 20];
    let mut len = 0;
    let mut n = v;
    if n == 0 {
        buf[0] = b'0';
        len = 1;
    } else if hex {
        const HEX: &[u8] = b"0123456789abcdef";
        while n > 0 {
            buf[len] = HEX[(n & 0xF) as usize];
            len += 1;
            n >>= 4;
        }
    } else {
        while n > 0 {
            buf[len] = b'0' + (n % 10) as u8;
            len += 1;
            n /= 10;
        }
    }
    let pad = spec.width.saturating_sub(len);
    let pad_ch = if spec.zero_pad { b'0' } else { b' ' };
    for _ in 0..pad {
        out.write_char(pad_ch);
    }
    for k in (0..len).rev() {
        out.write_char(buf[k]);
    }
}
