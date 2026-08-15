"""
GOOD: flash-verify gate + HIL-as-merge-gate decision (host model).
Encodes the rule that a firmware change is mergeable only after a verified HIL
run: compile+host tests alone score 0% deploy confidence without hardware
feedback (arxiv-2606-16190). The gate function is host-runnable.

Run: python good/flash_verify_gate.py
"""
import hashlib


def flash_verified(flash_ok, checksum_matches):
    return flash_ok and checksum_matches


def merge_gate(compiled, host_tests_pass, hil_passed):
    # HIL is the ONLY real gate: compile/host tests are necessary, not sufficient
    return compiled and host_tests_pass and hil_passed


def main():
    image = b"\x5A" * 64
    expected = hashlib.sha256(image).hexdigest()

    # verified flash (read-back matches) then tests on the real board
    assert flash_verified(True, True)
    assert merge_gate(True, True, True)

    # compile + host tests green, but no hardware run -> NOT mergeable
    assert not merge_gate(True, True, False)

    print("verified flash passes; compile-only CI does NOT pass the merge gate")
    print("PASS: flash-verify gate + HIL-as-only-merge-gate")


if __name__ == "__main__":
    main()
