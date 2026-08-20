"""Non-constant-time rejection sampling (ML-KEM style) — BAD model.

The loop exits as soon as n_coeffs coefficients are collected, so the
iteration count depends on the sampled values (data-dependent control flow).
A timing attacker who can measure how long sampling takes learns how often
rejection occurred, leaking information about the secret seed.

The branch analyzer at the bottom checks both behavior (iteration counts
across inputs) and source shape (a `while` guard on the collection size),
and flags the data-dependent branch.

Run:  python examples/bad/branchy_rejection.py
Expect:  FLAG — data-dependent control flow (iteration count varies with input)
"""
import hashlib
import inspect

Q = 3329
N_COEFFS = 256
CANDIDATES = 32768  # plenty for 256 accepts at the ~5% acceptance rate
STREAM_BYTES = 2 * CANDIDATES


def shake256_bytes(seed: bytes, length: int) -> bytes:
    return hashlib.shake_256(seed).digest(length)


def branchy_rejection_sample(stream: bytes, n_coeffs: int):
    """Rejection sampling with a data-dependent loop bound (BAD)."""
    coeffs = []
    idx = 0
    iterations = 0
    while len(coeffs) < n_coeffs:          # data-dependent loop bound
        iterations += 1
        b0 = stream[2 * idx]
        b1 = stream[2 * idx + 1]
        val = b0 | (b1 << 8)
        if val < Q:                         # early-acceptance path
            coeffs.append(val)
        idx += 1
        if 2 * idx > len(stream):
            raise ValueError("stream exhausted")
    return coeffs, iterations


def make_unlucky_stream(seed: bytes) -> bytes:
    """Build a stream where half the candidates are rejected."""
    data = bytearray(shake256_bytes(seed, STREAM_BYTES))
    for i in range(0, len(data) - 1, 2):
        v = data[i] | (data[i + 1] << 8)
        if v % 2 == 0 and v < Q:
            v += Q  # force rejection on even values
        data[i] = v & 0xFF
        data[i + 1] = (v >> 8) & 0xFF
    return bytes(data)


def branch_analyzer():
    """Flag the data-dependent branch in branchy_rejection_sample."""
    flags = []

    source = inspect.getsource(branchy_rejection_sample)
    if "while len(coeffs) < n_coeffs" in source:
        flags.append("source: loop bound depends on the accepted-count state "
                     "(`while len(coeffs) < n_coeffs`)")

    lucky = shake256_bytes(b"lucky-seed", STREAM_BYTES)
    unlucky = make_unlucky_stream(b"unlucky-seed")
    _, it_lucky = branchy_rejection_sample(lucky, N_COEFFS)
    _, it_unlucky = branchy_rejection_sample(unlucky, N_COEFFS)
    print(f"lucky:   iterations={it_lucky}")
    print(f"unlucky: iterations={it_unlucky}")
    if it_lucky != it_unlucky:
        flags.append(
            f"behavior: iteration count varies with input "
            f"({it_lucky} vs {it_unlucky}) — timing leak"
        )

    return flags


def main() -> int:
    flags = branch_analyzer()
    if flags:
        for f in flags:
            print(f"FLAG: {f}")
        print("FLAG — data-dependent control flow: iteration count varies "
              "with input")
        return 1
    print("no flags (unexpected for this fixture)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
