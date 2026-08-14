// GOOD: untrusted input is handled with `Result`; no panic is reachable.
use std::env;

fn parse_id(raw: &str) -> Result<u32, String> {
    raw.trim()
        .parse::<u32>()
        .map_err(|_| format!("invalid id: {raw:?}"))
}

fn main() {
    let input = env::args().nth(1);
    match input.as_deref().map(parse_id) {
        Some(Ok(id)) => println!("id = {id}"),
        Some(Err(e)) => println!("rejected: {e}"),
        None => println!("rejected: no id supplied"),
    }
    println!("OK");
}
