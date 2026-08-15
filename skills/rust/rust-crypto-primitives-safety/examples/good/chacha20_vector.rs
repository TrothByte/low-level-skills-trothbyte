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

fn block(key: &[u8; 32], counter: u32, nonce: &[u8; 12]) -> [u8; 64] {
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

fn to_hex(bytes: &[u8]) -> String {
    bytes.iter().map(|b| format!("{:02x}", b)).collect()
}

fn main() {
    let key: [u8; 32] = (0u8..32).collect::<Vec<u8>>().try_into().unwrap();
    let nonce: [u8; 12] = [0, 0, 0, 9, 0, 0, 0, 0x4a, 0, 0, 0, 0];
    let keystream = block(&key, 1, &nonce);
    let expected = "10f1e7e4d13b5915500fdd1fa32071c4c7d1f4c733c068030422aa9ac3d46c4ed2826446079faa0914c2d705d98b02a2b5129cd1de164eb9cbd083e8a2503c4e";
    let got = to_hex(&keystream);
    assert_eq!(got, expected, "RFC 8439 section 2.3.2 keystream mismatch");
    println!("keystream[0..8] = {}", &got[0..16]);
    println!("RFC 8439 section 2.3.2 test vector: PASS");
}
