# Fuzzing Harness Evidence Gate — Reference Rules

Sources: libfuzzer-docs, aflpp-docs, oss-fuzz, clang-docs (registered ids in
registry/sources.yaml). `sanitizer-report-reading` is a skill, not a source.
Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE
→ VERIFICATION → SOURCE. All rules are researched against the official docs; this
environment has no clang/libFuzzer/AFL toolchain, so commands are listed but not
executed here (UNVERIFIED in this environment).

## 1. Map the target before writing the harness

- **RULE**: find the entry function that consumes untrusted bytes (the parser or
  format decoder) and its exact signature, then write the harness to call THAT
  function. Everything between the fuzzed buffer and the vulnerable code must be
  pure bytes-to-code, not environment.
- **WHY AI GETS IT WRONG**: harnesses a convenience wrapper that reads a file path
  or stdin, or calls an unrelated function, so mutations never reach the parser;
  then reports "no crashes" as if the parser had been fuzzed.
- **CORRECT REASONING**: libFuzzer and AFL mutate raw bytes in memory. If the real
  entry is `pkt_parse(uint8_t *buf, size_t len)`, the harness body is one call to
  it. If the entry takes a path, the bridge (temp file per call) must be explicit
  and flagged as a limitation — it changes mutation dynamics and is slow.
- **EXAMPLE** (bad): `examples/bad/harness_wrong_entry.c` — the harness converts
  `Data` with `atoi` and returns; `pkt_parse` is never called.
- **COUNTEREXAMPLE** (good): `examples/good/harness_parser_entry.c` — a
  `Size >= 8` guard, then exactly `pkt_parse(Data, Size)`.
- **VERIFICATION**: run with `-print_coverage=1` (or `-print_pcs=1`) and confirm
  the parser's lines appear in the trace; or break on the parser entry and confirm
  it fires.
- **SOURCE**: libfuzzer-docs; aflpp-docs.

## 2. The LLVMFuzzerTestOneInput contract

- **RULE**: the libFuzzer driver calls exactly one function,
  `int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)`, once per input.
  It must be reentrant: no per-call assumptions about prior state, no reliance on
  globals mutated by a previous input.
- **WHY AI GETS IT WRONG**: writes `LLVMFuzzerTestOneInput(char *s)` or reuses
  results across calls; keeps a static cache so input N's result depends on input
  1..N-1, making crashes order-dependent and non-reproducible.
- **CORRECT REASONING**: `Data` points to a buffer of exactly `Size` bytes; there
  is no NUL terminator. Reset per-input state at the top of the function; treat
  every call as independent.
- **EXAMPLE** (bad): `int LLVMFuzzerTestOneInput(char *p)` — wrong signature,
  does not compile with `-fsanitize=fuzzer`.
- **COUNTEREXAMPLE** (good): `int LLVMFuzzerTestOneInput(const uint8_t *Data,
  size_t Size) { return pkt_parse(Data, Size); }`.
- **VERIFICATION**: compile with `clang -O1 -g -fsanitize=fuzzer,address
  harness.c`; a wrong signature fails to link the driver.
- **SOURCE**: libfuzzer-docs.

## 3. Size is the only trusted length

- **RULE**: the value of `Size` is the sole authority on buffer length. Never
  derive a length from bytes inside `Data` and never read `Data` beyond `Size`.
- **WHY AI GETS IT WRONG**: does `uint32_t n = *(uint32_t*)Data;` then loops to
  `n` or copies `n` bytes — an attacker-controlled bound. When the fuzzer feeds a
  short input, ASan fires inside the harness, and the agent reports a "target bug"
  that is actually harness code.
- **CORRECT REASONING**: guard with `if (Size < kMin) return 0;` before any parse.
  If the protocol genuinely needs a header length, validate it against `Size`
  (`n <= Size - offset`) — that check is the target's job to make correctly, not
  the harness's to guess.
- **EXAMPLE** (bad): `examples/bad/harness_trusts_input_size.c` — reads 4 bytes
  unconditionally and `memcpy`s `n` attacker-controlled bytes into a 16-byte stack
  buffer.
- **COUNTEREXAMPLE** (good): `if (Size < 8) return 0;` then
  `pkt_parse(Data, Size)`; the parser validates its own header.
- **VERIFICATION**: compile with ASan and fuzz; the bad harness produces
  `stack-buffer-overflow` with frames that never enter the parser.
- **SOURCE**: libfuzzer-docs; clang-docs (AddressSanitizer).

## 4. Sanitizer selection and build flags

