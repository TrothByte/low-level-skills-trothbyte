---
name: meta-security-audit
description: Use when reviewing low-level code or this repository for security. Teaches license compatibility checks, CVE regression verification, examples/bad scrutiny, and a security-review checklist with evidence.
---

# Meta: Security Audit

## When to use

- Reviewing C/C++/Rust/asm code for memory-safety, crypto, and side-channel
  issues before shipping.
- Auditing this repository: licenses of referenced sources, CVE-regression
  claims, and whether `examples/bad` are actually broken.
- Verifying that a claimed CVE fix or "security bug" is real and reproducible.
- Checking that security claims in skills/evals are evidence-backed, not
  vibes.

## When not to use

- Writing new code (that is the domain skill's job) — audit is review.
- Non-security correctness (an off-by-one in a debug printer with no security
  consequence) — use `meta-verification`.
- The audit target is already covered by a dedicated skill (e.g.
  `side-channel-constant-time-verification`) — defer to it for that class.

## What the agent often gets wrong

- B6: confident "this is exploitable" without a reproducer.
- Claiming a CVE regression is verified when only one side (vulnerable OR
  fixed) was built — the pair is required.
- Reading `examples/bad` and assuming they fail; never compiling them
  (bad-with-wrong-behavior compiles clean, which is the whole point).
- Treating a clean license list as "audited" without checking per-file
  licenses, SPDX mismatch, and the repo's own policy (MIT).
- CWE-classifying a finding without opening the CWE definition.
- Skipping constant-time/crypto claims entirely because "no crypto here"
  (Pearce/Perry: AI code is ~40% vulnerable on security tasks).

## How to reason correctly

1. **License audit**: list every source/package the repo or skill references,
   extract its license, compare with the repo's MIT policy and the
   referenced project's own license terms (provenance rule: no reuse without
   license compatibility). Record a per-item verdict.
2. **CVE regression**: for each claimed CVE, build the vulnerable snippet and
   the fixed snippet on this host; require the vulnerable build to fail under
   the gate (sanitizer/diff) and the fixed build to pass. Record both exit
   codes. No pair = UNVERIFIED.
3. **examples/bad scrutiny**: compile and run every `examples/bad` fixture —
   each must be genuinely broken (exit non-zero, sanitizer error, or wrong
   output). A bad example that behaves correctly is a lie in the skill.
4. **Security checklist**: run the standard classes — memory safety (CWE-787/
   119/416/415), integer (CWE-190/191), crypto misuse (nonce reuse, weak
   primitives), side-channel (CWE-1254 constant-time), TOCTOU (CWE-362) — and
   classify each finding with a CWE id, evidence, and severity.
5. Record everything: commands, outputs, verdicts in the audit report.

## What to verify

- Every CVE claim has a vulnerable+fixed build pair with recorded exit codes.
- Every `examples/bad` fixture is actually broken when compiled and run.
- Every referenced source's license is compatible with the repo policy; any
  incompatibility is reported, not hidden.
- Every CWE classification maps to the real CWE definition.
- Findings carry evidence (command + output), not confidence.

## How to verify

```
python tools/validate.py
python tools/source/source_check.py
python tools/lint/claim_extractor.py
python skills/_meta/meta-security-audit/examples/good/license_audit.py
python skills/_meta/meta-security-audit/examples/good/cve_regression_check.py
python skills/_meta/meta-security-audit/examples/bad/unsupported_cve_claim.py
```

## Where the knowledge comes from

- `registry/evals.yaml` (historical_cves core + secondary sets; FP cases).
- `cwe` / `cwe-1254` (weakness taxonomy for classification).
- `pearce-copilot`, `perry-ai-code`, `cyberseceval` (AI security-failure rates).
- `LICENSE.md` (repo policy), `docs/ACKNOWLEDGMENTS.md` (per-project licenses).
- `arxiv-2607-12507` (provenance-gating of unsafe binary actions).

## Related skills

- `meta-verification` (require) — gates and exit codes underpin every audit claim.
- `meta-evidence` (require) — findings are KNOWN/INFERRED/UNVERIFIED, never vibes.
- `meta-verification-harness-validity` (recommend) — the vulnerable-fixture
  pair is an ablation-delta check.
- `side-channel-constant-time-verification` (recommend) — constant-time class.
- `fuzzing-harness-evidence-gate` (recommend) — fuzz-discovered CVE claims.

## Evaluation

- Synthetic: `good/license_audit.py` and `good/cve_regression_check.py` are
  valid audit loops (recorded on host); `bad/unsupported_cve_claim.py` makes
  an unverifiable claim and must be rejected.
- False-positive: correct code must not be flagged as a vulnerability; an
  INFERRED finding labeled honestly is fine.
- Adversarial: a "security fix" that compiles clean and passes tests but
  still has the vuln (vulnerable==fixed behavior) must be caught by the pair
  run.
- Historical: CVE core set from `registry/evals.yaml` reproduced where the
  host toolchain allows; unbuildable cases marked UNVERIFIED.
