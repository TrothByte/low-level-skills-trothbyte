# BAD: unsupported_cve_claim.py
# intentionally incorrect
"""
The agent claims "CVE-2014-0160 (Heartbleed) verified fixed" with NO build
pair: neither the vulnerable nor the fixed variant was ever compiled or run.
Only the fixed version is mentioned, and only by description. This is B2
("it compiles therefore correct") applied to a security claim — a sentence,
not a regression proof.
"""
# intentionally incorrect


def fixed_openssl_read(record):
    # The claimed fix: bounds-check the heartbeat payload length.
    return record[: min(len(record), 100)]


def main():
    # No vulnerable build. No sanitizer run. No recorded exit code.
    # The "verification" is a one-line function and a confident summary.
    result = fixed_openssl_read(b"x" * 200)
    print(f"fixed read returned {len(result)} bytes")  # looks plausible
    print("CVE-2014-0160: FIX VERIFIED")  # fabricated claim — pair not run


if __name__ == "__main__":
    main()
