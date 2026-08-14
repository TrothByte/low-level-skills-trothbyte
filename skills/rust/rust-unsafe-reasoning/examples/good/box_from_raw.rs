fn main() {
    let boxed = Box::new(41u64);
    let raw = Box::into_raw(boxed);
    // SAFETY: raw was produced by Box::into_raw (non-null, aligned, correct
    // layout), and this is the only from_raw that reclaims its ownership.
    let boxed = unsafe { Box::from_raw(raw) };
    println!("{}", *boxed + 1);
}
