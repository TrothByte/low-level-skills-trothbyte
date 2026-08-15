# Evaluation — riscv-cheri-capability-safety

Skill: `skills/riscv/riscv-cheri-capability-safety`.
Stability: `researched` (source-backed grounding: cheri-spec, cheribsd-docs,
cheribuild). No QEMU-CHERI / cheribuild on this machine; the `.c` examples are
documentary with target commands recorded. The capability enforcement logic
(bounds/permission/tag) was verified with a self-contained Python 3.11 model
(`examples/good/sim_cheri_model.py`), actually run; output recorded below. Mark:
SIMULATED — models the capability model (cheri-spec §2/§6), not the ISA or
hardware tags.

## Toolchain status

`cheribuild` (run-sdk, run-fett, qemu-cheri), QEMU-CHERI, purecap-cc: NOT
available. Consequences:

- The `.c` files use the documented CHERI intrinsics
  (`cheri_bounds_set`, `cheri_offset_set`, `cheri_address_get`, `cheri_seal`,
  `cheri_unseal`, `cheri_perms_get`) that exist in the CHERI clang SDK; they
  compile only there. NOT run here.
- The Python model reproduces the enforcement *decisions* (in-bounds?
  permission present? tag set?) so the review rules have a deterministic oracle.
  It does not model hardware tags, microarchitectural behavior, or the fault
  delivery mechanism.

Target commands to promote to `verified` (CHERI SDK + QEMU-CHERI):

```
# purecap compile of the good and bad examples:
cheribuild run-sdk --cheribsd -- purecap-cc -O2 examples/good/good_bounded.c -o good_b
cheribuild run-sdk --cheribsd -- purecap-cc -O2 examples/bad/bad_oob_derived.c -o bad_oob
# run under QEMU-CHERI:
cheribuild qemu-cheri --run ./good_b     # expect: clean, exit 0
cheribuild qemu-cheri --run ./bad_oob    # expect: SIGPROT (tag/bounds fault)
# or the full boot target:
cheribuild run-fett
```

## Synthetic evals

| Case | Fixture | Expected | Status |
|---|---|---|---|
| easy/negative | `bad/bad_oob_derived.c` | arithmetic past bounds clears tag; deref faults | model-checked |
| medium/negative | `bad/bad_byte_copy.c` | memcpy of capability storage drops tag | model-checked |
| medium/negative | `bad/bad_int_roundtrip.c` | (uintptr_t) round-trip drops tag | model-checked |
| medium/negative | `bad/bad_missing_perm.c` | capability store needs StoreCap | model-checked |
| positive | `good/good_bounded.c` | bounds_set + offset_set in-bounds is correct | model-checked |
| positive | `good/good_seal_copy.c` | element-wise copy keeps tag; seal/unseal paired | model-checked |

## False-positive evals (correct code must NOT be flagged)

- `good/good_bounded.c` — a bounded sub-pointer within the object's bounds is the
  intended pattern; must NOT be flagged as "unsafe narrowing".
- `good/good_seal_copy.c` — element-wise capability copy and correct paired
  seal/unseal: correct.
- `cheri_address_get`-based hashing/printing: correct; must NOT be "fixed" to an
  integer cast.
- An unseal that uses the same key it was sealed with: correct.

## Historical evals

Not applicable as dedicated category: CHERI prevents a *class* of memory-safety
bugs (the classic CVE corpus: buffer overflows, use-after-free, type confusion)
rather than a single CVE. The historical angle is "CHERI turns these CVEs into
faults" — flagged cases from the synthetic set are the standing examples.
Out of scope until a QEMU-CHERI runner exists.

## Adversarial evals

- An object-relative pointer that stays in-bounds for the tested path but escapes
  on the last element (`values[8]` on an 8-element array) — must be caught.
- A pointer that never dereferences out-of-bounds in the tested path but loses
  the tag via integer round-trip — must be caught by tag-tracking review.
- Correct code must not be flagged (FP rule): the bounded sub-pointer pattern is
  the adversarial trap for reviewers who flag all "narrowed" pointers.

## Verified facts (python 3.11.9 run, recorded 2026-08-15)

Command: `python examples/good/sim_cheri_model.py`

```
CHERI capability model — bounds/permission/tag enforcement

  bad_oob_derived (arithmetic past bounds): PASS (faults as expected: tag fault: capability tag cleared)
  bad_int_roundtrip (tag dropped): PASS (faults as expected: tag fault: capability tag cleared)
  bad_byte_copy (memcpy drops tag): PASS (faults as expected: tag fault: capability tag cleared)
  bad_missing_perm (missing store_cap): PASS (faults as expected: permission fault: missing ['store_cap'])
  good_bounded (in-bounds sub-pointer): PASS (no fault)
  good_seal_copy (element-wise copy keeps tag): PASS (no fault)

All model checks: PASS
Model of CHERI capability semantics (cheri-spec §2/§6) — not ISA/
hardware. Documented target: cheribuild run-sdk --cheribsd -- purecap-cc; QEMU-CHERI.
```

Interpretation: the four bad patterns each fault for a *different* reason
(oob arithmetic drops the tag on derivation; integer round-trip and byte copy
drop the tag; missing permission is a permission fault) — confirming the
four-property failure model in `references/cheri-capability-model.md` rule 1 —
and both good patterns pass. Note the model reports the oob-derivation fault as
a tag fault (tag cleared when arithmetic escapes bounds), which matches the
"escaping bounds clears the tag" rule.

## Scoring (for routing eval)

- recall: all four bad classes detected via the four-property reasoning.
- precision: bounded sub-pointers, element-wise copies, and sealed round-trips
  produce zero flags.
- FP-rate: target 0 on the good set; the main FP risk is flagging the legitimate
  bounded sub-pointer pattern (rule 6 exists precisely for this).

## Target toolchains (absent, documented)

- `cheribuild run-sdk --cheribsd -- purecap-cc`: CHERI clang SDK.
- `cheribuild qemu-cheri --run ...` / `cheribuild run-fett`: QEMU-CHERI runner.
- Python 3.11 capability-model sim: AVAILABLE, run, recorded above.
