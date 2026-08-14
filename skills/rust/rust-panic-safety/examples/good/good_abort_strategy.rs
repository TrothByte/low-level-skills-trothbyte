// GOOD: the panic strategy is a build decision made explicit.
// With `panic=unwind` (default) `catch_unwind` can recover; with
// `panic=abort` a panic terminates the process and `catch_unwind` never
// returns. The trade-off is compiled in and detected here with `cfg!(panic = ..)`.
fn main() {
    if cfg!(panic = "unwind") {
        let r = std::panic::catch_unwind(|| panic!("recoverable"));
        assert!(r.is_err());
        println!("strategy: unwind, catch_unwind caught the panic");
    } else if cfg!(panic = "abort") {
        println!("strategy: abort, catch_unwind would never return");
    }
    println!("OK");
}
