// intentionally incorrect
// env::set_var became `unsafe fn` in edition 2024. Calling it without an
// unsafe block is E0133 under edition 2024, although this exact code compiled
// cleanly under edition 2021.
fn main() {
    std::env::set_var("KILO_DEMO", "value");
}
