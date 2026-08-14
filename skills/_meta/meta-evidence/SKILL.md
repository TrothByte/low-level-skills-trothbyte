---
name: meta-evidence
description: Use whenever making a normative or factual claim about C/C++/Rust/asm/ABI/UB/compiler behavior. Enforces the KNOWN / INFERRED / UNVERIFIED classification and requires source-backed evidence for strong claims.
---

# Meta: Evidence & Provenance

## When to use

- Stating "this is UB", "this ABI passes X in Y", "the compiler will do Z".
- Reviewing code where a correctness claim depends on a standard/spec/ABI.
- Writing or updating skill references.

## What the agent often gets wrong

- Stating speculation as fact ("the struct goes in registers", "this is implementation-defined").
- Confusing inferred behavior (from one compiler) with normative rules.
- No source section cited for normative claims.
- "It worked in my test, so it's universally true" (platform/version bias).

## How to reason correctly

1. Classify each claim:
   - **KNOWN** — source-backed (standard/spec/official doc) and consistent with verification.
   - **INFERRED** — derived from sources/practice but not explicit (say so; mark uncertainty).
   - **UNVERIFIED** — needs verification; NEVER present as fact.
2. For normative claims, cite `registry/claims.yaml` entry → source → section.
3. Verify platform/version sensitivity: ABI claims are per-OS+compiler (SysV vs Win64 vs AAPCS64).

## What to verify

- Every strong claim maps to a source id in `registry/sources.yaml`.
- UNVERIFIED claims are explicitly labeled and never appear in stable skills.
- Claims about asm/ABI are confirmed by disassembly or ABI docs, not memory.

## When not to use

- Non-normative prose where a claim does not affect correctness — no evidence ceremony needed.
- When a claim is trivially verifiable by compilation; still state the gate used.

## How to verify

- For each KNOWN claim, open the cited source section and confirm the statement matches.
- For INFERRED claims, record the reasoning chain from the source to the inference.

## Where the knowledge comes from

- `registry/claims.yaml`, `registry/sources.yaml`.

## Related skills

- `meta-rationalizations` — reject evidence-free confidence.
- `meta-verification` — evidence feeds the verification gate choice.

## Evaluation

- Claims in stable skills must be KNOWN (source-backed); UNVERIFIED is forbidden there.
- `tools/source/source_check.py` automates the source-reference audit.
