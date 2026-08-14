// GOOD: catch_unwind + AssertUnwindSafe + resume_unwind, and Drop during unwind.
use std::panic::{self, AssertUnwindSafe};

struct Guard;

impl Drop for Guard {
    fn drop(&mut self) {
        println!("guard dropped (destructor runs during unwind)");
    }
}

fn panicky(x: &mut i32) {
    *x += 1;
    panic!("boom");
}

fn main() {
    // AssertUnwindSafe: catch_unwind requires UnwindSafe; here a &mut is
    // captured, so the wrapper asserts the capture is safe to unwind across.
    let mut state = 0i32;
    let r1 = panic::catch_unwind(AssertUnwindSafe(|| {
        let _g = Guard;
        panicky(&mut state);
    }));
    assert!(r1.is_err());
    let msg = *r1.err().unwrap().downcast_ref::<&str>().unwrap();
    assert_eq!(msg, "boom");

    // resume_unwind re-throws the payload to the next enclosing catch.
    let r2 = panic::catch_unwind(AssertUnwindSafe(|| {
        let payload = panic::catch_unwind(|| panic!("inner")).err().unwrap();
        panic::resume_unwind(payload);
    }));
    assert!(r2.is_err());

    println!("OK");
}
