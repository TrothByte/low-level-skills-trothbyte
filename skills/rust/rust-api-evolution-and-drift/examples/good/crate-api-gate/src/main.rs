#[deprecated(since = "0.2.0", note = "use `parse_config` instead")]
fn parse_config_legacy() -> u32 {
    1
}

fn parse_config() -> u32 {
    2
}

fn main() {
    let v = parse_config();
    #[allow(deprecated)]
    let legacy = parse_config_legacy();
    println!("{} {}", v, legacy);
    println!("declared rust-version: {}", env!("CARGO_PKG_RUST_VERSION"));
}
