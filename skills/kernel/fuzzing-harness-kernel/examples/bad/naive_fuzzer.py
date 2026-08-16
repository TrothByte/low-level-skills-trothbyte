# BAD: random testing without a coverage feedback loop. 1M random inputs
# never assemble the depth-3 + magic-byte structure, so the run reports
# "no crashes" while the target is buggy. This is the "ran for 24h, clean"
# illusion: the bug exists, the harness simply cannot reach it.
# Marker: intentionally incorrect
# Run: python examples/bad/naive_fuzzer.py
import random

def parse(data):
    if len(data) < 3:
        return 0
    depth = 0
    for b in data:
        if b == 0x10:
            depth += 1
        elif b == 0x20:
            depth -= 1
        if depth >= 3:
            if data[-3:] == bytes([0x55, 0xAA, 0xDE]):
                raise RuntimeError("CRASH: deep nested malformed packet")
    return depth

def main():
    random.seed(1)
    crashes = 0
    for _ in range(1000000):
        n = random.randint(0, 8)
        data = bytes(random.randrange(256) for _ in range(n))
        try:
            parse(data)
        except RuntimeError:
            crashes += 1
    # intentionally incorrect: "no crashes" is reported as evidence the
    # target is fine, but the feedback loop (coverage) is absent, so the
    # fuzzer cannot learn to build the depth-3 structure.
    print(f"fuzzed 1000000 inputs, crashes={crashes}")
    print("RESULT: no crashes found -> target appears clean")
    print("BAD: no coverage feedback; the depth-3 bug is reachable but the "
          "random generator never builds it; 'clean' is an artifact")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
