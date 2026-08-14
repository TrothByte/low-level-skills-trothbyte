fn fill(p: *mut u32, n: usize, value: u32) {
    for i in 0..n {
        // SAFETY: p points to an initialized buffer of at least n elements
        // (the caller guarantees it), and each add(i) stays in bounds.
        unsafe { *p.add(i) = value; }
    }
}

fn main() {
    let mut buf = [0u32; 4];
    {
        // The &mut borrow of buf lives only inside this scope; the raw
        // pointer is derived from it and used while no other access occurs.
        let p: *mut u32 = buf.as_mut_ptr();
        fill(p, buf.len(), 9);
    }
    println!("{:?}", buf);
}
