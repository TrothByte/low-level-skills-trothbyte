use std::marker::PhantomData;

struct Borrowed<'a, T> {
    ptr: *const T,
    _marker: PhantomData<&'a T>,
}

impl<'a, T> Borrowed<'a, T> {
    fn new(r: &'a T) -> Self {
        // SAFETY: `r` keeps the pointee alive for `'a`; the borrow checker
        // rejects any Borrowed<'a, _> that outlives `'a` (PhantomData<&'a T>).
        Borrowed { ptr: r as *const T, _marker: PhantomData }
    }
    fn get(&self) -> &'a T {
        // SAFETY: ptr was derived from a reference alive for `'a`.
        unsafe { &*self.ptr }
    }
}

fn main() {
    let v = 42u32;
    let b = Borrowed::new(&v);
    println!("{}", b.get());
}
