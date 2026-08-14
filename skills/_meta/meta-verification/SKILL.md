---
name: meta-verification
description: Use before concluding that low-level code is correct or that a bug is found. Enforces executable verification (compile+run, sanitizers, asm inspection, debugger) instead of "it compiles" or "tests pass".
---

# Meta: Verification Discipline

## When to use

- Declaring a fix correct, a bug found, or a behavior understood.
- Any task where the consequence of a wrong conclusion is a memory-safety/ABI bug.

## What the agent often gets wrong

- "It compiles, therefore correct" (B2).
- "Tests pass, therefore correct" — tests may not cover UB boundary.
- "Sanitizer passed" for a concurrency bug — ASan does NOT detect races (AD-07); use TSan/Miri.
- Skipping sanitizers/fuzzers entirely on C/C++ where they are mandatory (B5).
- Concluding from `-O0` behavior what happens at `-O2` (AD-03).

## How to reason correctly

1. Choose the strongest available gate for the bug class:
   - UB/overflow → UBSan at `-O2`; memory → ASan; race → TSan/Miri; uninit → MSan.
   - ABI/layout → `offsetof`/`sizeof` program + prologue asm.
   - Optimizer behavior → diff `-O0` vs `-O2` asm.
2. If the gate is unavailable, state the limitation explicitly (do not claim verification).
3. Verification must reproduce: record commands and expected outputs.

## Verification matrix

| Claim | Gate |
|---|---|
| no UB at -O2 | `-fsanitize=undefined -fno-sanitize-recover=undefined` |
| no memory bug | ASan (+ valgrind if no ASan) |
| no data race | TSan / Miri (ASan is NOT sufficient) |
| layout/ABI | `offsetof` program + `-S` prologue |
| optimizer behavior | `gcc -O0 -S` vs `gcc -O2 -S` diff |
| constant-time | ctgrind / dudect / asm check |

## What to verify

- The exact commands were run (not assumed).
- The gate chosen actually detects the bug class being verified.
- A "clean" result is explained (gate ran and found nothing — with flags shown).

## When not to use

- Tasks with no executable artifact (pure documentation work) — verification is still
  needed for factual claims, but via source checks, not compilation.
- When a faster gate already proves the claim (e.g. compiler diagnostic) — but always name it.

## How to verify

- Run the exact commands in the verification matrix; record flags and expected output.
- If a gate is unavailable, write "NOT VERIFIED: <gate> unavailable" instead of skipping silently.

## Where the knowledge comes from

- `registry/evals.yaml`, `skills/c/c-undefined-behavior/evals/README.md`.

## Related skills

- `meta-evidence` — the claim being verified must be KNOWN/INFERRED, not assumed.
- `meta-completion` — "done" requires the verification gates recorded here.

## Evaluation

- Adversarial AD-07: agent must not accept an ASan-clean run as proof against data races.
- Verification quality is scored on: gate choice for the bug class, exact commands, honest
  unavailable-gate reporting.
