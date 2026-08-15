// intentionally incorrect
// A hand-rolled "cipher": stateless XOR with a key-derived rotating byte.
// No nonce, no block counter, no authentication, 256 possible keys, and
// identical plaintexts always produce identical ciphertexts (detectable).
// This is not crypto; it must be flagged and replaced with an AEAD crate.
fn invented_encrypt(key: u8, msg: &[u8]) -> Vec<u8> {
    msg.iter().enumerate().map(|(i, b)| b ^ key.rotate_left((i % 8) as u32)).collect()
}

fn main() {
    let key = 0x5A;
    let m1 = b"hello world hello world hello world";
    let m2 = b"hello world hello world hello world";
    let c1 = invented_encrypt(key, m1);
    let c2 = invented_encrypt(key, m2);
    assert_eq!(c1, c2);
    println!("no nonce, deterministic keystream: identical plaintexts give identical ciphertexts");
}
