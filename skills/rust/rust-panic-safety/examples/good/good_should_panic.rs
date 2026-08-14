// GOOD: `#[should_panic]` pins the panic contract in tests.
// Compile with `rustc --test`; the harness passes only if the function panics.
fn oob_index() -> u8 {
    let data = [1u8, 2, 3];
    let idx: usize = data.len() + 4; // runtime value, not const-foldable
    data[idx]
}

#[test]
#[should_panic(expected = "index out of bounds")]
fn oob_panics() {
    let _ = oob_index();
}

#[test]
#[should_panic(expected = "invalid id")]
fn parse_rejects_garbage() {
    let _: u32 = "not-a-number".parse().expect("invalid id");
}
