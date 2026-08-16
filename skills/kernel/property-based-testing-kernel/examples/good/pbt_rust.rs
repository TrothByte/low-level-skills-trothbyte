// GOOD: property-based test for a kernel-adjacent little-endian
// encoder/decoder in Rust, using only std (no external crates). Shows the
// quickcheck-style loop: deterministic PRNG, boundary-aware generator,
// universal round-trip property, seed replay.
//
// Build: rustc --edition 2021 --test pbt_rust.rs -o pbt_rust
// Run:   pbt_rust

struct XorShift(u64);

impl XorShift {
    fn next(&mut self) -> u64 {
        let mut x = self.0;
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        self.0 = x;
        x
    }
    fn len(&mut self) -> usize {
        // boundary-aware length generator
        match self.next() % 8 {
            0 => 0,
            1 => 1,
            2 => 16,
            3 => 17,
            4 => 255,
            5 => 256,
            _ => (self.next() % 257) as usize,
        }
    }
}

/// Kernel-adjacent code under test: a 64-bit BE/LE pair.
fn encode_u64_le(v: u64) -> [u8; 8] {
    let mut b = [0u8; 8];
    for (i, byte) in b.iter_mut().enumerate() {
        *byte = ((v >> (8 * i)) & 0xff) as u8;
    }
    b
}

fn decode_u64_le(b: &[u8; 8]) -> u64 {
    let mut v = 0u64;
    for (i, byte) in b.iter().enumerate() {
        v |= (*byte as u64) << (8 * i);
    }
    v
}

/// Ring-buffer index math: idx must stay in [0, capacity).
fn wrap_index(idx: usize, capacity: usize) -> usize {
    debug_assert!(capacity > 0);
    idx % capacity
}

#[test]
fn roundtrip_encode_decode_is_identity() {
    let mut rng = XorShift(0x9E3779B97F4A7C15);
    for _ in 0..10_000 {
        let v = rng.next();
        assert_eq!(decode_u64_le(&encode_u64_le(v)), v, "roundtrip failed");
    }
}

#[test]
fn wrap_index_always_in_bounds() {
    let mut rng = XorShift(0xDEADBEEF);
    for _ in 0..10_000 {
        // capacity 0 is a precondition violation — never generated
        let cap = 1 + (rng.next() % 4096) as usize;
        let idx = (rng.next() % 1_000_000) as usize;
        assert!(wrap_index(idx, cap) < cap, "index out of bounds");
    }
}

#[test]
fn boundary_lengths_covered() {
    let mut rng = XorShift(12345);
    let mut seen_boundaries = [false; 6];
    for _ in 0..5000 {
        match rng.len() {
            0 => seen_boundaries[0] = true,
            1 => seen_boundaries[1] = true,
            16 => seen_boundaries[2] = true,
            17 => seen_boundaries[3] = true,
            255 => seen_boundaries[4] = true,
            256 => seen_boundaries[5] = true,
            _ => {}
        }
    }
    for (i, hit) in seen_boundaries.iter().enumerate() {
        assert!(hit, "boundary value #{} never generated", i);
    }
}
