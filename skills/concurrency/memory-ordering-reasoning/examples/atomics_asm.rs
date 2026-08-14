//! Verified asm artifact: Rust atomics on x86-64 (rustc 1.97.1, opt-level 2).
//!
//! Compile: `rustc --crate-type=lib --emit=asm -C opt-level=2 atomics.rs`
//! See the generated store/load/fetch_add bodies — table in references/memory-ordering.md.
//!
//! Key results on x86:
//!   store_relaxed      -> movl %edx, (%rcx)          (plain store)
//!   store_seqcst       -> xchgl %edx, (%rcx)         (locked exchange)
//!   load_acquire       -> movl (%rcx), %eax          (x86 loads are acquire-strong)
//!   fetch_add_relaxed  -> lock xaddl %eax, (%rcx)
//!   fetch_add_seqcst   -> lock xaddl %eax, (%rcx)    (identical on x86)
//!
//! Teaching point: on x86 TSO, RMWs are identical for Relaxed and SeqCst; the ordering
//! differences surface on weakly-ordered targets (AArch64 stlr/ldar, RISC-V fence).

use std::sync::atomic::{AtomicI32, Ordering};

#[no_mangle]
pub extern "C" fn store_relaxed(a: &AtomicI32, v: i32) {
    a.store(v, Ordering::Relaxed);
}

#[no_mangle]
pub extern "C" fn store_seqcst(a: &AtomicI32, v: i32) {
    a.store(v, Ordering::SeqCst);
}

#[no_mangle]
pub extern "C" fn load_acquire(a: &AtomicI32) -> i32 {
    a.load(Ordering::Acquire)
}

#[no_mangle]
pub extern "C" fn fetch_add_relaxed(a: &AtomicI32) -> i32 {
    a.fetch_add(1, Ordering::Relaxed)
}

#[no_mangle]
pub extern "C" fn fetch_add_seqcst(a: &AtomicI32) -> i32 {
    a.fetch_add(1, Ordering::SeqCst)
}