- **RULE**: instrument the fuzz build with the sanitizer(s) that cover the
  suspected bug class: `-fsanitize=fuzzer,address`, `-fsanitize=fuzzer,undefined`,
  or `-fsanitize=fuzzer,memory`. MSan must NOT be combined with other sanitizers
  and requires every library in the process to be MSan-instrumented. UBSan
  defaults to recover mode — make it fatal with `-fno-sanitize-recover=undefined`
  so UB yields a crash artifact. AFL++ builds use `AFL_USE_ASAN=1` /
  `AFL_USE_UBSAN=1` with `afl-clang-fast` or `afl-clang-lto`.
- **WHY AI GETS IT WRONG**: reports only ASan results for a bug class only UBSan
  or MSan can see (signed shift, uninit read); builds `-fsanitize=fuzzer,memory,
  address` and gets a silent non-functional MSan; leaves UBSan recovering, so UB
  prints as a log line and never becomes an artifact the gate can hold.
- **CORRECT REASONING**: choose the tool by bug class and by what the codebase can
  be fully instrumented with; for fuzzing, a finding IS an artifact. UB must be
  fatal; MSan must run alone with all dependencies instrumented.
- **EXAMPLE** (bad): a single run with `-fsanitize=fuzzer,address` then "clean" for
  a signed-shift bug the parser definitely has.
- **COUNTEREXAMPLE** (good): two builds —
  `-fsanitize=fuzzer,address` and `-fsanitize=fuzzer,undefined
  -fno-sanitize-recover=undefined`; both report `-print_final_stats=1` numbers.
- **VERIFICATION**: `clang -O1 -g -fsanitize=fuzzer,memory,address` fails to
  produce a working MSan build; `-fsanitize=fuzzer,memory` alone builds and runs.
- **SOURCE**: clang-docs; libfuzzer-docs; aflpp-docs.

## 5. Reproducibility: a finding is an artifact, not a memory

- **RULE**: a finding must reproduce from a saved input against a pinned binary.
  libFuzzer writes crash artifacts under `-artifact_prefix` as `crash-<sha>`; rerun
  with `./fuzzer -runs=1 ./art/crash-<sha>`. AFL++ writes `out/crashes/id:*`; rerun
  the target with the `@@` marker to confirm.
- **WHY AI GETS IT WRONG**: reports a crash seen once in a long run with no saved
  input; rebuilds between run and repro; counts OOM, timeout, or SIGKILL (resource
  limits, tool kills) as sanitizer findings — those are tool artifacts.
- **CORRECT REASONING**: resource-limit aborts are the fuzzer protecting itself,
  not evidence. The artifact + pinned binary + one command must reproduce the exact
  report. Never change build flags between the finding and the repro.
- **EXAMPLE** (bad): `examples/bad/finding_without_evidence.md` — "a crash was seen
  during a two-minute run", no artifact, no command, no coverage.
- **COUNTEREXAMPLE** (good): `examples/good/minimize_and_reproduce.sh` — artifact
  saved, `-runs=1` repro, then minimization, then re-verification.
- **VERIFICATION**: `./fuzzer -runs=1 ./art/crash-<sha>` exits non-zero with the
  same `ERROR: AddressSanitizer` header on the same binary.
- **SOURCE**: libfuzzer-docs; oss-fuzz.

## 6. Minimization before reporting

- **RULE**: the reported finding uses a minimized input. libFuzzer's
  `-minimize_crash=1` reduces a crash artifact in place; AFL++ provides `afl-tmin`
  for a single input and `afl-cmin` for a corpus.
- **WHY AI GETS IT WRONG**: ships the first crash file (often megabytes) or a
  thousand-file corpus and claims "the bug needs this input"; a fat reproducer is
  itself evidence the harness or target understanding is wrong.
- **CORRECT REASONING**: minimization shrinks the input while preserving the
  crash; the minimized input is the unit of triage. After minimizing, re-run the
  repro on the minimized file — minimization that changes the crash is a bug.
- **EXAMPLE** (bad): `examples/bad/minimize_skipped.sh` — copies `crashes/id:*`
  straight into the report.
- **COUNTEREXAMPLE** (good): `afl-tmin -i crashes/id:000000 -o crash.min -- ./target
  @@` then `./target crash.min` reproduces the crash.
- **VERIFICATION**: `./fuzzer -minimize_crash=1 -runs=1 ./art/crash-<sha>` writes a
  smaller `minimized-from-<sha>` that still crashes; for AFL,
  `afl-tmin -i <in> -o <out> -- ./target @@` exits with the artifact smaller than
  the input and still crashing.
- **SOURCE**: libfuzzer-docs; aflpp-docs.

## 7. Demonstrate the reachable path

- **RULE**: the report's stack must run from the harness entry through the target's
  parse functions to the fault. The vulnerable function must be provably reached
  from fuzzed input.
