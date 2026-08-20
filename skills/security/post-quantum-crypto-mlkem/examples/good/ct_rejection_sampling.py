"""Constant-time rejection sampling (ML-KEM style) — GOOD model.

ML-KEM converts SHAKE256 output into ring coefficients (values < q = 3329)
via rejection sampling. The constant-time property is that the extraction
loop runs a FIXED number of iterations regardless of how many candidates are
accepted or rejected: no early exit, no data-dependent loop bound.

This fixture proves the property by counting iterations for a "lucky" stream
(almost all candidates accepted) and an "unlucky" stream (all candidates
forced to rejection) and asserting the iteration counts are identical.
Acceptance is applied with an arithmetic select (mask-based write), so every
input takes the same execution path.

Run:  python examples/good/ct_rejection_sampling.py
Expect:  PASS — fixed iteration count for accepted vs rejected samples
"""
import hashlib

Q = 3329          # ML-KEM modulus
N_COEFFS = 256    # n, ring degree (coefficients per polynomial)


def shake256_bytes(seed: bytes, length: int) -> bytes:
    """Stand-in for the SHAKE256 stream (XOF) used by ML-KEM."""
    return hashlib.shake_256(seed).digest(length)


def ct_rejection_sample(stream: bytes, n_coeffs: int, max_candidates: int):
    """Fixed-iteration rejection sampling from a 2-byte-candidate stream.

    Iterates exactly `max_candidates` times. Accepted candidates land in the
    output buffer via an arithmetic select; rejected candidates consume the
    same iteration and leave the slot untouched. The loop bound never depends
    on the sampled values.

    Returns (out, fill, iterations): `out` is the coefficient buffer,
    `fill` is how many coefficients were accepted, `iterations` is the loop
    count.
    """
    out = [0] * n_coeffs
    fill = 0
    iterations = 0
    for idx in range(max_candidates):
        iterations += 1
        b0 = stream[2 * idx]
        b1 = stream[2 * idx + 1]
        val = b0 | (b1 << 8)
        # constant-time select: mask = 0 on accept, all-ones on reject
        accept = 1 if (val < Q and fill < n_coeffs) else 0
        mask = accept - 1
        out[fill % n_coeffs] = (out[fill % n_coeffs] & mask) | (val & ~mask)
        fill += accept
    return out, fill, iterations


def make_lucky_stream() -> bytes:
    """Build a stream whose every candidate is < Q (always accepted)."""
    data = bytearray()
    for i in range(N_COEFFS):
        v = i % Q
        data += bytes([v & 0xFF, (v >> 8) & 0xFF])
    return bytes(data)


def make_unlucky_stream(seed: bytes) -> bytes:
    """Build a stream whose every candidate is >= Q (always rejected)."""
    data = bytearray(shake256_bytes(seed, 2 * N_COEFFS))
    for i in range(0, len(data) - 1, 2):
        v = data[i] | (data[i + 1] << 8)
        if v < Q:
            v += Q  # force rejection
        data[i] = v & 0xFF
        data[i + 1] = (v >> 8) & 0xFF
    return bytes(data)


def main() -> int:
    lucky = make_lucky_stream()
    unlucky = make_unlucky_stream(b"unlucky-seed")

    max_cands = N_COEFFS
    out_lucky, fill_lucky, it_lucky = ct_rejection_sample(lucky, N_COEFFS, max_cands)
    out_unlucky, fill_unlucky, it_unlucky = ct_rejection_sample(unlucky, N_COEFFS, max_cands)

    print(f"lucky:   iterations={it_lucky} accepted={fill_lucky}/{N_COEFFS}")
    print(f"unlucky: iterations={it_unlucky} accepted={fill_unlucky}/{N_COEFFS}")

    ok = True
    if it_lucky != it_unlucky:
        ok = False
        print("FAIL: iteration counts differ (data-dependent control flow)")
    if it_lucky != max_cands or it_unlucky != max_cands:
        ok = False
        print("FAIL: loop bound is not fixed")
    if fill_lucky != N_COEFFS:
        ok = False
        print("FAIL: lucky stream should accept every candidate")
    if fill_unlucky != 0:
        ok = False
        print("FAIL: unlucky stream should reject every candidate")
    if any(c < 0 or c >= Q for c in out_lucky + out_unlucky):
        ok = False
        print("FAIL: output coefficient outside [0, q)")
    for i, val in enumerate(out_lucky):
        expect = lucky[2 * i] | (lucky[2 * i + 1] << 8)
        if val != expect:
            ok = False
            print(f"FAIL: lucky out[{i}] = {val}, expected {expect}")
            break

    print("PASS — fixed iteration count for accepted vs rejected samples"
          if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
