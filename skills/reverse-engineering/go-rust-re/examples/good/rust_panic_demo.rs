// rust_panic_demo.rs — a tiny Rust program with a reachable panic path and
// formatting machinery, used to demonstrate Rust-binary RE markers:
//   mangled symbols (_ZN... / _RNv...), rust_begin_unwind, core::fmt strings,
//   and panic message literals in .rodata/.rdata.
// Build (see evals/README.md):
//   rustc -O rust_panic_demo.rs -o rust_panic_demo.exe
use std::env;

fn main() {
    let args: Vec<String> = env::args().collect();
    let n: i32 = match args.get(1) {
        Some(s) => s.parse().unwrap_or(3),
        None => 3,
    };
    println!("double: {}", double(n));
    let v = vec![10i32, 20, 30];
    let idx: usize = args
        .get(2)
        .map(|s| s.parse().unwrap_or(0))
        .unwrap_or(0);
    println!("pick: {}", pick(&v, idx));
}

fn double(x: i32) -> i32 {
    if x < 0 {
        panic!("negative input: {}", x);
    }
    x * 2
}

fn pick(v: &[i32], i: usize) -> i32 {
    v[i]
}
