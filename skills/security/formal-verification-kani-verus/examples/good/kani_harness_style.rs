// GOOD: Kani-style proof harness, compilable as a plain Rust program on this
// host to demonstrate the harness *structure* (the real Kani run needs the
// kani toolchain, documented in SKILL.md). The property is asserted with a
// symbolic-looking input pattern and checked at runtime here.
// Compile: rustc --edition 2021 kani_harness_style.rs -o /tmp/kani_style && /tmp/kani_style
fn checked_add_saturating(x: u32, y: u32) -> u32 {
    x.saturating_add(y)
}

fn main() {
    // GOOD: harness covers an input range (stand-in for kani::any()) and
    // asserts the property on the result — the assertion names the invariant.
    for x in 0u32..1000u32 {
        for y in 0u32..1000u32 {
            let r = checked_add_saturating(x, y);
            assert!(r >= x, "saturating add never decreases"); // the property
            assert!(r >= y, "saturating add never decreases");
        }
    }
    println!("harness: property holds for sampled inputs");
}
