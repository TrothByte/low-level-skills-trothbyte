// BAD: indexing with a user-controlled index without a bounds check.
// `data[i]` panics ("index out of bounds") when `i >= data.len()`.
use std::env;

fn lookup(data: &[u8], key: &str) -> u8 {
    let idx = key.trim().parse::<usize>().unwrap();
    data[idx]
}

fn main() {
    let data = [10u8, 20, 30];
    let key = env::args().nth(1).unwrap();
    println!("data[{key}] = {}", lookup(&data, &key));
}
