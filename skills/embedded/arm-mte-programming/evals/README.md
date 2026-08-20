# Evaluation — arm-mte-programming

Skill: `skills/embedded/arm-mte-programming`. Stability: `researched` — the
Python model of the tag machinery runs and passes on this host; the C sketches
and every instruction (IRG/STG/LDG on real silicon, prctl on an MTE-enabled
kernel, SEGV_MTEAERR/SEGV_MTESERR delivery) are TARGET-ONLY and need an
Armv8.5+/Armv9 machine (device or QEMU with MTE).

## Verified facts (host, recorded 2026-08-20)

Host: Windows, python 3.11.9, x86 MinGW gcc 16.1 (cannot compile AArch64).
Simulator: `examples/good/mte_sim.py` — deterministic (seeded), plain python 3.

`python examples/good/mte_sim.py` (real output):

```
MTE tag-machinery simulator (model, python 3)

  PASS  use-after-free: stale pointer faults (SYNC)
  PASS  heap overflow past the 16-byte granule: fault
  PASS  intra-granule overflow correctly NOT caught (limitation)
  PASS  correct alloc/free/re-alloc: stale faults, fresh ok
  PASS  TBI disabled: tag ignored, no fault (must enable TBI)
  PASS  MTE_ASYNC: faults deferred, reported at a later point

RESULT: 6 passed, 0 failed
The model matches the MTE properties in the technical brief: UAF and cross-granule overflow are caught, intra-granule overflow is not, and ASYNC defers the report.
```

Exit code: 0. The model reproduces the four MTE properties that determine
what the skill claims: (1) free-poisoning makes stale pointers fault (UAF),
(2) overflow past a 16-byte granule faults when the neighbor tag differs,
(3) intra-granule overflow is NOT caught (shared tag — documented limitation),
(4) ASYNC defers the report past the faulting access.

| Fact | Status | Evidence |
|---|---|---|
| UAF (stale pointer after re-tag on free) faults | VERIFIED (model) | mte_sim.py scenario 1 |
| Cross-granule overflow faults on tag mismatch | VERIFIED (model) | mte_sim.py scenario 2 |
| Intra-granule overflow NOT caught | VERIFIED (model) | mte_sim.py scenario 3 |
| Correct alloc/free/re-alloc cycle: only stale pointer faults | VERIFIED (model) | mte_sim.py scenario 4 |
| TBI disabled: tag bits ignored, no fault | VERIFIED (model) | mte_sim.py scenario 5 |
| ASYNC defers fault report (SEGV_MTEAERR semantics) | VERIFIED (model) | mte_sim.py scenario 6 |
| Granules 16 bytes, tags 4 bits, pointer tag bits 56-59 | KNOWN | Arm MTE whitepaper, Armv9 ARM FEAT_MTE |
| prctl constants (PR_SET_TAGGED_ADDR_CTRL 55, PR_MTE_TCF_SYNC/ASYNC, PR_MTE_TAG_MASK) | KNOWN | linux/include/uapi/linux/prctl.h |
| SEGV_MTEAERR=35, SEGV_MTESERR=36; SYNC precise, ASYNC not | KNOWN | asm/sigcontext.h, kernel MTE docs |
| Real IRG/STG/LDG behavior on silicon, prctl return on MTE-enabled kernel | UNVERIFIED | needs Armv9 target |

## Synthetic evals

- **positive**: an aligned, tagged, poisoned allocator/free in C — predict
  which bugs MTE catches (UAF: yes; overflow past granule: yes) and which not
  (intra-granule: no; crafted tag match ~1/16: no).
- **negative 1**: tag a `malloc` pointer with no 16-byte alignment
  (`examples/bad/bad_unaligned_tagging.c`) — recognize the object shares a
  granule/tag with neighbors and intra-granule overflow is invisible.
- **negative 2**: debug an MTE_ASYNC report as precise
  (`examples/bad/bad_async_precision.c`) — the reported address must be
  treated as a hint, not the bug site.
