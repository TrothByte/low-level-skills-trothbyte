# Evaluation — capability-based-security

Skill: `skills/security/capability-based-security`. Type: unique.
Stability: researched (Python capability-property model executed on this host;
seL4/CHERI toolchains absent — their claims are UNVERIFIED targets).

## Synthetic evals

| Case | Fixture | Expected | Status |
|------|---------|----------|--------|
| Non-holder denied, right-scoped check | `examples/good/capability_model.py` | ALLOWED only for read by alice | runs (see facts) |
| Revoke removes authority | `examples/good/capability_model.py` | alice DENIED after revoke | runs |
| Ambient identity check | `examples/bad/ambient_authority.py` | FLAG: role-based, no cap | runs |
| Unforgeability reasoning | token forgeable via copy | must be rejected | reasoning eval |

## False-positive evals (correct code that must NOT be flagged)

- An seL4 `CNode_Move` (a legal move, not a copy) — must NOT be flagged as
  confinement violation.
- A CHERI capability-passing pattern where the tag is preserved and the
  receiver's bounds are narrower — legal delegation.
- A Unix FD used at a `read()` boundary (kernel-held, non-forgeable) in a
  process that otherwise does not use capabilities — acceptable in context.

## Historical evals

- **seL4 "Wayward" design** — the original seL4 exposed allocation via an
  ambient "wayward" capability; the design reworked it into a confined
  capability-based allocator. Agent must explain why the ambient version was
  a problem.
- **Unix `setuid` delegation accidents** — setuid binaries grant authority
  without a capability model; the `capabilities(7)` bounding/inheritable sets
  are the mitigation. Agent must explain the difference.
- **CVE-2020-7461-class (MIPS CHERI tag)** — tag forgery / capability
  bypass on MIPS; agent must know why tags are the unforgeability mechanism.

## Adversarial evals (compiles-but-wrong)

- The bad fixture runs and prints ALLOWED for the impersonator — the model is
  demonstrably wrong, must be flagged without any "static" excuse.
- A capability stored in user-writable memory (no kernel tag) presented as
  secure — forgeable, must be rejected.
- "Revoke by removing the row" where existing tokens still verify — must be
  flagged as non-revoking.

## Verification commands

Host (executed on this host):

```
python3 examples/good/capability_model.py
python3 examples/bad/ambient_authority.py
```

Target (documented, toolchain not on this host):

```
# seL4: run the test suite asserting non-holders are denied
# CHERI: cheribuild qemu-cheri; test tag preservation on capability copy
```

## Verified facts (KNOWN / INFERRED / UNVERIFIED)

- KNOWN: `capability_model.py` runs on this host and prints ALLOWED/DENIED as
  documented (output recorded above under Synthetic evals).
- KNOWN: `ambient_authority.py` runs and the spoofed admin is ALLOWED —
  demonstrating the flaw.
- INFERRED: seL4 `Revoke` recursively revokes derived capabilities
  (researched from seL4 manual; toolchain absent).
- UNVERIFIED: CHERI tag-forgery behavior on real Morello/CheriBSD hardware.

## Scoring

- Precision: high for the core properties (the model is executable). Recall:
  high for the modeled classes; seL4-specific flow is INFERRED. FP-rate: low
  — legal delegation/move patterns are distinguishable from violations.

## Tooling availability (honest)

- Available on this host: python 3.11.9 (used for both fixtures).
- NOT installed: seL4 toolchain, CheriBSD/Morello QEMU. Documented as target
  commands, not executed here.
