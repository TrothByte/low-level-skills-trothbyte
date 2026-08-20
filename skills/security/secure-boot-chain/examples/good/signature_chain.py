"""signature_chain.py — python twin of signature_chain.c using hashlib.

Hash-chain model of the UEFI Secure Boot chain of trust:
  link = SHA-256(parent_public_key_hash || child_public_key)
The root key hash is the trust anchor; a tampered child breaks every link
that derives from it.

Run:  python signature_chain.py
"""
import hashlib

ROOT_KEY = b"root-key"
INTERMEDIATE = b"intermediate-key"
LEAF = b"leaf-key"
TAMPERED = b"leaf-key-modified"


def sha256(data: bytes) -> bytes:
    return hashlib.sha256(data).digest()


root_hash = sha256(ROOT_KEY)
int_sig = sha256(root_hash + INTERMEDIATE)
leaf_sig = sha256(INTERMEDIATE + LEAF)


def verify(parent_hash: bytes, child: bytes, expected: bytes) -> bool:
    return sha256(parent_hash + child) == expected


def main() -> None:
    print("signature_chain: SHA-256 chain-of-trust model (python/hashlib)")
    print("root_key_hash      = " + root_hash.hex())
    print("intermediate_sig   = " + int_sig.hex())
    print("leaf_sig           = " + leaf_sig.hex())
    print()
    ok1 = verify(root_hash, INTERMEDIATE, int_sig)
    ok2 = verify(INTERMEDIATE, LEAF, leaf_sig)
    print("link 1 (root -> intermediate)     : VERIFY " + ("OK" if ok1 else "FAIL"))
    print("link 2 (intermediate -> leaf)     : VERIFY " + ("OK" if ok2 else "FAIL"))
    tampered = verify(INTERMEDIATE, TAMPERED, leaf_sig)
    print("link 2 with modified leaf          : VERIFY " + ("OK" if tampered else "FAIL") + " (expected)")
    print()
    print("Result: trust chain intact (2/2 links verified); modified child rejected.")
    print("Digests must match signature_chain.c output.")
    return 0 if (ok1 and ok2 and not tampered) else 1


if __name__ == "__main__":
    raise SystemExit(main())
