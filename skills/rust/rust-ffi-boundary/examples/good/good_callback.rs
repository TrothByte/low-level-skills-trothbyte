// GOOD: C callback ABI = extern "C" fn + opaque user-data pointer.
// State travels through the context pointer, never through closure capture.
// C stores `cb` and `user`; each invocation casts user back to the context.
use std::ffi::{c_char, c_void};
use std::os::raw::c_longlong;

pub type VisitFn = extern "C" fn(key: *const c_char, value: c_longlong, user: *mut c_void);

// Safety: user must point to a Visitor while this callback is installed.
pub unsafe extern "C" fn on_visit(_key: *const c_char, value: c_longlong, user: *mut c_void) {
    let ctx = unsafe { &mut *(user as *mut Visitor) };
    ctx.sum += value;
}

pub struct Visitor {
    pub sum: c_longlong,
}
