# Meta-Security-Audit: Security Review — Reference Rules

Knowledge layer for `meta-security-audit`. RULE → WHY AI GETS IT WRONG →
CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE → VERIFICATION → SOURCE.

## 1. A CVE claim is a pair of recorded builds, not a sentence

- **RULE**: "this skill covers CVE-XXXX" is a claim with two mandatory
  halves: (a) the vulnerable fixture builds and FAILS under the gate, and (b)
  the fixed fixture builds and PASSES. Both exit codes recorded. One half
  alone proves nothing.
- **WHY AI GETS IT WRONG**: agents build only the fixed version (it compiles,
  the fix "works") or only the vulnerable one (it crashes, "bug confirmed").
  Without the pair you cannot tell the gate actually detects the class — this
  is the ablation-delta applied to security claims.
- **CORRECT REASONING**: the vulnerable build is the positive control. If it
  fails and the fixed build passes, the pair demonstrates detection; anything
  else (both fail, both pass, one unbuildable) is UNVERIFIED on this host.
- **EXAMPLE** (bad): a skill claims CVE-2016-8617 coverage; the agent shows
  only the fixed malloc guard, "compiles clean". No vulnerable build → the
  claim is unsupported.
- **COUNTEREXAMPLE** (good): vulnerable build with UBSan reports "runtime
  error: signed integer overflow", fixed build clean, both commands+exit
  codes recorded.
- **VERIFICATION**: `python examples/good/cve_regression_check.py` — the
  script models this pair logic; in the repo, run per-CVE fixtures under the
  sanitizer named in `registry/evals.yaml`.
- **SOURCE**: registry/evals.yaml (historical_cves: each has class/detect/
  fix/verify); arxiv-2607-00107.

## 2. examples/bad must be bad when compiled

- **RULE**: every `examples/bad` fixture in a skill must be demonstrably
  broken: compile+run it and record the failure (non-zero exit, sanitizer
  error, wrong output). A bad example that behaves correctly certifies
  nothing and misleads learners.
- **WHY AI GETS IT WRONG**: agents write "bad" examples by deleting an assert
  or marking a comment, and never run them — many "bad" fixtures turn out to
  compile clean and produce correct output because the defect was never real
  or is optimized away.
- **CORRECT REASONING**: bad is a runtime property, not a label. During the
  audit, every examples/bad file gets compiled and executed; any that exits 0
  with correct output is a defect in the skill and must be fixed or removed.
- **EXAMPLE** (bad): an "unsafe strcpy" example that never actually overflows
  on the fixture input — runs, exits 0, prints the expected string.
- **COUNTEREXAMPLE** (good): the same example uses an oversized input, ASan
  aborts, exit code non-zero — recorded in evals/README.md.
- **VERIFICATION**: compile and run each examples/bad fixture; record
  exit codes; any exit-0-with-correct-output fixture is flagged.
- **SOURCE**: arxiv-2606-20128 (certifying-by-label);
  meta-verification-harness-validity (ablation-delta discipline).

## 3. License audit is per-artifact, not per-repo

- **RULE**: the audit checks each referenced source/package's license against
  the repo's policy (this repo: MIT for its own code) and against the
  referenced project's own license terms. SPDX id, author, and any
  "non-commercial"/copyleft restrictions are recorded per artifact.
- **WHY AI GETS IT WRONG**: agents cite "MIT license" from a README badge
  without opening the actual LICENSE file, or approve a CC-BY-NC or GPL
  dependency inside an MIT project without flagging the incompatibility.
- **CORRECT REASONING**: license claims are KNOWN only when the LICENSE file
  is read (not the badge). The audit output is a table: artifact → license →
  compatible? → evidence.
- **EXAMPLE** (bad): a skill's reference table lists a CC-BY-NC repo as
  "freely usable" because the README said "open".
- **COUNTEREXAMPLE** (good): `good/license_audit.py` flags
  non-commercial and copyleft entries against a policy and reports per-item
  compatibility.
- **VERIFICATION**: `python examples/good/license_audit.py` — recorded output
  on this host.
- **SOURCE**: LICENSE.md (repo policy); docs/ACKNOWLEDGMENTS.md (per-project
  licenses already catalogued in this repo).

## 4. CWE classification requires opening the definition

- **RULE**: a finding is classified as CWE-XXXX only after reading that CWE's
  definition and confirming the code matches the described weakness. CWE ids
  are citations, not decoration.
- **WHY AI GETS IT WRONG**: agents attach CWE numbers from memory or from a
  vague similarity; the classification then misroutes the fix. (Pearce et al.:
  ~40% of AI-generated C code is vulnerable — accurate classification matters
  because the volume is high.)
- **CORRECT REASONING**: for each finding, open the CWE page, match the
  trigger condition, and quote the CWE's own description in the evidence.
  If no CWE matches, leave the class blank rather than force one.
- **EXAMPLE** (bad): labeling an uninitialized-read as CWE-787 (out-of-bounds
  write) — wrong class, wrong fix.
- **COUNTEREXAMPLE** (good): uninitialized read → CWE-457 (uninitialized
  variable), definition opened and quoted.
- **VERIFICATION**: open the CWE id in `cwe` source and diff the trigger
  condition against the finding.
- **SOURCE**: cwe; cwe-1254.

## 5. Constant-time and crypto classes need their own gates

- **RULE**: any secret-dependent code (comparison, branch, table lookup,
  key/IV handling) is audited for timing/side-channel classes with a
  dedicated gate (dudect/ctgrind or asm inspection), not by eyeballing. Crypto
  misuse (nonce reuse, weak primitives) is checked against the standard the
  primitive claims to implement.
- **WHY AI GETS IT WRONG**: agents skip the class ("no crypto here") or
  certify constant-time by code reading. AI code is disproportionately weak
  here (CyberSecEval; arxiv-2604-27001: 57% vulnerable crypto Rust) — this is
  the highest-signal, most-skipped class.
- **CORRECT REASONING**: if a function touches secrets, the checklist item
  "constant-time" is mandatory, not optional. The gate is executable
  (ctgrind/dudect) or a disassembly check for secret-dependent branches.
- **EXAMPLE** (bad): a login token comparison uses `strcmp` with early exit —
  passed review "because it's only auth". Length leaks via timing.
- **COUNTEREXAMPLE** (good): constant-time compare (fixed iterations) and an
  asm/timing check recorded (see `side-channel-constant-time-verification`,
  which measured 0.054s early-exit vs ~0s constant-time on this host).
- **VERIFICATION**: ctgrind/dudect or asm inspection; record the output.
- **SOURCE**: cwe-1254; pearce-copilot; perry-ai-code; cyberseceval;
  arxiv-2604-27001.
