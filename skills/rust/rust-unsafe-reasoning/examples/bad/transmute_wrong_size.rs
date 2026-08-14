// BAD: transmute between differently-sized types.
// Expected: compile error E0512 "cannot transmute between types of different
// sizes, or dependently-sized types". rustc rejects this statically.
fn main() {
    let n: u32 = 42;
    let _b: [u8; 3] = unsafe { std::mem::transmute(n) };
}
