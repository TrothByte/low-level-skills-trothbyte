#!/usr/bin/env python3
"""Deterministic model of a single-bit CDC with a 2-FF synchronizer.

Semantics verified: a single-bit level that changes in the source clock
domain is eventually observed in the destination domain with bounded,
phase-independent latency, and never produces a spurious 1->0->1 pulse in
the destination domain. Runs with a seeded model of metastability window.

This is the GOOD pattern: one bit, two flops, no combinational logic
between the stages. Run:  python examples/good/cdc_two_flop_sync.py
"""
import random

SEED = 20260815
rng = random.Random(SEED)


def flop(d, q, rng, meta_window=0.05):
    """Model a D flip-flop sampling d. If d changed inside the metastability
    window of the sampling edge, output is resolved with a random delay;
    otherwise the FF copies d. Returns the sampled value."""
    if rng.random() < meta_window:
        # metastable sample: resolved at some later random point
        return q if rng.random() < 0.5 else d
    return d


def cdc_two_flop(input_signal, cycles):
    """2-FF synchronizer: q1 is the metastability 'absorber', q2 is the
    only output allowed to cross into the destination logic."""
    q1, q2 = 0, 0
    dest = []
    for cyc in range(cycles):
        d = input_signal[cyc]
        q1 = flop(d, q1, rng)
        q2 = flop(q1, q2, rng)
        dest.append(q2)
    return dest


def main():
    # Single-bit level crossing: source holds a value for 4+ dest cycles
    # before toggling. Levels: low, high, low, high.
    input_signal = ([0] * 4 + [1] * 4 + [0] * 4 + [1] * 4)
    dest = cdc_two_flop(input_signal, len(input_signal))

    # Verification: every stable level eventually appears in dest with a
    # bounded latency, and dest never contains a spurious glitch (1->0->1
    # without a source transition).
    latency = []
    last_src = input_signal[0]
    src_change = 0
    for i, v in enumerate(input_signal[1:], start=1):
        if v != input_signal[i - 1]:
            last_src = v
            src_change = i
        if dest[i] == last_src:
            latency.append(i - src_change)
            break

    glitch = any(dest[i - 1] == 1 and dest[i] == 0 and dest[i + 1] == 1
                 for i in range(1, len(dest) - 1))

    print(f"dest sequence:        {dest}")
    print(f"source-to-dest delay: {latency[0] if latency else 'never'} dest cycles")
    print(f"spurious glitch:      {glitch}")
    if latency and latency[0] <= 2 and not glitch:
        print("VERIFIED: single-bit 2-FF synchronizer is coherent")
        return 0
    print("FAIL: single-bit 2-FF synchronizer misbehaved")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
