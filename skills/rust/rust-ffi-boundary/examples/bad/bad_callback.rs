// BAD: a capturing closure is not a C function pointer.
// A closure that captures state has no stable code address and cannot coerce
// to `extern "C" fn`. This file fails to compile on purpose (E0308).
use std::os::raw::c_int;

#[no_mangle]
pub extern "C" fn register_cb(cb: extern "C" fn(c_int)) -> c_int {
    cb(1);
    0
}

pub fn install() {
    let n = 7;
    let closure = |x: c_int| {
        let _ = x + n; // captures n -> not a fn pointer
    };
    register_cb(closure);
}
