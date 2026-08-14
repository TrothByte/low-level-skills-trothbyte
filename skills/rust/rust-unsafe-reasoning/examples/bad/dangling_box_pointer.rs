// BAD: dangling pointer to freed Box.
// Compiles and may print 7 (the freed memory is not yet reused), but reading
// through the raw pointer after the allocation was freed is use-after-free,
// i.e. UB. Miri flags it as "pointer to freed allocation".
fn main() {
    let raw = Box::into_raw(Box::new(7u32));
    unsafe {
        drop(Box::from_raw(raw)); // the allocation is freed here
    }
    let v = unsafe { *raw }; // UB: dangling dereference
    println!("{}", v);
}
