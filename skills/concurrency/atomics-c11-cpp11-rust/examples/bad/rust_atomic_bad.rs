// BAD: Rust atomic mistakes. Compiles; semantics wrong; one variant panics at
// runtime — that panic IS the Rust runtime enforcing the ordering precondition.
//   rustc --edition 2021 rust_atomic_bad.rs -o out && ./out   (exits with panic)
// Teaching: each function shows one bug class.
use std::sync::atomic::{AtomicBool, AtomicI32, AtomicU64, Ordering};

// B1: stale-current misuse. On failure the observed value is in the Err
// payload; `current` still holds the stale assumption (0), so the caller gets
// the wrong answer even though the slot holds 5.
fn stale_current(slot: &AtomicI32) -> i32 {
    let current = 0;
    match slot.compare_exchange(current, 1, Ordering::AcqRel, Ordering::Acquire) {
        Ok(_) => 1,
        Err(_observed) => current, // WRONG: must return _observed, not `current`
    }
}

// B2: one-shot claim without a retry loop. compare_exchange is the weak form
// and may fail spuriously, silently skipping the claim.
fn one_shot_weak(flag: &AtomicBool) -> bool {
    flag.compare_exchange(false, true, Ordering::AcqRel, Ordering::Acquire)
        .is_ok()
}

// B3: invalid ordering — Acquire on a store. store accepts only Relaxed,
// Release, or SeqCst. rustc rejects this at COMPILE time via the deny-by-
// default `invalid_atomic_ordering` lint; if that lint is allowed, the std
// runtime panics with "there is no such thing as an acquire store".
#[allow(invalid_atomic_ordering)]
fn store_with_acquire(flag: &AtomicBool) {
    flag.store(true, Ordering::Acquire);
}

// B4: relaxed flag protocol — no synchronizes-with edge for the reader.
fn publish_bad(ready: &AtomicBool) {
    ready.store(true, Ordering::Relaxed);
}

// B5: non-portable atomic assumption. Rust 1.97 guarantees available std
// atomic types are lock-free, but types may be ABSENT on some targets
// (AtomicU64 on 32-bit without hardware CAS). This code compiles on x86-64
// yet is not maximally portable; check cfg(target_has_atomic = "64") or use
// AtomicUsize.
fn handler_unsafe(slot: &AtomicU64) {
    slot.fetch_add(1, Ordering::Relaxed);
}

fn main() {
    let s = AtomicI32::new(5);
    println!("stale_current={} slot={}", stale_current(&s), s.load(Ordering::Relaxed));
    let f = AtomicBool::new(false);
    println!("one_shot_weak={}", one_shot_weak(&f));
    publish_bad(&f);
    let c = AtomicU64::new(0);
    handler_unsafe(&c);
    println!("count={}", c.load(Ordering::Relaxed));
    store_with_acquire(&f); // panics here — acquire store is invalid
}
