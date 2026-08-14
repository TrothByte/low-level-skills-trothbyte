// GOOD: panic contained at the boundary; no unwinding crosses extern "C".
// catch_unwind captures the panic and the export translates it to an error
// code, so a C caller always sees a normal return.
use std::os::raw::c_int;
use std::panic::{self, AssertUnwindSafe};

#[no_mangle]
pub extern "C" fn run_good(v: c_int) -> c_int {
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