- **WHY AI GETS IT WRONG**: a crash whose stack shows only `__interceptor_*`,
  `memcpy`, and `LLVMFuzzerTestOneInput` is blamed on the target; or a crash
  "near" the code with no parser frame is presented as the bug.
- **CORRECT REASONING**: every frame between the driver entry and the fault names
  the path. If the first target frame appears only below `memcpy`, the fault is in
  how the harness used the buffer, not in parsing. Coverage output
  (`-print_pcs=1`) is the same evidence stated as a set of reached PCs.
- **EXAMPLE** (bad): a report whose stack is `__interceptor_memcpy ->
  LLVMFuzzerTestOneInput` — the harness copied attacker-controlled bytes; the
  parser never ran.
- **COUNTEREXAMPLE** (good): `LLVMFuzzerTestOneInput -> pkt_parse -> parse_record ->
  read overflow at pkt_parse.c:42:18`.
- **VERIFICATION**: `./fuzzer -print_pcs=1 -runs=1 ./art/crash-<sha>` and confirm
  `pkt_parse`/`parse_record` addresses appear; or read the frames in the report.
- **SOURCE**: oss-fuzz; clang-docs (AddressSanitizer stack format).

## 8. "No findings" is a claim with stated bounds

- **RULE**: "no bugs found" is meaningful only with runtime, exec rate, corpus
  size and growth, coverage, sanitizer set, the exact entry point fuzzed, and a
  list of what was NOT covered (formats with checksums, structure-aware mutations,
  multi-buffer parsers, fuzzers without a seed corpus).
- **WHY AI GETS IT WRONG**: runs 60 seconds on one empty corpus, prints
  "fuzzed the parser, no bugs", and the reader treats that as a safety proof.
- **CORRECT REASONING**: fuzzing shows the absence of crashes within stated
  resource bounds, not the absence of bugs. Report the numbers (`-print_final_stats
  =1`) so the claim is falsifiable, and state the coverage gap.
- **EXAMPLE** (bad): "No findings." (nothing else).
- **COUNTEREXAMPLE** (good): "120 s, 3 workers, 48k exec/s, corpus 220 inputs,
  line coverage 41% over parse paths, ASan+UBSan-fatal; input carries a CRC field,
  so mutations after byte 4 are mostly rejected; entry fuzzed: pkt_parse."
- **VERIFICATION**: `./fuzzer -print_final_stats=1 -print_coverage=1
  -max_total_time=120 ./corpus` and transcribe the stats line into the report.
- **SOURCE**: libfuzzer-docs; oss-fuzz.

## 9. The evidence gate (proof standard)

- **RULE**: no CVE/security claim is acceptable without (a) a reproducible
  sanitizer report, (b) a minimized crashing input, (c) a demonstrated reachable
  path. All three must exist independently and cross-check on the same pinned
  binary.
- **WHY AI GETS IT WRONG**: files a claim from a single non-reproducible crash, or
  from a harness that never reached the vulnerable code, or from an un-minimized
  artifact — the report, the input, and the path are one indistinguishable blob.
- **CORRECT REASONING**: the gate is conjunctive. Missing (a) → rerun with the
  artifact; missing (b) → minimize; missing (c) → prove the parser is on the
  stack. The standard is the working hypothesis this skill operationalizes
  (INFERRED, idea only, from a third-party fuzzing skill 0xazanul/fuzz-skill); its
  three parts correspond to oss-fuzz's reproduce → minimize → disclose process.
- **EXAMPLE** (bad): a claim with one stack trace copied from a CI log of a
  different build.
- **COUNTEREXAMPLE** (good): `examples/good/finding_report.md` — reproducer
  command, 27-byte minimized input, stack with parser frames, and coverage/
  runtime/limitations stated.
- **VERIFICATION**: for each of (a), (b), (c), run the listed command
  (`./fuzzer -runs=1 <artifact>`, the minimizer command, `-print_pcs=1`) and record
  the outputs.
- **SOURCE**: oss-fuzz (reproduce, minimize, disclosure process); INFERRED:
  0xazanul/fuzz-skill proof standard (idea only, not a registered source).

## Quick recognition table

| Symptom in a fuzz report | Verdict | Rule |
|---|---|---|
| crash seen once, no saved input | observation, not a finding | 5 |
| crash only from a 10 MB file | must be minimized | 6 |
| stack: `memcpy` under the driver, no parser | harness bug, not target | 7 |
| "no bugs" with no stats or coverage | claim has no bounds | 8 |
| MSan+ASan in one build | MSan silently disabled | 4 |
| UB printed but exit 0 | recover mode; make it fatal | 4 |
| report + input + stack all present | evidence gate passes | 9 |
