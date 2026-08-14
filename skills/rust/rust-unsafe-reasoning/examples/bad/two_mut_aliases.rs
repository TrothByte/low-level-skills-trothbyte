// BAD: two mutable references to the same data.
// The borrow checker rejects this (E0499), which is exactly the point: the
// aliasing rule exists in safe code. The unsafe equivalent (two raw pointers
// derived from two &mut to one location, both written) compiles but is UB
// under the aliasing model and is flagged by Miri.
fn main() {
    let mut x = 5u32;
    let a = &mut x;
    let b = &mut x; // E0499: cannot borrow `x` as mutable more than once
    *a += 1;
    *b += 1;
    println!("{}", x);
}
