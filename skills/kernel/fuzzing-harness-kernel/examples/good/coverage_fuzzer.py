# GOOD: a coverage-guided fuzzer over a toy parser. Inputs are kept only
# when they add new coverage (the KCOV feedback loop); the mutated corpus
# quickly reaches the deep bug that random testing misses. This models the
# syzkaller/libFuzzer selection mechanism.
# Run: python examples/good/coverage_fuzzer.py   (expect exit 0)
import random

# toy target: parse a nested packet; crash (raise) only when a depth-3
# structure ends with byte 0x55 (the deep state random testing rarely hits)
def parse(data):
    depth = 0
    for i, b in enumerate(data):
        if b == 0x10:
            depth += 1
        elif b == 0x20:
            depth -= 1
        if depth >= 3 and i == len(data) - 1 and data[-1] == 0x55:
            raise RuntimeError("CRASH: deep nested malformed packet")
    return depth

def coverage(data):
    """Bounded proxy for coverage: (position, depth) pairs processed."""
    cov = set()
    d = 0
    for i, b in enumerate(data):
        if b == 0x10:
            d += 1
        elif b == 0x20:
            d -= 1
        cov.add((i, d))
    return frozenset(cov)

def main():
    random.seed(7)
    seed = [bytes([0x10, 0x20]),
            bytes([0x10, 0x10, 0x10, 0x20, 0x20, 0x20])]  # reaches depth 3
    corpus = seed
    seen = {coverage(s) for s in corpus}
    found_at = None
    for it in range(50000):
        parent = random.choice(corpus)
        mut = bytearray(parent)
        for _ in range(random.randint(1, 4)):
            if mut and random.random() < 0.5:
                mut[random.randrange(len(mut))] = random.randrange(256)
            else:
                mut.append(random.randrange(256))
        data = bytes(mut)
        cov = coverage(data)
        try:
            parse(data)
        except RuntimeError:
            found_at = it
            break
        if cov not in seen:        # keep only NEW-coverage inputs
            seen.add(cov)
            corpus.append(data)
    assert found_at is not None, "coverage-guided fuzzer failed to find bug"
    print(f"GOOD: coverage-guided fuzzer found the bug at iteration "
          f"{found_at}, corpus size {len(corpus)}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
