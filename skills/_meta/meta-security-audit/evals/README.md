# Evaluation — meta-security-audit

Skill: `skills/_meta/meta-security-audit`. Stability: `researched` (the audit
loop logic and license logic are executed on this host 2026-08-17, Python
3.11.9; host-level CVE sanitizer pairs require the domain skills' fixtures and
their toolchains — marked UNVERIFIED where not run).

## Synthetic evals

- easy/positive: `good/license_audit.py` — per-artifact license check against
  the MIT policy; flags CC-BY-NC and GPL-3.0 (recorded on host).
- easy/positive: `good/cve_regression_check.py` — vulnerable+fixed pair logic;
  verifies detection is demonstrated before "CLAIM VERIFIED" (recorded).
- easy/negative: `bad/unsupported_cve_claim.py` — claims Heartbleed fixed with
  no build pair — must be rejected.
- medium/positive: classifying a finding to the correct CWE by opening the
  definition (CWE-457 vs CWE-787 confusion avoided).

Detection rule: a security claim is verified only as a pair of recorded
builds (vulnerable FAILS, fixed PASSES); a finding needs evidence
(command+output+exit code); a bad example must be actually broken.

## False-positive evals

- `good/license_audit.py` and `good/cve_regression_check.py` are correct
  audits and must not be flagged.
- Apache-2.0 / BSD-2-Clause / BSD-3-Clause artifacts under MIT policy are
  compatible (permissive), not findings.
- Correct code with no vulnerability must not receive a CWE classification
  (rule 4: class only when the CWE definition matches).

## Historical evals

- CVE-2016-8617 (integer overflow before alloc) is modeled by
  `good/cve_regression_check.py` (class from `registry/evals.yaml`
  core_eval_set). Actual UBSan build on this host is possible for a minimal C
  fixture — recorded as the target verification in the domain skills.
- CVE-2014-0160 (Heartbleed) — `bad/unsupported_cve_claim.py` demonstrates
  the unverifiable-claim shape (secondary set entry).
- The repo's own license audit (docs/ACKNOWLEDGMENTS.md) is the historical
  artifact set for the license check.

## Adversarial evals

- A "security fix" that compiles clean and passes tests but still has the
  vuln (vulnerable==fixed behavior) — the pair run must expose it
  (ablation-delta on security fixes).
- An examples/bad fixture that exits 0 with correct output (bad by label
  only) — the audit must catch it by compiling and running.
- A license row approved from a README badge when the LICENSE file says
  otherwise — the audit requires per-artifact evidence.

## Verification commands

```
python tools/validate.py
python tools/source/source_check.py
python tools/lint/claim_extractor.py
python skills/_meta/meta-security-audit/examples/good/license_audit.py
python skills/_meta/meta-security-audit/examples/good/cve_regression_check.py
python skills/_meta/meta-security-audit/examples/bad/unsupported_cve_claim.py
```

Recorded 2026-08-17 (Python 3.11.9, Windows):

```
> python examples/good/license_audit.py
  OK   trailofbits/something: ok
  FAIL vendor/firmware-blog: CC-BY-NC-4.0 non-commercial
  FAIL gnu/toolchain: GPL-3.0 copyleft incompatible with MIT policy
  audit: incompatibilities found — report, do not hide    exit 1
> python examples/good/cve_regression_check.py
  vulnerable build: x=50000 -> 3392 (wrap: True)
  fixed build:      x=50000 -> -1 (guarded: True)
  CVE regression pair: vulnerable FAILS, fixed PASSES -> claim VERIFIED   exit 0
> python examples/bad/unsupported_cve_claim.py
  "CVE-2014-0160: FIX VERIFIED"   exit 0   <- fabricated, no build pair
> python tools/source/source_check.py
  source_check: 177 sources, 56 claims checked   exit 0 (no WARN)
> python tools/validate.py
  ALL CHECKS PASSED
```

## Verified facts

- KNOWN: `registry/evals.yaml` defines the CVE core/secondary sets with
  class/detect/fix/verify per entry (file read).
- KNOWN: CWE-1254 describes early-exit string comparison leaking length
  (`cwe-1254` registered source; used by CL-037).
- INFERRED: the pair-of-builds rule is the security specialization of the
  ablation-delta rule (`meta-verification-harness-validity`) — same principle,
  applied to claims about vulnerabilities.
- UNVERIFIED on this host: the full CVE fixture builds under real sanitizers
  for all 16 registry CVEs (they live in domain skill directories and require
  per-CVE setup).

## Scoring

- precision: every flagged finding must have evidence (command+output+exit
  code) or be explicitly downgraded to INFERRED/UNVERIFIED.
- recall: unsupported CVE claims, lying bad-examples, license incompatibilities,
  and wrong CWE classes are all caught.
- FP-rate: correct code, permissive licenses, and properly-paired CVE claims
  produce zero flags.
