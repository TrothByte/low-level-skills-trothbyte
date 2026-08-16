# Meta-Claim-Extraction: The Claim Model — Reference Rules

Knowledge layer for `meta-claim-extraction`. RULE → WHY AI GETS IT WRONG →
CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE → VERIFICATION → SOURCE.

## 1. A claim is (text, source, section, skill)

- **RULE**: every normative claim recorded in `registry/claims.yaml` has
  exactly four traceable parts: the claim text, the source id (from
  `registry/sources.yaml`), the source section, and the skill id (from
  `registry/skills.yaml`). Missing any part makes the claim untraceable.
- **WHY AI GETS IT WRONG**: agents write claims like "X is UB" with no source
  at all, or cite the whole standard instead of the section. Both make the
  provenance check impossible and turn the claim into an assertion of
  authority rather than a testable statement.
- **CORRECT REASONING**: if you cannot name the section, you have not read
  the source. Claim text must be precise enough that a reader could verify it
  against the section.
- **EXAMPLE** (bad): "signed overflow is UB" with source `iso-c11-n1570` but
  no section — the section is where the real authority lives.
- **COUNTEREXAMPLE** (good): CL-003 — "Signed integer overflow — UB;
  unsigned — wrap (modulo 2^N)", source `iso-c11-n1570`, section "6.5 p5,
  6.2.5 p9", skill `c-integer-promotion-and-conversion`.
- **VERIFICATION**: `tools/source/source_check.py` rejects claims with empty
  source or section (exit 2).
- **SOURCE**: registry/claims.yaml (meta.rule, provenance_checks).

## 2. Evidence levels are a ladder, not a switch

- **RULE**: evidence is KNOWN (source-backed and consistent with
  verification), INFERRED (derived from a source but not explicit), or
  UNVERIFIED (needs verification). A claim's evidence level must match how
  the fact was actually established — never upgraded by tone.
- **WHY AI GETS IT WRONG**: agents classify by confidence of phrasing
  ("clearly", "obviously") instead of by what was verified. An INFERRED fact
  stated confidently becomes a fake KNOWN; an UNVERIFIED fact becomes a fake
  INFERRED.
- **CORRECT REASONING**: ask "what did I actually check?" If a standard says
  it and a compiler run confirmed it — KNOWN. If the standard implies it but
  no source states it — INFERRED. If nothing ran and no source states it —
  UNVERIFIED, and it cannot appear in a stable skill.
- **EXAMPLE** (bad): "the compiler will remove this check at -O2" asserted as
  KNOWN without running the compiler — the correct level is INFERRED or
  UNVERIFIED until the disassembly exists.
- **COUNTEREXAMPLE** (good): CL-012 — "Компилятор вправе считать UB
  недостижимым…" with evidence INFERRED, explicitly derived from
  `carruth-gigo` (talk-level claim, not normative text).
- **VERIFICATION**: `registry/claims.yaml` provenance rule — "UNVERIFIED
  claims запрещены в stable skill".
- **SOURCE**: registry/claims.yaml (meta.evidence_levels, provenance_checks).

## 3. Confidence is about obligation, not strength

- **RULE**: confidence (MUST_VERIFY / MUST_CONSIDER / OPTIONAL) says how an
  agent must treat the claim when reasoning: MUST_VERIFY requires an
  executable check before reliance; MUST_CONSIDER requires the reasoning to
  include it; OPTIONAL is reference material. It is orthogonal to evidence.
- **WHY AI GETS IT WRONG**: agents use confidence as a measure of certainty
  ("I am very confident, so MUST"), duplicating the evidence level and making
  the field useless for routing.
- **CORRECT REASONING**: confidence describes the SKILL's use requirement,
  evidence describes the FACT's grounding. A MUST_VERIFY claim may be KNOWN —
  the agent must still re-check because the compiler/version matters.
- **EXAMPLE** (bad): CL-018 (NASM case sensitivity) marked MUST_VERIFY only
  because the agent is "not sure" — it is KNOWN and could be MUST_CONSIDER.
- **COUNTEREXAMPLE** (good): CL-001 (strncpy NUL-termination) is MUST_CONSIDER
  + KNOWN: a real rule agents keep forgetting, verified against the standard.
- **VERIFICATION**: cross-check each claim's confidence against its
  verification requirement in the target skill.
- **SOURCE**: registry/claims.yaml (meta.confidence).

## 4. Source ids must be registered, sections must be real

- **RULE**: claims may only reference source ids present in
  `registry/sources.yaml`, and sections must match the source's actual
  content. The provenance validators (`source_check.py`, `registry_check.py`)
  enforce the first; only a human/agent actually opening the source can
  enforce the second.
- **WHY AI GETS IT WRONG**: agents invent plausible section numbers
  ("7.24.2.4" for strncpy — real; "§12.7.3" for a hypothetical — fake) or
  cite unregistered ids like a paper title instead of the id. The validator
  catches the id; the invented section sails through unless checked.
- **CORRECT REASONING**: for every claim, open the cited source and confirm
  the section states the claim. If the section cannot be found, the claim is
  UNVERIFIED regardless of how well-known the fact is.
- **EXAMPLE** (bad): a claim citing "iso-c11-n1570 §3.4" for heap behavior —
  that section does not cover it; the validator cannot catch it.
- **COUNTEREXAMPLE** (good): CL-015 (access to freed memory is UB) cites
  `iso-c11-n1570` section "7.22.3" (stdlib memory management) — real and
  on-point.
- **VERIFICATION**: `python tools/source/source_check.py` — exit 2 on unknown
  source id; manual spot-open for section accuracy.
- **SOURCE**: registry/sources.yaml; registry/claims.yaml (provenance_checks).

## 5. Reuse registered claims instead of duplicating

- **RULE**: if a claim's text+source+section already exists as CL-XXX in
  `registry/claims.yaml`, reference that id; do not add a second record.
  Duplicates rot: one gets fixed, the other keeps the old (wrong) source.
- **WHY AI GETS IT WRONG**: agents extract claims per-skill in isolation and
  re-register the same fact under a new id, splitting provenance. Cross-skill
  claims (e.g. strncpy semantics in both `c-string-and-buffer-safety` and
  `c-undefined-behavior`) then diverge.
- **CORRECT REASONING**: before adding CL-0NN, grep claims.yaml for the
  normalized claim text. Reuse the id; the skill field can list the second
  skill or a cross-link can connect them.
- **EXAMPLE** (bad): two CL entries with identical text "strncpy does not
  guarantee NUL-termination" under different ids.
- **COUNTEREXAMPLE** (good): CL-001 is the single record; other skills cite
  it via cross-links instead of re-registering.
- **VERIFICATION**: `python tools/lint/claim_extractor.py` — scan for
  duplicate normalized texts.
- **SOURCE**: registry/claims.yaml.
