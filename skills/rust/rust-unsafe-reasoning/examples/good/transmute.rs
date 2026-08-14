use std::mem;

#[repr(C)]
#[derive(Clone, Copy)]
struct Word {
    hi: u16,
    lo: u16,
}

fn main() {
    let w = Word { hi: 0xDEAD, lo: 0xBEEF };
    // SAFETY: Word and [u8; 4] have equal size (asserted below), and every
    // byte bit pattern is a valid u8, so no validity invariant is violated.
    assert_eq!(mem::size_of::<Word>(), mem::size_of::<[u8; 4]>());
    let bytes: [u8; 4] = unsafe { mem::transmute(w) };
    println!("{:02x?}", bytes);
}
