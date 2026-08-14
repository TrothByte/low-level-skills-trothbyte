// GOOD: opaque handle with a single owner.
// token_new boxes a Token and returns the raw pointer (Box::into_raw);
// token_free reconstructs the Box exactly once (Box::from_raw). C only
// stores and passes the pointer; ownership stays on the Rust side.
use std::ffi::{c_char, CStr};

pub struct Token {
    name: String,
}

#[no_mangle]
pub extern "C" fn token_new(name: *const c_char) -> *mut Token {
    let owned = if name.is_null() {
        String::new()
    } else {
        unsafe { CStr::from_ptr(name) }.to_string_lossy().into_owned()
    };
    Box::into_raw(Box::new(Token { name: owned }))
}

// Safety: p must come from token_new (Box::into_raw) and be passed exactly
// once. Null is a valid no-op.
#[no_mangle]
pub unsafe extern "C" fn token_free(p: *mut Token) {
    if !p.is_null() {
        unsafe { drop(Box::from_raw(p)) }
    }
}

// Safety: p must be non-null and point to a live Token. The returned pointer
// borrows the token's name and is invalid after token_free.
#[no_mangle]
pub unsafe extern "C" fn token_name(p: *const Token) -> *const c_char {
    if p.is_null() {
        std::ptr::null()
    } else {
        unsafe { (*p).name.as_ptr() as *const c_char }
    }
}
