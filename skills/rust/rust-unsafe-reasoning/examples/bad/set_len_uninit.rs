// BAD: uninitialized Vec via set_len without writing the elements.
// Compiles and prints whatever the allocator left behind, but reading the
// uninitialized bytes is UB ("reading uninitialized memory"). Miri flags it.
fn main() {
    let mut v: Vec<u8> = Vec::with_capacity(4);
    unsafe {
        v.set_len(4); // claims 4 elements are initialized
    }
    println!("{:?}", v); // UB: reads uninitialized bytes
}
