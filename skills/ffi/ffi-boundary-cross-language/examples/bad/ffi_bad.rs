// BAD: FFI mistakes in Rust (compile where noted).
use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_int};

// B1: layout not pinned — no repr(C).
struct Header {
    magic: u32,
    ver: u8,
    len: u32,
}

// B2: double ownership of a C string (double-free).
unsafe fn take_twice(p: *mut c_char) {
    let a = CString::from_raw(p);
    let b = CString::from_raw(p); // double free / UB
    drop(a);
    drop(b);
}

// B3: panic unwinds through extern "C" (UB with panic=unwind).
#[no_mangle]
pub extern "C" fn exported_bad(v: c_int) -> c_int {
    if v < 0 {
        panic!("negative input"); // unwinds across the FFI boundary — UB
    }
    v
}

// B4: closure passed as fn pointer (won't compile — the lesson).
// fn register_bad(cb: impl Fn() + 'static) { /* cb is not a C fn pointer */ }
