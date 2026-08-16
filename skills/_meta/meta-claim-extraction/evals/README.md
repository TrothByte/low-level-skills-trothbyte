# Evaluation — meta-claim-extraction

Skill: `skills/_meta/meta-claim-extraction`. Stability: `researched`
(knowledge model is source-backed from `registry/claims.yaml`; the extractor
demo is executed on this host 2026-08-17, Python 3.11.9). The skill teaches
claim → source → section → skill traceability.

## Synthetic evals

- easy/positive: `good/claim_extract_demo.py` parses sample claim lines,
  validates source ids, classifies evidence — recorded on host.
- easy/negative: `bad/fabricated_claim.py` invents a section number and labels
  it KNOWN — must be rejected.
- medium/positive: the existing claims CL-001..CL-056 in `registry/claims.yaml`
  each map to a real source id (validated by `source_check.py`).
- medium/negative: a claim with an unregistered source id — `source_check.py`
  exits 2 (reference rule 4).

Detection rule: a claim is traceable iff text + source + section + skill are
all present AND source is registered AND section is real.

## False-positive evals

- `good/claim_extract_demo.py` correctly classifies and must not be flagged.
- A claim with evidence INFERRED (derived from a talk, not a standard) is
  legitimate — do not flag it for being "not KNOWN"; it is honestly labeled.
- Non-normative prose (no correctness impact) must not be forced into claims.

## Historical evals

- The 56 claims in `registry/claims.yaml` were added across PHASE 6 and the
  PHASE 13 batches; each one re-validated by `source_check.py` (0 WARN as of
  the last gate run). This is the historical regression set for the skill.
- Reference rule 1's example CL-003 and rule 4's CL-015 are real registered
  claims; the agent must be able to reproduce their provenance from the
  registry.

## Adversarial evals

- `bad/fabricated_claim.py`: claims a compiler guarantee with an invented
  section and KNOWN evidence — the agent must demand the actual section and a
  compiler run.
- A claim that is actually a duplicate (same text+source as CL-XXX) must be
  collapsed, not re-registered.
- Evidence-inflation: "it compiles, so the ABI claim is KNOWN" — KNOWN
  requires the source states it; compilation alone gives INFERRED at best.

## Verification commands

```
python tools/lint/claim_extractor.py
python tools/source/source_check.py
python tools/validate.py
python tools/lint/skill_lint.py skills/_meta/meta-claim-extraction/SKILL.md
python skills/_meta/meta-claim-extraction/examples/good/claim_extract_demo.py
python skills/_meta/meta-claim-extraction/examples/bad/fabricated_claim.py
```

Recorded 2026-08-17 (Python 3.11.9, Windows):

```
> python examples/good/claim_extract_demo.py
  OK    iso-c11-n1570 :: 6.5 p5 :: c-undefined-behavior
  OK    sysv-amd64-abi :: 3.2.1 :: abi-layout-reasoning
  OK    rust-reference :: drop-scopes :: cpp-object-lifecycle
  all claims traceable   exit 0
> python examples/bad/fabricated_claim.py
  source=gcc-manual section=12.7.3 evidence=KNOWN   exit 0
  <- invented section + KNOWN without verification; must be rejected
> python tools/source/source_check.py
  source_check: 177 sources, 56 claims checked   exit 0 (no WARN)
> python tools/validate.py
  ALL CHECKS PASSED
```

## Verified facts

- KNOWN: `registry/claims.yaml` defines confidence (MUST_VERIFY /
  MUST_CONSIDER / OPTIONAL) and evidence levels (KNOWN / INFERRED /
  UNVERIFIED) — quoted from the file.
- KNOWN: `source_check.py` exits 2 on a claim with missing source/section or
  an unknown source id (code inspected).
- INFERRED: the four-part claim model (text/source/section/skill) is the
  de-facto schema of `registry/claims.yaml` (every record uses those fields).
- UNVERIFIED: section-number accuracy for all 56 claims (each would need
  manual spot-open of the cited source).

## Scoring

- precision: every flagged claim must lack a real section, a registered
  source, or an honest evidence level.
- recall: fabricated claims, duplicate claims, and evidence-inflated claims
  are all caught.
- FP-rate: the good extractor and the real registry claims produce zero flags.
