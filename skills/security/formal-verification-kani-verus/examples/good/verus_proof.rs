// GOOD: Verus proof stub — the exact target fixture for the documented
// command `verus --verify examples/good/verus_proof.rs`. Verus syntax is not
// rustc-compatible; this file is a target fixture only (not compiled here).
// Run with the Verus toolchain on a proper host:
//   verus --verify examples/good/verus_proof.rs
verus! {

// spec function: the logical specification of increment.
spec fn spec_inc(x: u32) -> u32 {
    x + 1
}

// executable function with an ensures clause connected to the spec.
fn inc(x: u32) -> (r: u32)
    ensures
        r == spec_inc(x),
{
    proof {
        // GOOD: proof connects the executable to the spec.
        assert(spec_inc(x) == x + 1);
    }
    x + 1 // returns the spec-matching value
}

fn main() {
    // GOOD: the caller can rely on the ensures.
    let y = inc(41);
    assert(y == 42);
}

} // verus!
