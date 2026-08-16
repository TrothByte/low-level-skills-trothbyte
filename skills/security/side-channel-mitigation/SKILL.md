---
name: side-channel-mitigation
description: Use when choosing or reviewing countermeasures against side-channel leaks — constant-time vs masking vs blinding, cache/timing/power/EM channels, speculative-execution leaks (Spectre, Meltdown, MDS), and verifying a mitigation actually closes the channel. Teaches threat-model-driven countermeasure selection and evidence-based verification.
---

# Side-Channel Mitigation

## When to use

- Choosing a countermeasure class for a secret-dependent operation (constant-
  time, masking, blinding, shuffling, cache flushing).
- Reviewing crypto or security code for timing, cache, power, or EM leaks.
- Auditing Spectre/Meltdown/MDS mitigation requirements for a new code path.
- Deciding whether a "constant-time" claim is credible at the assembly level.
- Verifying a mitigation closes the channel (dudect-style testing, ctgrind).
- Responding to a side-channel CVE or a customer audit.

## When not to use

- Correctness-only code review with no security property (no secret).
- Runtime performance work unrelated to leakage.
- Purely hardware-side mitigation policy (CPU microcode, kernel flags) — that
  is the platform team's domain; this skill covers the software side.
- If the channel is not understood — first characterize the leak
  (see `side-channel-constant-time-verification`).

## What the agent often gets wrong

- "Constant-time code is immune to all side channels" — it addresses
  data-dependent *timing* on one machine; power/EM need masking, cache
  contention needs isolation/flushing, speculation needs fences or microcode
  (A10).
- "Masking is a drop-in replacement for constant-time" — masking protects
  against leakage of intermediate values only if the mask is independent and
  the implementation is order-invariant; naive masks leak through
  second-order effects (B2).
- "Blinding is enough for RSA" — blinding breaks the statistical link but
  leaves the implementation timing paths; the modular exponentiation must
  still avoid secret-dependent branches.
- "The compiler will preserve my constant-time code" — gcc/clang routinely
  transform it (e.g., removing an XOR-fold or the now-common `if` reintroduce
  a branch); verify at the assembly level (B7).
- Confusing "no secret-dependent branch" with "no secret-dependent memory
  access" — an index into a table leaks through the cache (CWE-1254 class).
- Treating hardware mitigations (IBRS/STIBP) as a substitute for software
  constant-time code — they bound speculation, they don't remove the timing
  channel of a data-dependent branch.
- Claiming a mitigation "is verified" after only reading the source —
  verification is measurement/assembly evidence (A10).

## How to reason correctly

1. Threat-model the channel first: which observation (time, cache, power,
   EM, speculative execution) can the attacker make, and which secret
   dependency (data flow, address flow, control flow) does the code expose?
2. Map the channel to the countermeasure class:
   - data-dependent timing → constant-time operations (no secret branches or
     divisions; XOR-fold compares; arithmetic selects).
   - secret-indexed memory → constant-time table access (read-all-then-select)
     or cache-line masking; in crypto, this is the AES-T-table → bitsliced
     transition.
   - power/EM → masking, blinding, shuffling, or hardware AES; higher-order
     masking for strong attackers.
   - speculative execution → bounds check before the deref + `lfence`/`ISB`
     (or rely on hardware mitigations with documented assumptions); avoid
     secret-dependent address formation in Spectre gadgets.
3. Implement the countermeasure so the secret never influences the observable
   at the right granularity; keep the mask/blinding independent of the secret.
4. Verify at multiple levels:
   a. source review (no secret branch/index/div),
   b. compiler output (`objdump -d` — check the compiler didn't undo it),
   c. runtime measurement (dudect t-test: no class separation),
   d. hardware-dependent checks (ctgrind/valgrind for data-dependency leaks).
5. Re-audit on toolchain/CPU change — mitigations are microarchitecture- and
   compiler-version-specific.

## What to verify

- No secret-dependent branch, index, division, or variable shift at the
  assembly level.
- Table accesses are either read-all-then-select or masked to cache lines.
- Mask/blinding values are independent and freshly generated; no reuse.
- Speculation-sensitive dereferences are behind a bounds check plus a fence
  (or the hardware mitigation is documented as covering the case).
- A measurement shows no class separation (dudect t-test below threshold).
- The compiler did not re-transform the constant-time idiom (objdump check).

## How to verify

Host-executable timing check (C, gcc on this host):

```
gcc -O2 examples/good/ct_compare_demo.c -o /tmp/ct && /tmp/ct
gcc -O2 -S examples/good/ct_compare_demo.c -o -   # inspect: no secret branch
objdump -d /tmp/ct | grep -E "je|jne|sete"        # early-exit would show jumps
```

Documented target tools (not installed on this host):

```
# dudect-style differential test on the target CPU:
dudect -n 10000000
# ctgrind (valgrind-based) for data-dependency leaks:
valgrind --tool=ctgrind ./binary
```

## Where the knowledge comes from

- `dudect` — differential constant-time testing (t-test methodology)
- `ctgrind` — data-dependency leak detection via valgrind
- `tob-constant-time` — LLVM-based constant-time analysis (Trail of Bits)
- `cwe-1254` — incorrect string comparison (early-exit length leak)
- `cwe` — CWE-203 (observable discrepancy), CWE-385 (timing)
- `intel-sdm` — Vol.3A cache and speculative-execution chapters
- `zeroize-constant-time` — memory zeroing that must not be elided (skill)

## Related skills

- `side-channel-constant-time-verification` — detecting data-dependence leaks (require)
- `zeroize-constant-time` — secure erasure discipline (recommend)
- `rust-crypto-primitives-safety` — crypto-implementation leaks in Rust (recommend)
- `compiler-ub-assumptions` — why the compiler can transform your code (recommend)
- `asm-optimizer-artifacts` — reading the generated assembly (recommend)
- `performance-measurement-discipline` — measuring behavior without self-deception (recommend)

## Evaluation

Synthetic: classify a channel (time/cache/power/speculation) and pick the
countermeasure; flag a secret-indexed table access; approve a
read-all-then-select; flag "constant-time = immune to everything".
Adversarial: code that "looks constant-time" but the compiler emits a branch
in `objdump` — must be caught in assembly, not source; a mask that is a
constant (no independence). Historical: CVE-2026-22705 (ML-DSA division
timing leak), CVE-2017-5753/5754 (Spectre/Meltdown — mitigation is not a
software constant-time fix), and the early-exit memcmp length leak class
(CWE-1254). FP: a correct read-all-then-select, a documented hardware-
mitigated Spectre path, and a properly masked implementation must NOT be
flagged.
