// BAD: GCC AT&T habits carried into Rust asm!, which uses Intel syntax on x86
// (destination operand first). The template reads as "b += r", so r is never
// updated and the function returns a instead of a + b. Compiles without
// warnings and silently returns the wrong value. Verified with rustc 1.97.1.
// The good twin is in examples/good/rust_asm.rs (add_two).

use std::arch::asm;

fn add_wrong(a: u32, b: u32) -> u32 {
    let mut r = a;
    unsafe {
        asm!("add {1:e}, {0:e}", inout(reg) r, in(reg) b, options(nostack));
    }
    r
}

fn main() {
    assert_eq!(add_wrong(20, 22), 42);
    println!("unreachable: add_wrong returned {}", add_wrong(20, 22));
}
