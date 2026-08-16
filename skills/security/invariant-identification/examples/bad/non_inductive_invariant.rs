// BAD: a non-inductive invariant certified by a harness that cannot fail.
// The claimed invariant "i <= n" is destroyed by the loop body (i += 2 can
// overshoot n), and the postcondition is never implied. Worse, the
// "verification" is gated: in a real Kani build this file would carry
// `#[cfg(not(kani))]` on the assert so the tool checks nothing. On the host
// the fixture prints PASS regardless.
// Compile+run: rustc -O non_inductive_invariant.rs -o /tmp/invb.exe && /tmp/invb.exe
// Marker: intentionally incorrect
#[allow(unused_macros)]
macro_rules! kani_assert {
    ($e:expr) => { /* intentionally incorrect: empty in host builds */ };
}

fn sum_stepping(data: &[u32]) -> u32 {
    let mut total: u32 = 0;
    let mut i: usize = 0;
    let n = data.len();
    // Claimed invariant: i <= n. Base holds (0 <= n). STEP FAILS: i += 2
    // can move i to n+1 when n is odd, so i <= n is destroyed.
    while i < n {
        // intentionally incorrect: this assert is compiled out in host
        // builds, and would be gated under cfg(not(kani)) in a real harness.
        kani_assert!(i <= n);
        total = total.wrapping_add(data[i]);
        i += 2;
    }
    // Post claim: total == sum of elements. Not implied by i <= n at all.
    total
}

fn main() {
    let data = [1u32, 2, 3]; // n = 3, i goes 0 -> 2 -> 4 (overshoot)
    let got = sum_stepping(&data);
    // intentionally incorrect: no check that the invariant ever held;
    // "PASS" is printed even though the invariant is false mid-loop.
    println!("PASS: harness reports verified (invariant i <= n), sum = {got}");
    println!("BAD: the invariant fails at the back-edge when i overshoots n; the gated assert hid it");
}
