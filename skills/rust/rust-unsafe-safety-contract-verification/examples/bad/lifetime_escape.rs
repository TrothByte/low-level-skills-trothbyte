// intentionally incorrect
// Trying to escape the lifetime: the PhantomData<&'a T> encoding makes this
// a compile error (E0597). This is the proof that the invariant in
// examples/good/phantomdata_invariant.rs is real, unlike the fabricated
// contract in fake_safety_contract.rs.
use std::marker::PhantomData;

struct Borrowed<'a, T> {
    ptr: *const T,
    _marker: PhantomData<&'a T>,
}

impl<'a, T> Borrowed<'a, T> {
    fn new(r: &'a T) -> Self {
        Borrowed { ptr: r as *const T, _marker: PhantomData }
    }
    fn get(&self) -> &'a T {
        unsafe { &*self.ptr }
    }
}

fn main() {
    let b: Borrowed<'static, u32>;
    {
        let v = 42u32;
        b = Borrowed::new(&v);
    }
    println!("{}", b.get());
}
