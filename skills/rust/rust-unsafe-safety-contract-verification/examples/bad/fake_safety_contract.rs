// intentionally incorrect
// SAFETY: caller guarantees the buffer outlives this handle.
// This is a fabricated contract: no lifetime, no PhantomData, nothing
// encodes the guarantee. The buffer is dropped, the pointer dangles, and
// the read is use-after-free — it compiles and prints garbage.
struct RawBuf {
    ptr: *const u8,
}

impl RawBuf {
    unsafe fn wrap(ptr: *const u8) -> Self {
        RawBuf { ptr }
    }
    fn read(&self) -> u8 {
        unsafe { *self.ptr }
    }
}

fn main() {
    let h = {
        let buf = String::from("hello");
        let h = unsafe { RawBuf::wrap(buf.as_ptr()) };
        drop(buf);
        h
    };
    println!("{}", h.read());
}
