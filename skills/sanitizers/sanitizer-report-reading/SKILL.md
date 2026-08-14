---
name: sanitizer-report-reading
description: Use when interpreting sanitizer output — ASan/UBSan/TSan/MSan/LSan reports from builds, CI logs, or fuzzing — to identify bug category, access site versus allocation/free site, root cause, and fix. Triggers on report headers like 'ERROR: AddressSanitizer', shadow bytes, 'data race', or 'use-of-uninitialized-value'.
---

# Sanitizer Report Reading

## When to use

- Reading any ASan/UBSan/TSan/MSan/LSan report and needing the bug category, exact
  file:line, and root cause.
- Triaging a failed sanitizer CI job: decide what to fix and where.
- Distinguishing which stack in a multi-stack report (UAF, data race) is the bug.
- Verifying that a fix removed a specific report without disabling the sanitizer.

## When not to use

- Setting up the build-and-test loop that RUNS sanitizers — use `sanitizer-agent-ci-loop`.
- Explaining why the flagged construct is UB — use `c-undefined-behavior` /
  `compiler-ub-assumptions`.
- Debugging when no sanitizer output exists yet.

## What the agent often gets wrong

- "The top frame is the bug." It is the access site; the allocation/free site is where the
  cause lives (for UAF, the `freed by` stack).
- Confusing `heap-buffer-overflow` with `stack-buffer-overflow`.
- Reading shadow bytes wrong: `[f1]`/`[f3]` are stack redzones, `fa` heap redzone, `fd`
  freed heap region.
- Fixing at the access line instead of the root cause (e.g. an off-by-one loop bound).
- Treating UBSan's single `runtime error:` line as the whole story: with
  `-fsanitize-recover` the program continues and may exit 0 despite UB.
- TSan: reading only the first stack; a race is defined by BOTH stacks.
- Claiming ASan-clean concurrency code is safe — ASan cannot see races.
- Deduplicating by report text instead of by (category, file, line).

## How to reason correctly

1. Read the header: `ERROR: AddressSanitizer: <category>` / `WARNING: ThreadSanitizer: <category>`
   / `runtime error: <check>` / `WARNING: MemorySanitizer: ...`. The category picks the fix class.
2. Read the access stack (first trace) for where it fired, then the allocation/free stack for
   why the memory is in that state.
3. For UAF: `freed by` shows who ended the lifetime — the root cause is there, not at the access.
4. Use the region line `[start,end)` for exact bounds and the shadow bytes to confirm
   overflow direction and heap-vs-stack.
5. Deduplicate by (category, file, line); triage distinct findings.
6. Confirm the report is real, not a tool artifact (custom allocator, intentional test,
   interceptor/library frames).

## What to verify

- Category named exactly from the header/SUMMARY line.
- Access site AND allocation/free site both located (file:line).
- Root cause named (off-by-one, missing refcount, missing synchronization, uninitialized
  field, signed shift).
- Fix touches the root cause, and the specific report disappears on re-run.

## How to verify

```
clang -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -o prog prog.c
./prog                  # expect the specific report
# after the fix: same command, report gone, exit 0
clang -O1 -g -fsanitize=thread -fno-omit-frame-pointer -o prog_t prog_t.c && ./prog_t
clang -O1 -g -fsanitize=memory -fsanitize-memory-track-origins=2 -fno-omit-frame-pointer -o prog_m prog_m.c && ./prog_m
```

Live sanitizer runs are the target verification. In this environment libasan/libubsan/libtsan/
libmsan are not installed, so the fixtures are verified for internal self-consistency only
(UNVERIFIED here; see `evals/README.md`).

## Where the knowledge comes from

- `clang-docs` (AddressSanitizer, UndefinedBehaviorSanitizer, ThreadSanitizer,
  MemorySanitizer, LeakSanitizer pages)
- `gcc-manual` (`-fsanitize`, `-fsanitize-recover`, instrumentation options)
- `cwe` (CWE-787/119/416/415/457/362)
- `cert-c` (ARR30-C, MEM30-C, CON43-C, EXP33-C)
- `iso-c11-n1570` (UB definitions behind the UBSan checks)

## Related skills

- `sanitizer-agent-ci-loop` (require of) — the loop this skill feeds
- `c-undefined-behavior` (recommend) — why the flagged construct is UB
- `memory-ordering-reasoning` (recommend) — happens-before semantics behind TSan reports
- `meta-verification` (recommend) — proving a "clean" run actually ran the code path

## Evaluation

Synthetic: easy (header → category), medium (two-stack UAF triage), hard (misleading
inlined access line, origin-tracking MSan), adversarial (recover-mode UBSan with exit 0
must not be read as clean). False-positives: intentional overflow test must NOT be fixed;
reports pointing at interceptor/library frames must not be treated as bug sites. Live
sanitizer runs are UNVERIFIED in this environment; full cases in `evals/README.md`.
