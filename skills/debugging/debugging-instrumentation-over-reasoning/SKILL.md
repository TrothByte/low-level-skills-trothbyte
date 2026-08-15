---
name: debugging-instrumentation-over-reasoning
description: Use when a bug resists a debugger and pure reasoning: repeated full runs, truncated traces, or fixes aimed at the wrong object. Replace speculation with deterministic append-only file instrumentation, entry logging, checkpoints, counters, and before/after values, and keep the log as evidence.
---

# Instrumentation over Reasoning

## When to use

- A bug survived several rounds of pure reasoning; each new theory contradicts the
  previous one and nothing was measured.
- A debugger session or stacktrace shows only the tail of the failure, and repeated
  full runs keep reproducing it without narrowing it.
- A previous "fix" was confident but wrong: it addressed a symptom or the wrong
  object and the bug came back.
- The failure is intermittent or order-dependent: a value corrupt only after a
  counter crosses a threshold, or a use-after-free only manifests on iteration N.
- You need an evidence baseline against which every hypothesis and fix can be
  tested cheaply.

## When not to use

- The crash is deterministic and immediate: single-step in gdb and inspect locals —
  faster than adding instrumentation.
- A sanitizer (ASan/UBSan) or a debugger already pinpoints the exact line — trust
  that point unless it is misleading.
- The code cannot be rebuilt or redeployed: prefer production live tracing
  (ftrace, kprobes, eBPF) — see `kernel-debugging-ftrace-kprobes-kdump`.
- A formal spec or proof tool already bounds the bug — `formal-spec-loop-invariants`
  is stronger evidence than runtime logging.
- A hot loop in production: file I/O per call distorts timing; use counters or a
  ring buffer instead.

## What the agent often gets wrong

- Keeps "reasoning" in place of measurement: produces theory after theory, none of
  them executed.
- Repeats the same full run hoping the tail reveals more, instead of widening the
  capture by writing the full trace to a file.
- Reads a tail-truncated console or stacktrace as the whole story and draws
  conclusions from the last lines only.
- Treats the debugger as the only instrument and skips cheap file-based
  entry/after logging.
- Reports a fix as success because the symptom disappeared, when the fix targeted
  the wrong object (e.g. a `Dispose()` call on the wrong instance, or re-zeroing
  the corrupted victim instead of fixing the write that overflows it).
- Intermittent corruptions are "reproduced" but never localized, because no
  before/after values are recorded at each write.
- Removes instrumentation the moment the run turns green, so a masked bug later
  resurfaces with no trace to recover.

## How to reason correctly

1. When reasoning stalls, switch to measurement: instrument deterministically
   before generating the next theory.
2. Use append-only file logging with checkpoints and counters; write the full trace
   to a file, never reason from a tail-truncated console.
3. Log input parameters at function entry and the value before and after each
   write, so every corruption maps to a specific call site and input.
4. Turn every hypothesis into a minimal experiment: change one variable, rerun,
   read the file log. A hypothesis that survives is evidence; an untested argument
   is not.
5. When a fix appears to work, re-run against the instrumented log and confirm the
   corrupting write is gone. If only the symptom is gone, the fix is unproven and
   may target the wrong object.
6. Keep the instrumented build as the evidence baseline while narrowing; remove
   instrumentation only after the root cause is fixed at the source and a clean
   run confirms it does not return.

## What to verify

- The trace file grows with every call: entry parameters, before/after values,
  checkpoint counters, and a detection line.
- The first corrupt value is preceded by an ENTRY line whose input length exceeds
  the destination capacity — the corrupting call, not a later one.
- The log is append-only and flushed after every write, so a crash does not lose
  the tail.
- The fix targets the source (the write that overflows), not the victim (the
  object that was overwritten).
- Removing instrumentation does not bring the bug back — the root cause is fixed,
  not masked.

## How to verify

```
gcc -g -O0 -Wall -Wextra examples/good/instrumented_trace.c -o trace
./trace
cat instrumentation.log
```

- The buggy run must end with `CORRUPTION DETECTED` and the log must show the
  first `was`/`after` mismatch at the threshold call.
- The clean run (`gcc -g -O0 -DNOBUG`) must end with `no corruption observed` and
  every `was` equal to `after`.
- The wrong-object fixture: `gcc -g -O0 examples/bad/wrong_object_fix.c -o wrongfix
  && ./wrongfix` prints `PASS` — the false confidence this skill rejects.
- For debugger-driven capture, redirect the full backtrace to a file with gdb
  logging: `gdb -batch -ex "set logging file full.log" -ex "set logging on" -ex
  run -ex "bt full" ./prog`.

## Where the knowledge comes from

- gdb-manual — `set logging`, `bt full`: capturing complete state to a file
  instead of the truncated terminal (KNOWN).
- empirical (Pypersistent 2026) — a debugger and pure reasoning failed; a
  file-based printf trace found the bug (KNOWN, registered source).
- empirical (codemine 2026) — tail-truncated stacktraces caused repeated full runs
  instead of widening the capture (KNOWN, registered source).
- empirical (localghost) — a confident-but-wrong `Dispose()` fix addressed the
  wrong object and the bug returned (KNOWN, registered source).
- Compiled verification run in this session — real file-log output reproduced the
  threshold corruption (KNOWN, executed).

## Related skills

- `sanitizer-report-reading` — ASan/UBSan reports when instrumentation is not needed.
- `kernel-debugging-ftrace-kprobes-kdump` — production live tracing without rebuild.
- `c-undefined-behavior` — the memory-corruption classes this skill's traces detect.
- `binary-memory-leak-vm-allocator-diagnosis` — heap-side diagnosis of leak/free bugs.
- `dwarf-debug-info` — debug info behind gdb-based capture.
- `meta-verification-harness-validity` — why a passing run is not proof of a fix.

## Evaluation

- Synthetic: a value corrupted only after a counter crosses a threshold must be
  localized to the exact call by before/after file logging.
- False-positive: a clean instrumented run must log every call and report no
  corruption (all `was` equal to `after`).
- Adversarial: a wrong-object fix that re-zeroes the victim and prints `PASS` must
  be recognized as masking, not fixing, and must be rejected unless the log shows
  the corrupting write gone.
- Historical: the Pypersistent, codemine, and localghost failure modes must map to
  the named anti-patterns above.
- Commands and recorded results: `evals/README.md`.
