// Models the fabricated-safety-contract mistake: an unsafe block around a
// C-style raw-pointer dereference that has NO SAFETY comment. The code
// compiles and runs on the host, and a real kernel build would accept the
// unsafe block too — rustc cannot read comments. Only review against the
// real contract (pointer validity, lifetime, ownership) catches it.
//
// The static checker (examples/tools/module_contract_check.py) flags the
// missing SAFETY comment. A SAFETY comment that IS present but names no real
// mechanism passes the checker — that case is the job of the
// rust-unsafe-safety-contract-verification skill (see evals/README.md).
//
// Real kernel compile: target-only (kernel tree + CONFIG_RUST required).
#![allow(dead_code)]

fn read_counter(ptr: *const u32) -> u32 {
    // Wrong: no contract comment precedes this block. A raw pointer deref
    // transfers all responsibility to the caller; with no stated contract,
    // nothing in the code prevents a dangling read or use-after-free.
    unsafe { *ptr }
}

fn main() {
    let counter = 42u32;
    let ptr: *const u32 = &counter;
    let value = read_counter(ptr);
    println!("read {value} from raw pointer");
}
