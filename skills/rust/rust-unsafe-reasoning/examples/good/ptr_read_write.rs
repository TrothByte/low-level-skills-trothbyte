use std::ptr;

fn main() {
    let mut a = String::from("hello");
    let mut b = String::from("world");

    unsafe {
        // SAFETY: a and b are distinct and both initialized; each value is
        // moved exactly once: a is read, a is rewritten, b is read, b is
        // rewritten. No location is dropped while uninitialized.
        let tmp = ptr::read(&a);
        ptr::write(&mut a, ptr::read(&b));
        ptr::write(&mut b, tmp);
    }

    println!("a={} b={}", a, b);
}
