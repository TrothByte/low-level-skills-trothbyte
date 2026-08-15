use std::marker::PhantomData;

pub struct Borrowed<'a, T> {
    ptr: *const T,
    _marker: PhantomData<&'a T>,
}

impl<'a, T> Borrowed<'a, T> {
    pub fn new(r: &'a T) -> Self {
        // SAFETY: the borrow checker rejects any Borrowed<'a, _> used beyond
        // 'a because PhantomData<&'a T> ties the struct to the borrow.
        Borrowed { ptr: r as *const T, _marker: PhantomData }
    }
    pub fn get(&self) -> &'a T {
        // SAFETY: ptr points into memory that stays alive for 'a, enforced by
        // the PhantomData<&'a T> field and the borrow checker.
        unsafe { &*self.ptr }
    }
}

pub struct RawBuf {
    ptr: *const u8,
}

impl RawBuf {
    pub unsafe fn wrap(ptr: *const u8) -> Self {
        RawBuf { ptr }
    }
    pub fn read(&self) -> u8 {
        // SAFETY: claimed in `wrap`; nothing in the type system enforces it.
        unsafe { *self.ptr }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn borrowed_reads_the_value() {
        let v = 42u32;
        let b = Borrowed::new(&v);
        assert_eq!(*b.get(), 42);
    }

    #[test]
    fn fake_contract_compiles_but_has_no_guard() {
        let v = [1u8, 2, 3];
        let r = unsafe { RawBuf::wrap(v.as_ptr()) };
        assert_eq!(r.read(), 1);
    }
}
