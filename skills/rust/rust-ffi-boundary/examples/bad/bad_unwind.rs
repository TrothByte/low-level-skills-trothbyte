// BAD: panic unwinds through extern "C".
// Compiles with no warning on rustc 1.97.1. At runtime the unwinder cannot
// cross the C ABI frame: the process aborts ("panic in a function that cannot
// unwind", exit status 0xC0000409). From a real C caller this is UB.
use std::os::raw::c_int;

#[no_mangle]
pub extern "C" fn run_bad(v: c_int) -> c_int {
    if v < 0 {
        panic!("negative input");
    }
    v
}
