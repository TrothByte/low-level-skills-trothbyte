# Evaluation — checked-c-migration

Skill: `skills/c/checked-c-migration`. Stability: `researched` — the Checked C compiler
itself is target-only (not installed on the host), so host verification is the python
inference model + gcc-runnable plain-C examples; the annotated Checked C file is
`TARGET-ONLY` (compiles only with the Checked C clang fork).

## Verified facts (host, recorded 2026-08-20)

Environment: Windows, Python 3.11.9, GCC 16.1 (MSYS2/MinGW-w64).

- `python examples/tools/annotation_infer.py` → `MODEL SOUND`; all 7 scenarios matched
  expected verdicts. Sample rows: `count_too_small` → FAIL `access index 7 >= declared
  count 4: out of bounds`; `memcpy_overflow` → FAIL `copy length 8 > declared count 4:
  overflow`; `nt_string_unproven` → FAIL `NUL termination not proven: nt bounds are
  unsound`; `unchecked_boundary` → PASS `no static proof: requires _Dynamic_check at
  each use`.
- `gcc -Wall -Wextra -Werror -O2 examples/good/incremental_step.c` compiles clean and runs:
  output `incremental_step: all bounds respected`, exit 0.
- `gcc -Wall -Wextra -O2 examples/bad/bounds_lying.c` compiles with NO warning under gcc
  (the lying count is invisible to stock gcc), but the program overruns at runtime:
  `canary=0x600000005 (expected 0xdeadbeef)` then `OVERFLOW DETECTED: canary corrupted`,
  exit 1. The 32-byte `memcpy` into `int data[4]` + canary overwrote the canary with
  source ints 5 and 6. A Checked C `count(claimed)` bound would have rejected/trapped
  this statically or at runtime.
- Stock gcc rejects the Checked C keywords in `examples/good/annotated_checkedc.c`
  (expected — it is TARGET-ONLY).

## Synthetic evals

- **easy/positive**: `single_object` (deref only) infers `_Ptr<T>` — must PASS.
- **easy/positive**: `count_correct` (loop `[0,4)` on a 4-element buffer) infers
  `_Array_ptr<T> : count(4)` — must PASS.
- **easy/negative**: `count_too_small` (loop `[0,8)` on 4 elements) — must FAIL the
  proposed `count(4)`.
- **medium/negative**: `memcpy_overflow` (copy length 8 into a 4-element buffer) — must
  FAIL.
- **medium/positive**: `nt_string_ok` (provably terminated) — `_Nt_array_ptr<T>` — PASS.
- **medium/negative**: `nt_string_unproven` (termination not proven) — `_Nt_array_ptr`
  must FAIL (see references/checked-c.md rule 3).
- **hard/negative**: `unchecked_boundary` (no proven allocation, subscript 3) — must fall
  back to `bounds(none) + _Dynamic_check`, never a blind cast.

Each eval: DETECT (state the annotation) → EXPLAIN (name the rule) → VALIDATE (run the
model) → VERIFY (compile/run on target when available).

## False-positive evals (correct code must not be flagged)

- FP-01: `_Ptr<T>` on a genuinely single, non-arithmetic pointer must NOT be escalated to
  `_Array_ptr` or `_Nt_array_ptr` (`single_object` stays PASS).
- FP-02: `_Array_ptr : count(4)` with all accesses in `[0,4)` must NOT be rejected
  (`count_correct` stays PASS).
- FP-03: a provably terminated string (`nt_string_ok`) must NOT be demoted to
  `bounds(none)`.
- FP-04: `examples/good/incremental_step.c` (capacity-disciplined plain C) must compile
  clean at `-Wall -Wextra -Werror` — do not flag it as "unmigrated".
- FP-05: `bounds(none)` + `_Dynamic_check` fallback on truly unprovable bounds must be
  accepted, not cast-silenced.

## Historical evals

Vulnerabilities whose root cause a Checked C bounds annotation would have caught (spatial
safety, not control/data flow):

