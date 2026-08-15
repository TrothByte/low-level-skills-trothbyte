---
name: debugging-crash-triage-discipline
description: Use when debugging a crash, segfault, or access violation in C/C++ or low-level code. Guides reliable reproduction, capturing backtraces and registers, deciding whether the crash site is the bug or corruption surfaces later, and verifying the fix. Prevents merry-go-round hypothesis churn and false "fixed" verdicts.
---

# Crash Triage Discipline

A crash is a stopping point, not an answer. The faulting instruction only
proves that earlier code produced the state it dereferenced. The discipline:
reproduce exactly, minimize, classify, change one variable, verify the root
cause is gone — in that order.

## When to use

- A native crash (segfault, access violation, trap) in C/C++ or low-level
  code, especially with a backtrace whose top frame "looks innocent".
- Intermittent or input-dependent crashes that survive a superficial fix.
- Reviewing another agent's (or your own) crash fix for symptom-vs-cause.
- Heap corruption, use-after-free, or uninitialized-data suspicion where the
  crash location does not match the buggy write.

## When not to use

- Logic errors with no fault (wrong value, wrong total) and no crash — the
  loop still applies to corruption, but start from the value producer.
- Debugging at the language level in a managed runtime (JVM, Go) — the
  tooling differs (this skill targets gdb + a C toolchain).
- Performance tuning, deadlocks without a crash, or remote-debugging-only
  setups where you cannot reproduce locally.

## What the agent often gets wrong

- Trusts the error location: edits the function named by the backtrace, even
  though that function only reads already-corrupt state. KNOWN, bundled
  `examples/bad/crash_site.c`: the fault is in `strlen`/`print_owner`, the
  cause is an unbounded `memcpy` in `ingest`.
- "Exit code 0 means fixed": guards the symptom, declares victory, and leaves
  the corruption in place. KNOWN, `examples/bad/symptom_guard.c` exits 0 with
  a wrong total (`-2105375976` instead of 185).
- Hypothesis merry-go-round: fixes A, breaks B, fixes B, breaks A, with no
  baseline and no single-variable experiments. KNOWN, reported in the Gemini
  CLI parser 2025 incident.
- Wrong-layer diagnosis followed by a false "fixed" verdict: the symptom
  stops crashing while the root cause remains. KNOWN, reported in
  claude-code#78133.
- Rewrites the whole subsystem because one object looks corrupt, when a
  single unchecked write is the cause. KNOWN, reported in the mropert 2026
  lighting-system rewrite.
- Skips registers and memory inspection, so it cannot tell a local fault
  (bad logic at the crash site) from corruption that surfaces later.

## How to reason correctly

1. Reproduce reliably and capture the EXACT crash before any edit:
   backtrace plus the faulting frame's registers and the offending pointer.
2. Minimize the input and the code path; stub unrelated modules so the set
   of writers that could corrupt the faulting memory is small.
3. Classify the fault. Ask: was this memory valid earlier and broken later
   (OOB write, UAF, uninitialized data), or is the faulting read itself the
   defect? Garbage-looking field bytes are payload leaked by an overflow; a
   stale pointer is a UAF; a NULL no legitimate path can produce is a
   candidate for uninitialized data. INFERRED, from the patterns of the
   registered empirical incidents.
4. Change one variable per experiment and re-run the same reproducer.
5. Verify the fix: original crash gone under the same reproducer AND output
   correctness checks (regression tests) pass. A fix that only suppresses the
   crash is not a fix.

## What to verify

- The exact crash: same signal, same frames, under the same reproducer,
  before and after each change.
- The corruption signature: neighbor structs/fields around the faulting value
  are intact or are clobbered by a specific writer.
- One experiment, one change (check `git diff` per iteration).
- Post-fix: exit 0 AND correct outputs AND the regression assertion passes.
- No residual dead guards left in place of the removed root cause.

## How to verify

Compile with debug info and no optimization, run under batch gdb, and record
the backtrace and registers:

```
gcc -g -O0 -Wall -Wextra examples/bad/crash_site.c -o evals/crash_site.exe
gdb -batch -ex "set pagination off" -ex run -ex bt -ex "info registers rip rsp rdi" --args ./evals/crash_site.exe
```

At the fault, inspect the memory around the crash (frame 3 = main):

```
gdb -batch -ex "set pagination off" -ex run -ex bt -ex "frame 3" -ex "p/x acc[2].balance" -ex "p/x acc[2].owner" -ex "p/x acc[3].owner" --args ./evals/crash_site.exe
```

On toolchains without ASan — clang is not installed here and MinGW gcc cannot
link it (`ld: cannot find -lasan`, KNOWN, measured 2026-08-15) — manual gdb
field inspection is the substitute: `0x41414141` bytes in a `long` field are
the payload of an overflow, not a runtime-library artifact. The recorded
output is in `evals/README.md`.

## Where the knowledge comes from

- `gdb-manual` — batch invocation, backtraces, register and memory inspection.
- `empirical (Gemini CLI parser 2025)` — hypothesis merry-go-round and
  unminimized input.
- `empirical (claude-code#78133)` — wrong-layer diagnosis and a false "fixed"
  verdict while the root cause remained.
- `empirical (mropert 2026)` — single corrupt texture led to a full lighting
  subsystem rewrite; the fix was one unchecked write.
- Toolchain and example facts above are KNOWN: measured on this host
  (gcc 16.1.0 MinGW UCRT, gdb 17.2, ASan unavailable) on 2026-08-15.

## Related skills

- `debugging-instrumentation-over-reasoning` — build on this skill when the
  choice is more instrumentation vs. more hypothesis churn.
- `sanitizer-report-reading` — where ASan/MSan are available, use it to find
  the write site this skill locates by manual field inspection.
- `c-string-and-buffer-safety` and `c-undefined-behavior` — the defect classes
  (OOB, UAF, uninitialized data) this skill's loop is designed to expose.
- `asm-x86-64-registers-and-addressing` — interpreting raw register state at
  the fault frame.

## Evaluation

- Synthetic: `examples/bad/crash_site.c` must crash reproducibly and its
  backtrace must lead to the wrong conclusion (crash site in `print_owner`),
  with the root cause found only via field inspection; `examples/bad/
  symptom_guard.c` must exit 0 while corrupting data (false fix); `examples/
  good/disciplined.c` must exit 0 with `total: 185` and the rejection message.
- False-positive: `examples/good/disciplined.c` must never be flagged as
  buggy; a NULL check that is genuinely part of a contract is not "a symptom
  guard" unless the writer is left unbounded.
- Historical: the three registered incidents (Gemini CLI parser 2025,
  claude-code#78133, mropert 2026) must be recognizable in the rules —
  merry-go-round (rule 4), false fix (rule 6), wrong-scope rewrite (rule 7).
- Adversarial: a variant that hides the same overflow behind a different
  consumer (e.g. reading `acc[3].balance` instead of `owner`) must still be
  diagnosed to `ingest` by the same field-inspection method.
- Commands and recorded results: `evals/README.md`.
