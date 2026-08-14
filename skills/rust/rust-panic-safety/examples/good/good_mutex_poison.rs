// GOOD: `Mutex::lock` poisoning is handled explicitly instead of unwrapped.
// A thread that panics while holding the lock poisons it; subsequent locks
// return `Err(PoisonError)`. Here the state is known-safe after a panic in a
// worker, so we recover the guard with `into_inner()`.
use std::sync::{Arc, Mutex};
use std::thread;

fn main() {
    let counter = Arc::new(Mutex::new(0u32));

    let worker = {
        let c = Arc::clone(&counter);
        thread::spawn(move || {
            let _guard = c.lock().unwrap();
            panic!("worker died mid-update");
        })
    };
    let _ = worker.join();

    let recovered = match counter.lock() {
        Ok(guard) => guard,
        Err(poisoned) => poisoned.into_inner(),
    };
    assert!(counter.is_poisoned());
    assert_eq!(*recovered, 0);
    println!("OK");
}
