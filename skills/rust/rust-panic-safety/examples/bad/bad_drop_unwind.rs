// BAD: `Drop` panics while another panic is unwinding. A second panic during
// unwinding aborts the process ("thread panicked while panicking").
struct Cleanup;

impl Drop for Cleanup {
    fn drop(&mut self) {
        panic!("cleanup failure");
    }
}

fn work() {
    let _cleanup = Cleanup;
    panic!("operation failed");
}

fn main() {
    work();
}