- **negative 3**: free without re-tagging/poisoning
  (`examples/bad/bad_no_poison_on_free.c`) — stale pointer still matches and
  the use-after-free is silent.
- **negative 4**: enabling MTE without checking the prctl return — must be
  flagged as "silently untagged on unsupported hardware".

## False-positive evals (correct code must NOT be flagged)

- `examples/good/prctl_mte.c` — checks support, uses named prctl constants,
  aligns to 16 with posix_memalign, tags with IRG+STG, and separates
  SEGV_MTESERR (precise) from SEGV_MTEAERR (not precise). Must pass clean.
- A SYNC-mode handler reading `si_addr` as the exact fault address is correct
  — do NOT flag it for the ASYNC caveat.
- Tagged pointer dereferenced inside the tagged region is correct with TBI on
  — do NOT flag "pointer has tag bits set" per se.
- Poisoning the granule on free even when the chunk is reused internally is
  correct (that is the UAF guarantee) — do NOT flag it as a waste.
- MTE disabled on a device without FEAT_MTE is correct — do NOT flag the code
  for not tagging.

## Historical evals

- **Android heap-overflow mitigations**: Scudo allocator MTE integration;
  MTE turned on for the Android 13/14 era devices (Pixel 8 / Snapdragon 8
  Gen 2+). Use-after-free and heap-overflow CVEs dropped in classes covered by
  MTE; production uses ASYNC, so crash stacks are approximate and triage
  re-runs with SYNC — the exact reasoning the skill drills.
- **Chromium Android MTE rollout**: MTE enabled on Android release builds;
  reports treated as signals for bug fixing rather than proof of safety,
  documenting the probabilistic nature and the ASYNC reporting caveats.
- Neither is a CVE the agent must reproduce; they are the empirical record
  for why MTE is production-plausible and where its attribution limits show.

## Adversarial evals

- A patch that tags an unaligned allocation and claims "overflow now
  detected" — catch the intra-granule false claim.
- A triage that "fixes" an ASYNC si_addr and returns — require a SYNC re-run
  before attribution.
- A free path that returns the chunk without re-tagging, then a stale-pointer
  test that "passes" because the tag still matches — recognize the missing
  poison.
- An agent that calls prctl with hand-rolled literals instead of the named
  constants — flag the silent mode-selection risk.
- A claim that "MTE makes the allocator memory-safe" — reject: 4-bit tags,
  probabilistic, complement with bounds checks.

## Verification commands (target — ARMv9/Android device or QEMU with MTE)

Host (executed 2026-08-20):

```
python examples/good/mte_sim.py   # 6 passed, 0 failed, exit 0
```

Target (documented-as-target, NOT executed here; host cannot run AArch64):

```
# QEMU user mode with MTE:
qemu-aarch64 -cpu max ./prctl_mte        # expect: "MTE enabled (SYNC)"
# Device / kernel check:
grep -E '^Features.*\bmte\b' /proc/cpuinfo
# Build the good sketch on an ARM toolchain:
aarch64-linux-gnu-gcc -mcpu=armv9-a -march=armv9-a+memtag \
    examples/good/prctl_mte.c -o /tmp/mte
# Deliberate fault experiment: free a chunk, keep the stale pointer, touch it
# under MTE_SYNC -> SIGSEGV with SEGV_MTESERR and si_addr == the address.
```

## Scoring

- recall: all five error classes detected (unaligned tagging, ASYNC
  misattribution, missing poison on free, uncheckable prctl enable, probabilistic
  overclaim) from the C snippets and the simulator scenarios.
- precision: `examples/good` and the false-positive list produce zero findings.
- FP-rate: no false positives on the correct aligned/poisoned/SYNC-handler path.
- platform correctness: host facts labeled VERIFIED (model, recorded output
  above); silicon/prctl/instruction behavior labeled TARGET-ONLY / UNVERIFIED.
