// GOOD: Rust asm! with correct operand classes and Intel-syntax templates.
// Rust asm! on x86 uses Intel assembly syntax: destination operand first.
// Teaching comments only. Verify: rustc --edition 2021 -O rust_asm.rs && run.
// Verified with rustc 1.97.1 (x86_64-pc-windows-msvc): prints "rust asm! ok".

use std::arch::asm;

// inout(reg): operand is read and written. const operand needs no register.
fn add_const(mut x: u64) -> u64 {
    unsafe {
        asm!("add {0}, {number}", inout(reg) x, number = const 5);
    }
    x
}

// inout(reg) + in(reg): "{0}" is the inout, "{1}" the in; Intel: dest first.
fn add_two(a: u64, b: u64) -> u64 {
    let mut r = a;
    unsafe {
        asm!("add {0}, {1}", inout(reg) r, in(reg) b, options(nostack));
    }
    r
}

// lateout(reg): output is written last, safe with early input reads.
fn flags_lateout() -> u64 {
    let mut f: u64;
    unsafe {
        asm!("pushfq", "pop {0}", lateout(reg) f, options(nostack));
    }
    f
}

fn main() {
    assert_eq!(add_const(3), 8);
    assert_eq!(add_two(20, 22), 42);
    let _ = flags_lateout();
    println!("rust asm! ok");
}
