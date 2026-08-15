fn main() {
    // SAFETY: called in single-threaded startup, before any thread exists
    // and with no concurrent environment access (edition 2024 contract).
    unsafe { std::env::set_var("KILO_DEMO", "value") };
    println!("{}", std::env::var("KILO_DEMO").unwrap());
}
