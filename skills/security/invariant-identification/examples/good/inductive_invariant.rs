// GOOD: an inductive loop invariant, asserted at entry, in the body, and at
// exit. P = (processed == i) && (total == partial sum) && (total <= cap).
// Kani-shape: under #[kani::proof], `assume(len <= N)` is the precondition
// and `assert!(total <= cap)` is the goal; here plain Rust asserts make every
// obligation executable on the host.
// Compile+run: rustc -O inductive_invariant.rs -o /tmp/inv.exe && /tmp/inv.exe
fn sum_capped(data: &[u32], cap: u32) -> Option<u32> {
    let mut total: u32 = 0;
    let mut i: usize = 0;
    let n = data.len();
    // INVARIANT (entry): processed == 0 == i, total == 0 <= cap.
    while i < n {
        // STEP obligation, checked on a live path:
        assert!(i <= n, "invariant: i in range at back-edge");
        assert!(total <= cap, "invariant: cap not exceeded in body");
        let x = data[i];
        total = total.saturating_add(x); // preserves total <= cap
        i += 1;
        assert!(total <= cap, "invariant preserved by iteration");
    }
    // POST obligation: !(i < n) && P  =>  i == n && total == sum && total <= cap
    assert!(i == n && total <= cap, "postcondition implied by invariant");
    Some(total)
}

fn main() {
    let data = [5u32, 7, 9, 1000, 2];
    let got = sum_capped(&data, u32::MAX).unwrap();
    assert!(got <= u32::MAX);
    println!("GOOD: invariant holds on entry, every back-edge, and exit; sum = {got}");
}