| CVE | Bug | Annotation that catches it |
|---|---|---|
| CVE-2014-0160 (Heartbleed) | `memcpy(bp, pl, payload)` with `payload` exceeding the response buffer | `_Array_ptr<unsigned char> pl : byte_count(payload)` with a preceding `_Dynamic_check(payload <= remaining)` |
| CVE-2021-23017 (nginx resolver) | off-by-one write of the terminating NUL past the allocated buffer | `_Nt_array_ptr` / `count(size+1)` on the copy target — the +1 hole is exposed by the checker |
| CVE-2023-38545 (curl SOCKS5) | oversized hostname copied into a fixed buffer with mismatched bounds | `_Array_ptr<char> host : count(len+1)` with the length validated before the copy |
| CVE-2015-7547 (glibc getaddrinfo) | stack buffer overflow when a crafted DNS response exceeds the `answer` buffer | `_Array_ptr<char> answer : count(answer_len)` enforced on every `memcpy` into it |

3C-scale eval (per arXiv 2203.13445): the 3C tool was evaluated on 11 programs totalling
~319 KLOC, converting the large majority of pointers and bounds automatically and leaving
a small remainder reported as root causes (code that must be refactored for bounds to be
provable). A migration agent must reproduce that shape: high automated coverage plus a
curated, documented root-cause list — not a claim of 100% conversion.

## Adversarial evals

- **count vs byte_count**: an `int` buffer declared `byte_count(16)` and then indexed
  `p[3]` — the model must catch element/byte unit confusion (count = 16/4 = 4).
- **off-by-one at `index == count`**: loop `[0, 4]` on `count(4)` accesses index 4 — must
  FAIL (bounds are half-open).
- **`p++` arithmetic in loops**: pointer increment beyond the declared count must be
  rejected unless rewritten to an index loop (3C root-cause class).
- **nullable data**: `_Ptr` on a pointer that may be NULL must be rejected (checked
  pointers are non-null); needs an explicit null test then annotation.
- **multi-level pointers**: `char **` / `_Ptr<_Array_ptr<char>>` requires bounds on the
  inner array and lifetime reasoning for the outer; a naive flat `_Nt_array_ptr` on the
  outer must be flagged.
- **unchecked interop hole**: an unchecked caller passing a raw pointer into a checked
  function with an unproven `count` — the boundary conversion must be flagged for review,
  not silently accepted (references/checked-c.md rule 5).
- **runtime-dependent bound**: a length parsed from input with no `_Dynamic_check` — must
  FAIL (rule 4).

## Verification commands (target — Checked C clang)

```
# good annotated pattern (TARGET-ONLY): clean compile + happy-path run
clang -fcheckedc-extension -Wall -Wextra -Werror -o annotated_checkedc examples/good/annotated_checkedc.c
./annotated_checkedc

# runtime bounds-check probe: the OOB write must TRAP, not corrupt silently
clang -fcheckedc-extension -Wall -Wextra -Werror -DOOB_PROBE -o oob_probe examples/good/annotated_checkedc.c
./oob_probe
# expect: process abort/trap on pc[3] with count(2)

# inference loop (host, verified)
python examples/tools/annotation_infer.py

# behavioral-equivalence control (host, verified)
gcc -Wall -Wextra -Werror -O2 -o incremental_step examples/good/incremental_step.c && ./incremental_step

# the lying-count bug: gcc is silent, runtime corrupts (host, verified)
gcc -Wall -Wextra -O2 -o bounds_lying examples/bad/bounds_lying.c && ./bounds_lying

# 3C inference tool (target where available)
python3 3c/src/3c.py file.c
```

## Scoring

- precision: every proposed annotation must survive `validate()` and, on target,
  `clang -fcheckedc-extension`; zero cast-to-silence fixes.
- recall: every bad scenario (lying count, overflow copy, unproven termination, missing
  `_Dynamic_check`, unit confusion, off-by-one) must be rejected.
- FP-rate: FP-01..FP-05 stay PASS; good plain-C control compiles clean at
  `-Wall -Wextra -Werror`.
- correctness of reasoning: root-cause refactors reported, not worked around with casts;
  unchecked interop boundaries documented.
