// BAD: untrusted input reaches `unwrap()`. One malformed request kills the process.
use std::env;

fn parse_id(raw: &str) -> u32 {
    raw.trim().parse::<u32>().unwrap()
}

fn main() {
    let input: Vec<String> = env::args().skip(1).collect();
    let id = parse_id(&input[0]);
    println!("id = {id}");
}
