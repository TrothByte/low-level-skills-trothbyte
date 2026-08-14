// GOOD: indexing is bounds-checked via `.get()`; out-of-range input becomes
// an error instead of a panic.
use std::env;

fn lookup(data: &[u8], key: &str) -> Result<u8, String> {
    let idx = key.trim().parse::<usize>().map_err(|_| "bad index")?;
    data.get(idx).copied().ok_or_else(|| format!("index {idx} out of bounds"))
}

fn main() {
    let data = [10u8, 20, 30];
    let key = match env::args().nth(1) {
        Some(k) => k,
        None => {
            println!("no key supplied");
            println!("OK");
            return;
        }
    };
    match lookup(&data, &key) {
        Ok(v) => println!("data[{key}] = {v}"),
        Err(e) => println!("rejected: {e}"),
    }
    println!("OK");
}
