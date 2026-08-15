fn main() {
    let a = [1u8, 2, 3];
    let mut total = 0;
    for x in a.iter() {
        total += x;
    }
    println!("{} {}", total, a.len());
}
