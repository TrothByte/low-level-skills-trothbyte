// intentionally incorrect
// Written for edition 2018 semantics: array.into_iter() borrowed. Since
// edition 2021 it moves, so dereferencing the element is E0614 and using the
// array afterwards would be E0382. Compiles under 2018, fails under 2021.
fn main() {
    let a = [1u8, 2, 3];
    let mut total = 0;
    for x in a.into_iter() {
        total += *x;
    }
    println!("{} {}", total, a.len());
}
