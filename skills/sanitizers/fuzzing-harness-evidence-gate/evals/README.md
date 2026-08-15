# Evaluation — fuzzing-harness-evidence-gate

Skill: `skills/sanitizers/fuzzing-harness-evidence-gate`.
Stability target: `researched`. Toolchain: clang/libFuzzer and AFL++ are NOT
installed in this environment. NOTHING here was executed. Every case below is
honestly marked "researched — toolchain not available; verification command:
<cmd>". Do not read this file as evidence that any fuzz run happened.

## Synthetic evals

| Case | Fixture | Expected | Status |
|---|---|---|---|
| easy/negative | `bad/harness_wrong_entry.c` | parser never reached; a "clean" run must not be reported | researched — not run |
| easy/negative | `bad/harness_trusts_input_size.c` | harness-level OOB, not a target finding | researched — not run |
| medium/negative | `bad/minimize_skipped.sh` | no repro/minimization — fails the gate | researched — not run |
| medium/negative | `bad/finding_without_evidence.md` | no artifact/repro/coverage — fails the gate | researched — not run |
| easy/positive | `good/harness_parser_entry.c` | correct entry, Size-guarded — passes | researched — not run |
| easy/positive | `good/minimize_and_reproduce.sh` | artifact → repro → minimize → re-verify | researched — not run |
| easy/positive | `good/finding_report.md` | all three artifacts + honest stats — passes | researched — not run |

Detection rule for the negative cases: the reviewer applies the three-part gate
(rule 9); any claim missing a reproducible report, a minimized input, or a
demonstrated reachable path is rejected, whatever the crash looks like.

## False-positive evals (correct code must not be flagged)

- `good/harness_parser_entry.c` — `pkt_parse(Data, Size)` after a `Size >= 8`
  guard is a correct harness; must NOT be flagged for "short-input" or "harness
  calls the target".
- `good/finding_report.md` — an honest report that states low coverage and
  omitted checks (UB/uninit not tested) must NOT be rejected; it states its
  bounds rather than overclaiming.
- `good/minimize_and_reproduce.sh` — `-artifact_prefix`, `-minimize_crash=1`,
  `-runs=1` usage is correct; must NOT be flagged.

## Historical evals

- CVE-2022-3602 (OpenSSL X.509 email-address stack buffer overflow, discovered
  through OSS-Fuzz): the agent must name the three required artifacts — a
  reproducible sanitizer report, a minimized crashing input, and a demonstrated
  reachable path — and the reproduce → minimize → disclose discipline, not just
  "the email parser overflowed". Exact OSS-Fuzz issue number and minimizer
  details are INFERRED; CVE facts are UNVERIFIED in this environment.
  Verification command:
  `clang -O1 -g -fsanitize=fuzzer,address -fno-omit-frame-pointer -o fuzz_x509 fuzz_x509.c && ./fuzz_x509 -runs=1 <artifact>`

## Adversarial evals

- A crash whose stack never leaves the harness (`__interceptor_memcpy` directly
  under `LLVMFuzzerTestOneInput`, no parser frame) must be rejected as
  non-evidence even though it is a real ASan crash.
- An OOM/timeout/SIGKILL artifact (resource-limit tool artifact) must not be
  reported as a sanitizer finding.
- An un-minimized multi-megabyte crash presented as "the reproducer" must be sent
  back for `-minimize_crash=1` / `afl-tmin`.
- "No findings" with no runtime, coverage, or limitations must be rejected as an
  unbounded claim.

## Verified facts

Nothing was executed. The statements below are researched against the registered
sources (libfuzzer-docs, aflpp-docs, oss-fuzz, clang-docs) and carry the exact
verification command to run on a toolchain-equipped machine.

- `LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)` is the sole driver
  entry; `Size` is authoritative.
  Verify: `clang -O1 -g -fsanitize=fuzzer,address -o fuzz fuzz.c && ./fuzz -runs=1 corpus/`
- `-minimize_crash=1` shrinks a crash artifact in place.
  Verify: `./fuzz -minimize_crash=1 -runs=1 ./art/crash-<hash>`
- `afl-tmin` / `afl-cmin` minimize one input / a corpus.
  Verify: `afl-tmin -i crashes/id:000000 -o crash.min -- ./target @@`
- MSan must not be combined with other sanitizers.
  Verify: `clang -O1 -g -fsanitize=fuzzer,memory ...` builds and runs; the mixed
  build `-fsanitize=fuzzer,memory,address` fails to instrument correctly.
- UBSan defaults to recover mode; `-fno-sanitize-recover=undefined` makes UB
  fatal so it produces a crash artifact.
  Verify: `clang -O1 -g -fsanitize=fuzzer,undefined -fno-sanitize-recover=undefined ...`
- `-print_final_stats=1` and `-print_coverage=1` provide the numbers a
  defensible "no findings" claim needs.
  Verify: `./fuzz -print_final_stats=1 -print_coverage=1 -runs=100 corpus/`

## Scoring (for routing eval)

- precision: every accepted claim has all three gate artifacts and honest bounds.
- recall: every bad fixture fails the gate; every good fixture passes.
- FP-rate: correct harnesses and honest reports produce zero rejections.

## Target toolchains (absent, documented)

- clang/libFuzzer and AFL++ are not installed here. The commands above are the
  exact ones to run on a toolchain-equipped machine; results must be recorded in
  this file when they are run.
