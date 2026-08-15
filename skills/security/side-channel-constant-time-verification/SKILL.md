---
name: side-channel-constant-time-verification
description: Use when writing or reviewing cryptographic or security-critical code that handles secrets: timing-leak audits, constant-time compares, secret-indexed lookups, division-timing. Prevents early-exit memcmp, secret-derived branches and indices, and value-dependent division in C, C++, and Rust. Requires measuring timing, not just reading source.
---

# Constant-Time Code and Timing-Leak Verification

## When to use

- Reviewing or writing any function that processes secrets (keys, MACs, tags,
  tokens, signatures, passwords): comparison, indexing, arithmetic, decoding.
- Auditing a crypto primitive for timing side channels before shipping.
- Judging whether `memcmp`/`strcmp`/`==` on secret data is acceptable.
- Investigating a claim like "compiles and passes, so it is constant-time".
- Any change to code that has ever had a `constant_time`, `ct_`, or "no
  branch/division" annotation — check the annotation is still true in the object
  code.

## When not to use

- Code that only ever touches public data (logging, formatting, config), where
  timing leaks carry no secret. Still prefer constant-time primitives where the
  library provides them for free.
- Pure speed optimization with no security claim.
- Hardware crypto units (AES-NI, ARM `cryptographic extension`, secure
  elements): these are data-independent by design; validate the *usage* (key
  loading, mode) instead. If unsure, still apply the audit here.
- Side channels other than timing (power, EM, fault injection) — different
  discipline.

## What the agent often gets wrong

- Uses `memcmp`/`strcmp`/string `==` on secret data and calls it fine — they
  exit at the first difference; CWE-1254 found ~40% of Copilot-generated crypto
  contained such leaks.
- Writes `if (secret > 0) ...` or early-exit loops, treating correctness as
  sufficient; path length is observable.
- Uses `table[secret_index]` — the touched cache line is a signal.
- Assumes integer division is constant-latency; ML-DSA's UDIV/SDIV timing leak
  (CVE-2026-22705) shows it is not on real hardware.
- Verifies the C source and trusts `-O2` to keep the loop intact — compilers
  turn XOR-folds into short-circuit or vectorized compares.
- Treats "no constant-time claim" lists (e.g. rustcrypto) as authoritative for
  the whole crate instead of per-function.

## How to reason correctly

1. Classify every input/output: what is secret, what is public, who can observe
   timing/cache/branch state.
2. For each secret-dependent operation, list the three channels: branch (path
   length), memory address (cache line), ALU latency (e.g. division).
3. Rewrite to fixed work per input: XOR-fold compares, arithmetic select over
   all candidates, reciprocal-multiply instead of division.
4. Check the compiled object, not the source: `objdump -d` at `-O2`, looking for
   a data-dependent `jz/jnz` after a secret byte, a load addressed by a secret
   index, or an `idiv`/`div` on secret operands.
5. Measure: differential timing (dudect) or taint flow (ctgrind) on the real
   binary. If a toolchain is unavailable, benchmark with a monotonic clock and
   report real numbers (see this skill's evals).

## What to verify

- No branch whose condition is derived from a secret byte (source AND assembly).
- No memory access whose address is derived from a secret.
- All loops over secret data execute a fixed number of iterations.
- Final branch happens only after the secret is folded into an accumulator.
- Generated `-O2` code still shows the XOR-fold loop (compiler did not
  re-introduce an early exit).
- Timing measurement shows the "different position" delta ≈ 0 for the
  constant-time version and >> 0 for the early-exit version.

## How to verify

```
gcc -O2 -Wall -Wextra -c examples/good/ct_compare.c
objdump -d ct_compare.o          # loop intact, branch only on accumulator

gcc -O2 timing_demo.c -o timing_demo
timing_demo 500000 256           # compare early-exit vs constant-time timings

gcc -O2 examples/bad/early_exit_secret.c -c   # compiles fine — audit flags it

# when available (target toolchains, not on this host):
git clone https://github.com/oreparaz/dudect  # differential timing
valgrind --tool=memcheck ...      # ctgrind (BearSSL) marks secret taint
```

Recorded numbers for this host (Windows MSYS2, gcc 16.1.0) are in
`evals/README.md` under Verified facts.

## Where the knowledge comes from

- `cwe-1254` — software-created encryption leaks (early-exit comparisons,
  ~40% Copilot vulnerable).
- `tob-constant-time` — Trail of Bits' constant-time handbook: rules, why
  compilers break them, assembly-level verification.
- `dudect` / `ctgrind` — the two standard detection tools.
- `CVE-2026-22705` — ML-DSA UDIV/SDIV timing leak: division latency is
  value-dependent on real hardware.

## Related skills

- `zeroize-constant-time` — safe wiping of secrets (same discipline, opposite
  direction).
- `rust-crypto-primitives-safety` — constant-time claims in Rust crates,
  including "no constant-time claim" lists.
- `compiler-ub-assumptions` — why the optimizer can change the observable
  behavior of security code.
- `c-undefined-behavior` — signed overflow in constant-time arithmetic.
- `formal-spec-loop-invariants`, `smt-z3-sound-usage` — formal proof of the same
  properties.

## Evaluation

- Synthetic: flag bad/early_exit_secret.c, bad/secret_indexed_table.c,
  bad/secret_division.c; approve good/ct_compare.c, good/ct_select.c,
  good/ct_division.c.
- False-positive: a `memcmp` on clearly public data (version strings) must NOT
  be flagged; `ct_select` over a full 256-entry table must be approved.
- Historical: CVE-2026-22705 (ML-DSA division timing) — explain why the fix
  removes value-dependent `UDIV`/`SDIV`, and CWE-1254.
- Adversarial: a "constant-time" function whose `-O2` output shows an early-exit
  branch — the agent must catch it in assembly, not the source.
- Verified facts and commands actually run: `evals/README.md`.
