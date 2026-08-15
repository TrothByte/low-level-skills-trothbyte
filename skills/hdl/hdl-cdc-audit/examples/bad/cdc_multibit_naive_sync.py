#!/usr/bin/env python3
# intentionally incorrect: naive multi-bit CDC (2-FF per bit).
"""Deterministic model of a naive multi-bit CDC: one 2-FF synchronizer per
data bit. This is the BAD pattern.

Semantics verified: synchronizing each bit of a multi-bit bus with its own
2-FF pair does NOT make the bus coherent. Because each bit's first stage
samples the source at a slightly different destination edge (routing/clock
skew), the destination can capture a MIX of old and new bits -- a value that
never existed on the source bus. This is why '2-FF each bit' is wrong for
multi-bit data and gray-code/handshake/FIFO is required.

Run:  python examples/bad/cdc_multibit_naive_sync.py
"""
import random

SEED = 20260815
rng = random.Random(SEED)


def flop(d, q, rng, meta_window=0.05):
    if rng.random() < meta_window:
        return q if rng.random() < 0.5 else d
    return d


def sync_bit_chain(bus_values, bit_index, skew_cycles, rng):
    """A single bit crossing through a 2-FF synchronizer. skew_cycles models
    how many destination cycles later this bit's first stage sees the
    source transition (routing skew between the bits)."""
    q1, q2 = 0, 0
    out = []
    delayed = [0] * skew_cycles + bus_values
    for d in delayed:
        q1 = flop(d, q1, rng)
        q2 = flop(q1, q2, rng)
        out.append(q2)
    return out


def main():
    # Source bus holds 0b0101 then switches to 0b1010 -- every bit changes.
    bus = [0b0101] * 3 + [0b1010] * 3
    nbits = 4

    # Each bit has a different skew (0, 1, 0, 1 destination cycles), the
    # real-world consequence of unequal routing to each first-stage flop.
    skews = [0, 1, 0, 1]
    bit_chains = [
        sync_bit_chain([(v >> b) & 1 for v in bus], b, skews[b], rng)
        for b in range(nbits)
    ]

    # Reconstruct the destination bus word per cycle.
    ncyc = len(bus) + max(skews)
    dest_words = []
    for cyc in range(ncyc):
        word = 0
        for b in range(nbits):
            sample = bit_chains[b][cyc] if cyc < len(bit_chains[b]) else bit_chains[b][-1]
            word |= sample << b
        dest_words.append(word)

    print(f"source bus:     {[format(v, '04b') for v in bus]}")
    print(f"destination:    {[format(v, '04b') for v in dest_words]}")

    incoherent = [w for w in dest_words if w not in (0b0101, 0b1010)]
    if incoherent:
        print(f"INCOHERENT values captured that never existed on the source: "
              f"{[format(v, '04b') for v in incoherent]}")
        print("VERIFIED: naive per-bit 2-FF sync of a multi-bit bus is unsafe")
        return 0
    print("FAIL: model produced no incoherent word; increase skew")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
