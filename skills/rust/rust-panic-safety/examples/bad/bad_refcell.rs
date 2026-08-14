// BAD: a `RefCell` borrow panics in a hot path reached per-request.
// `borrow_mut()` while a borrow is already active panics; here a request that
// re-enters the handler while the previous handler still holds the borrow
// kills the process. This is a panic-reachability DoS.
use std::cell::RefCell;
use std::rc::Rc;

struct Cache {
    inner: RefCell<Vec<u8>>,
}

impl Cache {
    fn update(&self, data: u8) {
        let mut v = self.inner.borrow_mut();
        v.push(data);
        self.on_dirty();
    }

    fn on_dirty(&self) {
        let v = self.inner.borrow();
        println!("len = {}", v.len());
    }
}

fn main() {
    let cache = Rc::new(Cache {
        inner: RefCell::new(Vec::new()),
    });
    cache.update(1);
}
