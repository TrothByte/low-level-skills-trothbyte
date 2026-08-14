// Demonstrates the abort caused by panicking through extern "C".
// Build:  rustc --edition 2021 run_bad.rs -o run_bad
// Run:    ./run_bad   (expected: abort, exit status 0xC0000409)
use std::os::raw::c_int;
use std::panic::{self, AssertUnwindSafe};

#[no_mangle]
pub extern "C" fn run_bad(v: c_int) -> c_int {
    if v < 0 {
        panic!("negative input");
    }
    v
}

fn main() {
    let _ = panic::catch_unwind(AssertUnwindSafe(|| run_bad(-1)));
    println!("survived");
}
