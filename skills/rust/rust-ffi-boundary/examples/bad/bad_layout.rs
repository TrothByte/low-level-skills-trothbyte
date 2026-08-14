// BAD: repr(C) mismatch with a packed C struct.
// bad_layout.h packs the fields (size 9, len at offset 5); this Rust struct
// assumes natural padding (size 12, len at offset 8). Every buffer read from
// C lands at the wrong offset, and arrays index at stride 12 instead of 9.
use std::mem::{align_of, size_of};

#[repr(C)]
pub struct Msg {
    pub magic: u32,
    pub ver: u8,
    pub len: u32,
}

#[no_mangle]
pub extern "C" fn msg_size() -> usize {
    size_of::<Msg>()
}

#[no_mangle]
pub extern "C" fn msg_align() -> usize {
    align_of::<Msg>()
}
