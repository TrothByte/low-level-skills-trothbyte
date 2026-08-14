# Contributing to Low-level skills TrothByte

Thanks for your interest. This repository is an engineering product, not a pile of
skills. Read [`AGENTS.md`](AGENTS.md) first — it defines the engineering rules, the
resume protocol, and the definition of "done" for every skill.

## The non-negotiables

1. No skill without a registry entry in [`registry/skills.yaml`](registry/skills.yaml).
2. No uniqueness claim without a gap analysis.
3. Normative claims must trace to a primary source in [`registry/claims.yaml`](registry/claims.yaml).
4. Every mature skill needs a verification method; every important skill needs an eval.
5. Run the validators before committing:

   ```bash
   python tools/lint/skill_lint.py
   python tools/lint/registry_check.py
   python tools/source/source_check.py
   ```

## A skill is complete only when

```text
DISCOVERED → DIFFERENTIATED → SOURCE-BACKED → IMPLEMENTED → VERIFIED → EVALUATED →
CALIBRATED → TOKEN-OPTIMIZED → REGISTERED → MARKED COMPLETE
```

## Process

1. Open an issue describing the gap and the primary sources you plan to use.
2. Follow the phase order in [`roadmap/progress.yaml`](roadmap/progress.yaml).
3. Update [`roadmap/progress.yaml`](roadmap/progress.yaml) and [`WORKLOG.md`](WORKLOG.md)
   after every completed unit.
