---
name: meta-claim-extraction
description: Use when documenting claims in a skill or SKILL.md/references. Teaches extracting claim → source → section → skill into registry/claims.yaml with confidence levels and KNOWN/INFERRED/UNVERIFIED evidence classification.
---

# Meta: Claim Extraction & Provenance

## When to use

- Writing or reviewing claims in a skill's SKILL.md, references, or evals.
- Adding a normative statement ("this is UB", "this ABI passes X in Y") that
  needs traceability.
- Preparing a new claim for `registry/claims.yaml` (claim → source → section
  → skill).
- Auditing whether existing claims are source-backed before marking a skill
  stable.

## When not to use

- Non-normative prose with no correctness impact — no evidence ceremony needed.
- The claim is trivially verifiable by compilation — still name the gate, but
  no registry entry is required.
- A skill in `draft`/`researched` status where the claim is explicitly
  UNVERIFIED — record it as UNVERIFIED, do not fabricate a source.

## What the agent often gets wrong

- B6: confident tone on invented API/behavior without a source.
- Claiming "KNOWN" for a fact that is only "INFERRED" or never verified.
- Citing a source id that is not registered (source_check fails) or a
  section number that does not exist.
- B18: recording a claim as extracted/complete when only part of the skill was
  scanned.
- Duplicating claims already in `registry/claims.yaml` instead of reusing the
  id (CL-XXX).
- Writing `- **SOURCE**:` lines in references that the source_check tool
  cannot match to `registry/sources.yaml`.

## How to reason correctly

1. Identify the normative claim: a sentence that asserts how a language, ABI,
   toolchain, or standard behaves.
2. Classify confidence: MUST_VERIFY (needs executable check), MUST_CONSIDER
   (context-dependent reasoning), OPTIONAL (reference info).
3. Classify evidence: KNOWN (source-backed + verified), INFERRED (derived
   from sources but not explicit), UNVERIFIED (needs verification — never
   present as fact).
4. Map claim → source id (must exist in `registry/sources.yaml`) → section
   (real section number/name, not invented) → skill id (must exist in
   `registry/skills.yaml`).
5. Add the record to `registry/claims.yaml`; in references, use the exact
   source id in `- **SOURCE**:` lines so `tools/source/source_check.py`
   matches.
6. Re-run the provenance validators.

## What to verify

- Every claim record has non-empty source and section.
- Source id exists in `registry/sources.yaml`; skill id exists in
  `registry/skills.yaml`.
- Section is real (open the source; do not invent numbers).
- Evidence classification matches: KNOWN only with a normative/official
  source; UNVERIFIED never in stable skills.
- No duplicate claim (same text+source) already registered as CL-XXX.

## How to verify

```
python tools/lint/claim_extractor.py
python tools/source/source_check.py
python tools/validate.py
python tools/lint/skill_lint.py skills/_meta/meta-claim-extraction/SKILL.md
```

## Where the knowledge comes from

- `registry/claims.yaml` (confidence + evidence model, provenance_checks).
- `registry/sources.yaml` (source ids + authority levels).
- `registry/skills.yaml` (skill ids).
- `tools/source/source_check.py`, `tools/lint/claim_extractor.py`.

## Related skills

- `meta-evidence` (require) — KNOWN/INFERRED/UNVERIFIED is this skill's core model.
- `meta-eval-runner` (recommend) — eval verdicts feed evidence classification.
- `meta-verification` (recommend) — evidence must be executable where possible.
- `meta-completion` (recommend) — a skill is complete only when claims are registered.

## Evaluation

- Synthetic: given a sample normative sentence, the agent must produce a claim
  record with correct source/section/confidence/evidence — use the extractor
  example as the fixture.
- False-positive: a correct, source-backed claim must NOT be flagged for
  "missing source"; a non-normative sentence must NOT be force-claimed.
- Adversarial: a claim with an invented section number or an unregistered
  source id must be rejected; "KNOWN" on an unverified fact must be downgraded.
- Historical: existing claims CL-001..CL-056 must each trace to a real source
  section (registry_check + source_check re-run).
