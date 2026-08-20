"""ML-KEM-style decapsulation with implicit rejection — GOOD model.

FIPS 203 requires that decapsulation NEVER signals whether a ciphertext is
valid: on failure it returns a pseudorandom shared secret derived from the
ciphertext and a secret rejection value, not an error. Early-exit on an
invalid ciphertext would create a decapsulation oracle (chosen-ciphertext
attack) and a timing side channel.

This model always performs the same operations and returns a 32-byte value
for every input. The valid/invalid decision only selects, with an arithmetic
mask, which derivation is returned. An operation trace proves both calls took
the identical path.

Run:  python examples/good/implicit_rejection.py
Expect:  PASS — invalid ciphertext still returns a derived value on the same path
"""
import hashlib

SS_BYTES = 32
OPS = []


def H(parts: list) -> bytes:
    """SHA3-256 over the concatenated parts (stand-in for the KDF)."""
    h = hashlib.sha3_256()
    for p in parts:
        h.update(p)
    return h.digest()


def check_valid(ct: bytes) -> bool:
    """Stand-in for the re-encryption / ciphertext consistency check."""
    OPS.append("check_valid")
    return ct[-1] == 0


def decrypt(ct: bytes, dk: bytes) -> bytes:
    """Stand-in for the decryption of the ciphertext to message m'."""
    OPS.append("decrypt")
    return bytes(b ^ k for b, k in zip(ct, dk))


def decaps(dk: bytes, rejection_secret: bytes, ct: bytes) -> bytes:
    """Constant-path decapsulation with implicit rejection.

    Always decrypts, always derives both candidate secrets, and combines them
    with arithmetic selection so the execution path and the API never
    distinguish valid from invalid ciphertexts.
    """
    m_prime = decrypt(ct, dk)
    valid = 1 if check_valid(ct) else 0

    # both derivations are always computed ...
    k_valid = H([m_prime])
    OPS.append("derive_valid")
    k_rejected = H([rejection_secret, ct])
    OPS.append("derive_rejected")

    # ... and combined with an arithmetic select (no secret branch).
    mask = -valid  # 0 if valid, all-ones if invalid
    k = bytes((a & mask) | (b & ~mask) for a, b in zip(k_valid, k_rejected))
    OPS.append("select")
    return k


def main() -> int:
    dk = bytes(range(32))
    rejection_secret = bytes(range(32, 64))
    valid_ct = bytes(range(1, 33)) + b"\x00"      # check_valid -> True
    invalid_ct = bytes(range(1, 33)) + b"\x01"    # check_valid -> False

    OPS.clear()
    ss_valid = decaps(dk, rejection_secret, valid_ct)
    ops_valid = list(OPS)

    OPS.clear()
    ss_invalid = decaps(dk, rejection_secret, invalid_ct)
    ops_invalid = list(OPS)

    print(f"valid ct:   returns {len(ss_valid)} bytes, type={type(ss_valid).__name__}")
    print(f"invalid ct: returns {len(ss_invalid)} bytes, type={type(ss_invalid).__name__}")
    print(f"path trace valid:   {ops_valid}")
    print(f"path trace invalid: {ops_invalid}")

    ok = True
    if len(ss_valid) != SS_BYTES or len(ss_invalid) != SS_BYTES:
        ok = False
        print("FAIL: decapsulation must always return 32 bytes")
    if ss_invalid is None:
        ok = False
        print("FAIL: invalid ciphertext returned None (early exit)")
    if ops_valid != ops_invalid:
        ok = False
        print("FAIL: execution paths differ between valid and invalid inputs")
    if ss_valid != H([decrypt(valid_ct, dk)]):
        ok = False
        print("FAIL: valid path did not return the message-derived secret")
    if ss_invalid != H([rejection_secret, invalid_ct]):
        ok = False
        print("FAIL: invalid path did not return the rejection-derived secret")
    if ss_valid == ss_invalid:
        ok = False
        print("FAIL: valid and invalid must not produce the same secret")

    print("PASS — invalid ciphertext still returns a derived value on the "
          "same path" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
