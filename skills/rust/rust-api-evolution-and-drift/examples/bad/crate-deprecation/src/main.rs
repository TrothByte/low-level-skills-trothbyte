// intentionally incorrect
#[deprecated(since = "0.2.0", note = "use `parse_config` instead")]
fn parse_config_legacy() -> u32 {
    1
}

fn main() {
    let v = parse_config_legacy();
    println!("{}", v);
}
