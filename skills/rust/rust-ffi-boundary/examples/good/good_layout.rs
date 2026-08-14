// GOOD: layout pinned with #[repr(C)] and verified, padding explicit.
// Matches good_layout.h: size 12, align 4, len at offset 8.
use std::mem::{align_of, offset_of, size_of};

#[repr(C)]
pub struct Header {
    pub magic: u32,
    pub ver: u8,
    pub _pad: [u8; 2],
    pub len: u32,
}

#[no_mangle]
pub extern "C" fn header_size() -> usize {
    size_of::<Header>()
}

#[no_mangle]
pub extern "C" fn header_align() -> usize {
    align_of::<Header>()
}

#[no_mangle]
pub extern "C" fn header_off_len() -> usize {
    offset_of!(Header, len)
}
