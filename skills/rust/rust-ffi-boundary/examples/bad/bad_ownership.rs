// BAD: two owners for one allocation.
// CString::from_raw is a take-ownership contract; calling it twice creates
// two owners of the same buffer, so both drops free it -> double free.
// The file compiles; the bug is a soundness violation, not a syntax error.
#![allow(dead_code)]

use std::ffi::CString;
use std::os::raw::c_char;

unsafe fn take_twice(p: *mut c_char) {
    let a = CString::from_raw(p);
    let b = CString::from_raw(p);
    drop(a);
    drop(b);
}
