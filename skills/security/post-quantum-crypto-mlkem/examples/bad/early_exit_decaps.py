"""Early-exit decapsulation (ML-KEM style) — BAD model.

Returning an error / None on an invalid ciphertext breaks FIPS 203 implicit
rejection and creates a decapsulation oracle: an attacker who queries
decapsulation and observes whether an error comes back (or measures timing)
learns whether the ciphertext is valid, enabling chosen-ciphertext attacks.

The branch analyzer flags the early-exit path: the return type and the
execution path distinguish valid from invalid ciphertexts at the API.

Run:  python examples/bad/early_exit_decaps.py
Expect:  FLAG — early exit on invalid ciphertext (decapsulation oracle)
"""
import hashlib
import inspect

SS_BYTES = 32


def check_valid(ct: bytes) -> bool:
    return ct[-1] == 0


def decrypt(ct: bytes, dk: bytes) -> bytes:
    return bytes(b ^ k for b, k in zip(ct, dk))


def decaps_early_exit(dk: bytes, ct: bytes):
    """Decapsulation that early-exits on an invalid ciphertext (BAD)."""
    if not check_valid(ct):            # FLAG: early exit on invalid ct
        return None
    return hashlib.sha3_256(decrypt(ct, dk)).digest()


def branch_analyzer():
    """Flag the early-exit decapsulation path."""
    flags = []

    source = inspect.getsource(decaps_early_exit)
    if "return None" in source:
        flags.append("source: early `return None` on the invalid-ciphertext "
                     "path — API distinguishes valid from invalid")

    dk = bytes(range(32))
    valid_ct = bytes(range(1, 33)) + b"\x00"
    invalid_ct = bytes(range(1, 33)) + b"\x01"

    ss_valid = decaps_early_exit(dk, valid_ct)
    ss_invalid = decaps_early_exit(dk, invalid_ct)
    print(f"valid ct:   returns {len(ss_valid) if ss_valid is not None else 'None'} bytes")
    print(f"invalid ct: returns {ss_invalid!r}")

    if ss_valid is not None and ss_invalid is None:
        flags.append("behavior: valid ciphertext yields a secret, invalid "
                     "yields None — an oracle distinguishes them by return "
                     "code (and by the shorter early-exit timing path)")
    return flags


def main() -> int:
    flags = branch_analyzer()
    if flags:
        for f in flags:
            print(f"FLAG: {f}")
        print("FLAG — early exit on invalid ciphertext: decapsulation oracle "
              "(breaks FIPS 203 implicit rejection)")
        return 1
    print("no flags (unexpected for this fixture)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
