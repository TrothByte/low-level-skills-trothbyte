# Evaluation — side-channel-mitigation

Skill: `skills/security/side-channel-mitigation`. Type: improved.
Stability: source-backed (CT-vs-leaky timing demo compiled and run with gcc
16.1 on this host — cycle counts recorded; dudect/ctgrind are documented
target tools, not installed).

## Synthetic evals

| Case | Fixture | Expected | Status |
|------|---------|----------|--------|
| Constant-time XOR-fold comparison | `examples/good/ct_compare_demo.c` | no secret branch, fold only | runs (see facts) |
| Early-exit branch on secret | `examples/bad/secret_branch_and_index.c` | FLAG: leaks position | compiles |
| Secret-indexed table access | `examples/bad/secret_branch_and_index.c` | FLAG: cache-address leak | compiles |
| Channel-to-countermeasure mapping | reasoning | timing→CT, cache→mask/select, power→mask | eval case |

## False-positive evals (correct code that must NOT be flagged)

- `ct_equal`'s single final `acc == 0` branch — depends only on equality, not
  on secret position; must NOT be flagged.
- A `memcmp`-free constant-time comparison that reads all bytes and folds —
  correct.
- A Spectre-mitigated dereference (`if (idx < len) { lfence(); x = t[idx]; }`)
  with the hardware mitigation documented — correct.
- A masked table lookup with a fresh per-operation mask — correct.

## Historical evals

- **CVE-2026-22705 (ML-DSA timing leak)** — value-dependent division latency
  in a signing path; agent must require replacing division with
  multiplication/reciprocal arithmetic (see `side-channel-constant-time`).
- **CVE-2017-5753 / CVE-2017-5754 (Spectre / Meltdown)** — speculative
  execution leaks; agent must explain why software constant-time alone does
  not fix them and what the mitigation (fences / KPTI / microcode) covers.
- **Early-exit memcmp length leak (CWE-1254)** — the classic
  `strcmp`/`memcmp` on secrets; agent must use XOR-fold + single compare.

## Adversarial evals (compiles-but-wrong)

- The bad fixture compiles cleanly and runs — the early-exit and table-index
  leaks are present in source and must be caught by review, and the timing
  difference by measurement.
- A function annotated "constant-time" whose gcc `-O2` output is an
  early-exit (compiler transformed the fold) — must be caught in `objdump -d`.
- A mask that is a compile-time constant (no independence) — must be flagged
  as ineffective.

## Verification commands

Host (executed on this host):

```
gcc -O2 -Wall -Wextra examples/good/ct_compare_demo.c -o /tmp/ct && /tmp/ct
gcc -O2 -S -o - examples/good/ct_compare_demo.c | grep -E "je|jne|sete"   # expect only the fold's final compare
gcc -O2 -Wall -Wextra examples/bad/secret_branch_and_index.c -o /tmp/badct && /tmp/badct
```

Target (documented, not on this host):

```
dudect -n 10000000              # differential t-test on the target CPU
valgrind --tool=ctgrind ./binary
```

## Verified facts (KNOWN / INFERRED / UNVERIFIED)

- KNOWN: `ct_compare_demo.c` compiled and ran on this host (gcc 16.1.0,
  x86-64, `-O2`). Actual run, 2026-08-17, `n=256 iters=2000000`:

  ```
  early_exit (early diff)   4.2938 ms
  early_exit (late diff)  157.8348 ms     ← leak: 153.5 ms position signal
  ct_equal  (early diff)   12.8910 ms
  ct_equal  (late diff)    12.6652 ms     ← flat: 0.2 ms within noise
  ```

  Interpretation: the early-exit compare's runtime scales with the position
  of the first differing byte (leak); the XOR-fold's runtime is flat. The
  sign of the contrast is stable; absolute numbers are host-specific.
- KNOWN: the bad fixture compiles cleanly with `-Wall -Wextra` — silent in
  the compiler, must be caught by review/measurement.
- INFERRED: `dudect` and `ctgrind` would flag the bad fixture's data
  dependence (researched from their documentation; tools not installed).
- UNVERIFIED: real-timing behavior on the target CPU (this host's timing
  numbers are indicative only).

## Scoring

- Precision: high — the CT-vs-leaky contrast is demonstrated on this host.
- Recall: high for the documented channels; target-hardware measurement is
  UNVERIFIED.
- FP-rate: low — the single fold-equality branch and documented hardware
  mitigations are distinguishable from real leaks.

## Tooling availability (honest)

- Available on this host: gcc 16.1.0 (timing demo executed).
- NOT installed: dudect, ctgrind/valgrind, Spectre test harness. Documented
  as target commands, not executed here.
