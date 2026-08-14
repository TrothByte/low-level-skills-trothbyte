// Rust-side layout probe, mirroring examples/good/c_side.c.
// Run: rustc --edition 2021 rust_layout.rs -o rust_layout && ./rust_layout
use std::mem::{align_of, offset_of, size_of};

#[repr(C)]
pub struct Header {
    pub magic: u32,
    pub ver: u8,
    pub _pad: [u8; 2],
    pub len: u32,
}

#[repr(C)]
pub enum Status {
    Ok = 0,
    Busy = 1,
    Error = 2,
}

#[repr(C)]
pub struct WithBool {
    pub b: bool,
    pub v: u32,
}

fn main() {
    println!(
        "header size={} align={} off_len={}",
        size_of::<Header>(),
        align_of::<Header>(),
        offset_of!(Header, len)
    );
    println!(
        "status size={} align={}",
        size_of::<Status>(),
        align_of::<Status>()
    );
    println!(
        "with_bool size={} align={} off_v={}",
        size_of::<WithBool>(),
        align_of::<WithBool>(),
        offset_of!(WithBool, v)
    );
}
