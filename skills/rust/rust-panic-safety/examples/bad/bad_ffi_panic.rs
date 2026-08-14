// BAD: a panic unwinds through an `extern "C"` frame.
// The Rust Reference lists "unwinding past a stack frame that does not allow
// unwinding" as undefined behavior; rustc 1.71+ guards this by aborting the
// process at the boundary instead of unwinding into the caller.
use std::ffi::c_int;

#[no_mangle]
pub extern "C" fn process(v: c_int) -> c_int {
    if v < 0 {
        panic!("negative input");
    }
    v * 2
}

fn main() {
    let r = process(-1);
    println!("result = {r}");
}
