// BAD: vacuous "proof" — the harness has no assertion about the property and
// only exercises a single literal input. A Kani run over this would return
// "no bug found" while proving nothing about the real code.
// intentionally incorrect
fn checked_add_saturating(x: u32, y: u32) -> u32 {
    x.saturating_add(y)
}

fn main() {
    // BAD: single literal input, no assertion — vacuous.
    let r = checked_add_saturating(1, 2);
    let _ = r; // BAD: result never checked against any property
    println!("harness: no property asserted (vacuously 'passes')");
}
