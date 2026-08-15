// intentionally incorrect
// Two messages encrypted with the same key, the same nonce, and the counter
// restarted at 1: the keystream is identical, so C1^C2 == P1^P2. An attacker
// who sees both ciphertexts recovers the XOR of the plaintexts.
fn quarter_round(state: &mut [u32; 16], a: usize, b: usize, c: usize, d: usize) {
    state[a] = state[a].wrapping_add(state[b]);
    state[d] ^= state[a];
    state[d] = state[d].rotate_left(16);
    state[c] = state[c].wrapping_add(state[d]);
    state[b] ^= state[c];
    state[b] = state[b].rotate_left(12);
    state[a] = state[a].wrapping_add(state[b]);
    state[d] ^= state[a];
    state[d] = state[d].rotate_left(8);
    state[c] = state[c].wrapping_add(state[d]);
    state[b] ^= state[c];
    state[b] = state[b].rotate_left(7);
}

fn chacha_block(key: &[u8; 32], counter: u32, nonce: &[u8; 12]) -> [u8; 64] {
    let mut st = [0u32; 16];
    st[0] = 0x61707865;
    st[1] = 0x3320646e;
    st[2] = 0x79622d32;
    st[3] = 0x6b206574;
    for i in 0..8 {
        st[4 + i] = u32::from_le_bytes([key[4 * i], key[4 * i + 1], key[4 * i + 2], key[4 * i + 3]]);
    }
    st[12] = counter;
    st[13] = u32::from_le_bytes([nonce[0], nonce[1], nonce[2], nonce[3]]);
    st[14] = u32::from_le_bytes([nonce[4], nonce[5], nonce[6], nonce[7]]);
    st[15] = u32::from_le_bytes([nonce[8], nonce[9], nonce[10], nonce[11]]);
    let mut ws = st;
    for _ in 0..10 {
        quarter_round(&mut ws, 0, 4, 8, 12);
        quarter_round(&mut ws, 1, 5, 9, 13);
        quarter_round(&mut ws, 2, 6, 10, 14);
        quarter_round(&mut ws, 3, 7, 11, 15);
        quarter_round(&mut ws, 0, 5, 10, 15);
        quarter_round(&mut ws, 1, 6, 11, 12);
        quarter_round(&mut ws, 2, 7, 8, 13);
        quarter_round(&mut ws, 3, 4, 9, 14);
    }
    for i in 0..16 {
        ws[i] = ws[i].wrapping_add(st[i]);
    }
    let mut out = [0u8; 64];
    for i in 0..16 {
        out[4 * i..4 * i + 4].copy_from_slice(&ws[i].to_le_bytes());
    }
    out
}

fn encrypt(key: &[u8; 32], nonce: &[u8; 12], msg: &[u8]) -> Vec<u8> {
    let mut out = Vec::with_capacity(msg.len());
    for (i, chunk) in msg.chunks(64).enumerate() {
        let ks = chacha_block(key, i as u32 + 1, nonce);
        for (j, b) in chunk.iter().enumerate() {
            out.push(b ^ ks[j]);
        }
    }
    out
}

fn main() {
    let key = [7u8; 32];
    let nonce = [0u8; 12];
    let p1 = b"secret message, number one (classified).......";
    let p2 = b"public  message, number two (unclassified)....";
    let c1 = encrypt(&key, &nonce, p1);
    let c2 = encrypt(&key, &nonce, p2);
    let xor_pt: Vec<u8> = p1.iter().zip(p2.iter()).map(|(a, b)| a ^ b).collect();
    let xor_ct: Vec<u8> = c1.iter().zip(c2.iter()).map(|(a, b)| a ^ b).collect();
    assert_eq!(xor_pt, xor_ct);
    println!("keystream reuse: C1^C2 == P1^P2 ({} bytes leaked)", xor_ct.len());
}
