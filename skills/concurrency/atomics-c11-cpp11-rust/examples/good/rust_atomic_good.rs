// GOOD: correct Rust std::sync::atomic usage. Compile:
//   rustc --edition 2021 rust_atomic_good.rs -o out && ./out
// Teaching: compare_exchange returns Result<T,T>; the Err payload is the
// observed value (the in-out "expected" of C/C++). There is no strong variant,
// so every CAS is weak and must sit in a retry loop. Race-freedom of the
// release/acquire pair is verified with TSan/Miri (see evals/README.md).
use std::sync::atomic::{AtomicBool, AtomicI32, Ordering};

// G1: CAS loop — refresh the expected value from the Err payload.
fn publish_once(slot: &AtomicI32) -> i32 {
    let mut expected = 0;
    loop {
        match slot.compare_exchange(expected, 1, Ordering::AcqRel, Ordering::Acquire) {
            Ok(_) => return 1,
            Err(observed) => {
                if observed != 0 {
                    return observed; // someone else already set it
                }
                expected = observed; // spurious failure: retry with the fresh value
            }
        }
    }
}

// G2: one-shot claim. Rust has only the weak form, so a loop is mandatory;
// Err(observed) with observed==true means someone else claimed first.
fn claim_once(flag: &AtomicBool) -> bool {
    let mut expected = false;
    loop {
        match flag.compare_exchange(expected, true, Ordering::AcqRel, Ordering::Acquire) {
            Ok(_) => return true,
            Err(observed) => {
                if observed {
                    return false;
                }
                expected = observed; // spurious failure: retry
            }
        }
    }
}

// G3: relaxed is correct for a lossy stats counter.
fn bump(c: &AtomicI32) {
    c.fetch_add(1, Ordering::Relaxed);
}

// G4: release/acquire publish protocol. In real code the non-atomic data is
// written BEFORE store(Release) and read only AFTER load(Acquire) establishes
// the synchronizes-with edge; race-freedom of that pair is a TSan/Miri check.
static READY: AtomicBool = AtomicBool::new(false);

fn publish(_data: i32) {
    READY.store(true, Ordering::Release);
}

fn consume() -> Option<i32> {
    if READY.load(Ordering::Acquire) {
        Some(42) // would be the published data; safe only because of the edge
    } else {
        None
    }
}

fn main() {
    let slot = AtomicI32::new(0);
    let r = publish_once(&slot);
    println!("publish_once r={} slot={}", r, slot.load(Ordering::SeqCst));
    let flag = AtomicBool::new(false);
    println!("claim_once={} flag={}", claim_once(&flag), flag.load(Ordering::SeqCst));
    let c = AtomicI32::new(0);
    bump(&c);
    bump(&c);
    bump(&c);
    println!("count={}", c.load(Ordering::Relaxed));
    publish(42);
    println!("consume={:?}", consume());
    // Rust 1.97 guarantees all available std atomic types are lock-free; the
    // portability concern is type AVAILABILITY, handled by cfg(target_has_atomic).
}
