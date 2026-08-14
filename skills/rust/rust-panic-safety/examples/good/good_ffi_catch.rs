// GOOD: a panic is contained at the FFI boundary with `catch_unwind`.
// `extern "C"` frames must never unwind; the panic is translated to an error
// code so foreign callers (C without unwinding) never observe an unwind.
use std::ffi::c_int;
use std::panic::{self, AssertUnwindSafe};

#[no_mangle]
pub extern "C" fn process(v: c_int) -> c_int {
    match panic::catch_unwind(AssertUnwindSafe(|| work(v))) {
        Ok(code) => code,
        Err(_) => -1,
    }
}

fn work(v: c_int) -> c_int {
    if v < 0 {
        panic!("negative input");
    }
    v * 2
}

fn main() {
    assert_eq!(process(21), 42);
    assert_eq!(process(-1), -1);
    println!("OK");
}
