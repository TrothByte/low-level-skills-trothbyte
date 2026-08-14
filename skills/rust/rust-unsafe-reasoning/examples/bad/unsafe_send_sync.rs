// BAD: invalid unsafe impl Send.
// The impl promises BadWrapper is safe to move across threads, but it wraps
// Rc (non-atomic refcount, !Send) and Cell (non-atomic access, !Sync).
// Two threads then race on the same Cell contents and on the refcount drops:
// a data race, i.e. UB. Miri flags it; TSan flags it.
//
// Note on closures: with Rust 2021 disjoint captures, `move || w.inner.set(..)`
// captures only the field, so the wrapper's unsafe impl would not be consulted
// (E0277). Passing the whole wrapper into a helper moves it by value, which is
// how the lie becomes effective.
use std::cell::Cell;
use std::rc::Rc;
use std::thread;

struct BadWrapper {
    inner: Rc<Cell<u32>>,
}

// SAFETY (NOT): Rc's refcount is not atomic and Cell is not Sync, so this
// impl is a lie. It exists to show that the compiler accepts it.
unsafe impl Send for BadWrapper {}

fn racy(w: BadWrapper) {
    for _ in 0..1000 {
        w.inner.set(w.inner.get() + 1);
    }
}

fn main() {
    let shared = Rc::new(Cell::new(0u32));
    let w1 = BadWrapper {
        inner: Rc::clone(&shared),
    };
    let w2 = BadWrapper {
        inner: Rc::clone(&shared),
    };
    let t1 = thread::spawn(move || racy(w1));
    let t2 = thread::spawn(move || racy(w2));
    t1.join().unwrap();
    t2.join().unwrap();
    println!("{}", shared.get());
}
