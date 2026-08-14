// GOOD: the same FFI surface, done correctly.
use std::ffi::{c_char, c_int, CStr};
use std::panic::{self, AssertUnwindSafe};

// G1: layout pinned with repr(C) — verify with size_of/offsetof on both sides.
#[repr(C)]
pub struct Header {
    pub magic: u32,
    pub ver: u8,
    pub _pad: [u8; 2],
    pub len: u32,
}

// G2: ownership is explicit — "C allocates, C frees"; Rust only borrows.
// (Caller keeps ownership of `p`; we never free it.)
pub unsafe fn peek(p: *const c_char) -> Option<&'static str> {
    if p.is_null() {
        return None;
    }
    unsafe { CStr::from_ptr(p) }.to_str().ok()
}

// G3: panic contained at the boundary via catch_unwind; error code translated.
#[no_mangle]
pub extern "C" fn exported_good(v: c_int) -> c_int {
    let result = panic::catch_unwind(AssertUnwindSafe(|| work(v)));
    match result {
        Ok(code) => code,
        Err(_) => -1, // translate panic to error code; never unwind out
    }
}

fn work(v: c_int) -> c_int {
    if v < 0 {
        panic!("negative input");
    }
    v * 2
}
