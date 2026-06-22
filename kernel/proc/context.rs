use super::task::Context;

pub fn save_context(ctx: &mut Context) {
    unsafe {
        core::arch::asm!(
            "sd ra,  0({0})",
            "sd sp,  8({0})",
            "sd s0,  16({0})",
            "sd s1,  24({0})",
            "sd s2,  32({0})",
            "sd s3,  40({0})",
            "sd s4,  48({0})",
            "sd s5,  56({0})",
            "sd s6,  64({0})",
            "sd s7,  72({0})",
            "sd s8,  80({0})",
            "sd s9,  88({0})",
            "sd s10, 96({0})",
            "sd s11, 104({0})",
            in(reg) ctx,
            options(nostack, preserves_flags)
        );
    }
}

pub fn restore_context(ctx: &Context) {
    unsafe {
        core::arch::asm!(
            "ld ra,  0({0})",
            "ld sp,  8({0})",
            "ld s0,  16({0})",
            "ld s1,  24({0})",
            "ld s2,  32({0})",
            "ld s3,  40({0})",
            "ld s4,  48({0})",
            "ld s5,  56({0})",
            "ld s6,  64({0})",
            "ld s7,  72({0})",
            "ld s8,  80({0})",
            "ld s9,  88({0})",
            "ld s10, 96({0})",
            "ld s11, 104({0})",
            in(reg) ctx,
            options(nostack, preserves_flags)
        );
    }
}
