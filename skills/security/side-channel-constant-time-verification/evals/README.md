# Evaluation — side-channel-constant-time-verification

Skill: `skills/security/side-channel-constant-time-verification`.
Stability target: `evaluated`. Toolchain: gcc 16.1.0 (MSYS2 ucrt64, Windows).

## Verified facts (ACTUAL runs, recorded 2026-08-15)

All commands below were actually executed on this host. The timing demo is in
`examples/good/timing_demo.c`.

```
gcc -O2 examples/good/timing_demo.c -o timing_demo   (exit 0)

timing_demo 500000 256
  n=256 iters=500000
  early_exit first-byte-diff: 0.000373 s
  early_exit last-byte-diff : 0.054375 s
  consttime  first-byte-diff: 0.003705 s
  consttime  last-byte-diff : 0.003463 s
  early-exit leak (last-first): 0.054002 s
  consttime leak (last-first) : -0.000242 s

timing_demo 200000 32
  n=32 iters=200000
  early_exit first-byte-diff: 0.000166 s
  early_exit last-byte-diff : 0.002681 s
  consttime  first-byte-diff: 0.000467 s
  consttime  last-byte-diff : 0.000424 s
  early-exit leak (last-first): 0.002515 s
  consttime leak (last-first) : -0.000043 s
```

Interpretation: the early-exit compare leaks the first-difference position —
the delta between "first byte differs" and "last byte differs" is ~0.054 s
(256-byte, 500k iters). The constant-time version shows a delta within noise
(~0.00024 s). These numbers are host-specific but the SIGN of the result
(early-exit leaks, fold does not) is stable across sizes and iterations.

Additional compile checks (all exit 0 — silent, must be caught by review):

```
gcc -O2 -Wall -Wextra -c examples/good/ct_compare.c    exit 0
gcc -O2 -Wall -Wextra -c examples/good/ct_select.c     exit 0
gcc -O2 -Wall -Wextra -c examples/good/ct_division.c   exit 0
gcc -O2 -Wall -Wextra -c examples/bad/early_exit_secret.c  exit 0
gcc -O2 -Wall -Wextra -c examples/bad/secret_indexed_table.c exit 0
gcc -O2 -Wall -Wextra -c examples/bad/secret_division.c exit 0
```

## Synthetic evals

- easy/negative: `bad/early_exit_secret.c` — secret-dependent early-exit
  compare must be flagged.
- easy/negative: `bad/secret_indexed_table.c` — `sbox[data[i] ^ key]` must be
  flagged (cache-line leak).
- medium/negative: `bad/secret_division.c` — value-dependent division must be
  flagged as timing-relevant.
- easy/positive: `good/ct_compare.c` — XOR-fold compare must be approved.
- medium/positive: `good/ct_select.c` — full-table arithmetic select must be
  approved.
- hard/positive: `good/ct_division.c` — reciprocal multiply must be approved
  AND the agent must note it still depends on target hardware division.

## False-positive evals (correct code must not be flagged)

- `memcmp`/`strcmp` on clearly public data (version banners, log prefixes,
  file-type magic compared before any secret is involved).
- `good/ct_select.c`: reading all 256 table entries then selecting is correct;
  do NOT flag "it still reads the table".
- A single final `acc == 0` branch AFTER the fold: that branch depends only on
  equality, not on secret position — do NOT flag.
- Calls into hardware AES / AEAD intrinsics (`_mm_aesenc_si128`) — do NOT flag
  as secret-indexed.

## Historical evals (CVE-2026-22705)

- Class: ML-DSA (post-quantum signature) timing leak via value-dependent
  integer division (`UDIV`/`SDIV`) latency in a signing path.
- Task: explain why removing division from the secret-dependent computation
  (replacing it with multiplication/reciprocal arithmetic) closes the channel,
  and why `div` cannot be assumed constant-latency on the target.
- Verify: reproduce with a dudect-style harness on the target, or with
  `objdump -d` showing no `idiv` on secret operands. Toolchain not available on
  this host — documented as target verification.

## Adversarial evals

- A function annotated "constant-time" whose `gcc -O2` output is a
  `memcmp`-like early-exit (compiler transformed the fold): agent must detect in
  `objdump -d`, not the source.
- A `ct_select` reordering where the compiler hoists a load of `sbox[i]`
  past the secret-dependent select, turning an address-derived read into a
  leak: must be caught in assembly.
- A claim "rustcrypto has no constant-time claim, so this crate is fine":
  must be rejected — per-function analysis required.

## Tooling availability (honest)

- Available: gcc 16.1.0, objdump, python 3.11 — used for the verified facts.
- NOT installed on this host: `dudect`, `ctgrind` (needs valgrind), qemu,
  perf-counters for cycle-level measurement. Documented as target toolchains;
  the timing demo is a portable monotonic-clock stand-in and the real numbers
  are recorded above.
