# Sanitizer Report Reading — Reference

Sources: clang-docs (AddressSanitizer, UndefinedBehaviorSanitizer, ThreadSanitizer,
MemorySanitizer, LeakSanitizer pages); gcc-manual; cwe; cert-c; iso-c11-n1570.
Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE →
VERIFICATION → SOURCE. Example reports are hand-written fixtures in `examples/reports/`.

## 1. The header line defines the category

- **RULE**: the first lines of any report name the tool and the category:
  `ERROR: AddressSanitizer: heap-buffer-overflow`, `WARNING: ThreadSanitizer: data race`,
  `file.c:12:5: runtime error: left shift of negative value`, or
  `WARNING: MemorySanitizer: use-of-uninitialized-value`. The category selects the fix class.
- **WHY AI GETS IT WRONG**: jumps straight to the first stack frame and guesses from code
  instead of reading the header.
- **CORRECT REASONING**: `heap-buffer-overflow` and `stack-buffer-overflow` lead to different
  allocations and different fixes; `runtime error:` means UB, not a memory violation. Read the
  header before any frame.
- **EXAMPLE**: `==12345==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x602000000030`
  — category is heap-buffer-overflow, an ASan memory error on heap memory.
- **COUNTEREXAMPLE**: `tools/bitpack.c:19:12: runtime error: left shift of negative value -3`
  — this is UBSan; the fix is a type change (unsigned), not a bounds check.
- **VERIFICATION**: for all five fixtures, state the exact category from the header before
  touching the stacks; compare with `examples/good/interpretation.md`.
- **SOURCE**: clang-docs (AddressSanitizer, UndefinedBehaviorSanitizer, ThreadSanitizer,
  MemorySanitizer report formats).

## 2. ASan: access site vs allocation site

- **RULE**: the FIRST stack trace is the access site (where the bad access fired). The
  `allocated by` block is the allocation site (where the memory came from, including size).
  The root cause lives in the code between the two — wrong allocation size, or an index/bound
  error on the way to the access.
- **WHY AI GETS IT WRONG**: "fix the top frame" — the top frame only says where it fired,
  not why the memory was in that state.
- **CORRECT REASONING**: the `located ... bytes to the right/left of N-byte region [start,end)`
  line gives exact bounds; combined with the allocation stack you know the object's intended
  size and can find the index arithmetic that walked past it.
- **EXAMPLE**: `asan-heap-oob.txt` — READ at `tools/pktparse.c:42:18`, 16-byte region
  allocated at `tools/pktparse.c:87:15`; the root cause is the `i <= hdr->count` loop bound
  that produces index 16, not the malloc.
- **COUNTEREXAMPLE**: "fixing" by growing the malloc size — that just moves the poisoned
  boundary; the off-by-one remains.
- **VERIFICATION**: for each ASan fixture, name access site, allocation site, and root cause
  independently.
- **SOURCE**: clang-docs (AddressSanitizer: heap-buffer-overflow and stack-buffer-overflow);
  cert-c ARR30-C; cwe CWE-787.

## 3. ASan shadow bytes

- **RULE**: one shadow byte represents 8 application bytes. Legend: `00` addressable, `01`-`07`
  partially addressable, `fa` heap left redzone, `fd` freed heap region, `f1` stack left
  redzone, `f2` stack mid redzone, `f3` stack right redzone, `f9` global redzone, `fc`
  container overflow, `f7` poisoned by user. A `[f1]` block around an object means a stack
  object with poisoned redzones.
- **WHY AI GETS IT WRONG**: reads `f1` as a generic error marker and mislabels heap bugs as
  stack bugs.
- **CORRECT REASONING**: shadow bytes confirm WHERE memory is poisoned: addressable `00`
  flanked by `fa` = heap object, flanked by `f1`/`f3` = stack object, `fd` = freed memory.
  Use them to confirm the direction of the overflow, then read the two stacks for the cause.
- **EXAMPLE**: `asan-heap-oob.txt` shadow dump shows `00 00` (16 addressable bytes) flanked
  by `fa` (heap redzones); the READ landed on the `fa` immediately to the right.
- **COUNTEREXAMPLE**: seeing `f1` in the legend and reporting a stack-buffer-overflow for a
  heap fixture — the legend lists ALL codes; only the dump around the buggy address matters.
- **VERIFICATION**: for each fixture, determine heap-vs-stack from the shadow dump before
  reading the SUMMARY.
- **SOURCE**: clang-docs (AddressSanitizer shadow memory).

## 4. Use-after-free: the three-stack report

- **RULE**: a UAF report carries THREE traces: the access (first), `freed by` (where the
  object was freed), and `previously allocated by` (where it was born). The root cause is the
  lifetime bug connecting them — usually visible in the `freed by` stack (free while users
  still hold references).
- **WHY AI GETS IT WRONG**: fixes the access site or reorders memory writes, ignoring who
  freed and why.
- **CORRECT REASONING**: the object was freed at the `freed by` site while a later user
  (timer callback, another thread) still holds a raw pointer. The fix is lifetime/ownership,
  e.g. a refcount or cancelled callback.
- **EXAMPLE**: `asan-uaf.txt` — access `tools/connserver.c:95:11`, freed by `conn_close`
  `tools/connserver.c:210:5` reached from `handle_timeout`; the timer callback kept a
  dangling pointer.
- **COUNTEREXAMPLE**: "make `conn_poll` check a flag" — reading the flag through the same
  dangling pointer is itself a UAF.
- **VERIFICATION**: from the `freed by` and access stacks, name who freed and who should have
  held the last reference.
- **SOURCE**: clang-docs (AddressSanitizer heap-use-after-free); cert-c MEM30-C; cwe CWE-416.

## 5. Double-free and invalid free

- **RULE**: `attempting double-free` reports the second free site; `attempting free on address
  which was not malloc()-ed` reports an invalid free of a stack/global/static pointer. Both
  are lifetime bugs located at the free, not at any access.
- **WHY AI GETS IT WRONG**: searches for a bad read; there is none — the second free is the bug.
- **CORRECT REASONING**: trace the ownership: two owners freeing once each (aliased pointers),
  or a pointer that never came from the heap. Fix by making ownership single or tracking
  freed state.
- **EXAMPLE**: `free(p); free(q);` where `q` aliases `p` — ASan flags the second free.
- **COUNTEREXAMPLE**: "null out after free" — NULLing one alias does not stop the other.
- **VERIFICATION**: `-fsanitize=address`; the report names the exact free line.
- **SOURCE**: clang-docs (AddressSanitizer double-free); cert-c MEM30-C, MEM31-C;
  cwe CWE-415.

## 6. UBSan: single-line reports and `-fsanitize-recover`

- **RULE**: UBSan prints `file:line:col: runtime error: <check>` and, unless
  `-fno-sanitize-recover`, CONTINUES executing. With recover on, one run can print many
  errors and still exit 0; with recover off it aborts at the first error.
- **WHY AI GETS IT WRONG**: "UBSan passed, exit 0" when recover-mode printed dozens of
  `runtime error:` lines; or fixes only the first reported line.
- **CORRECT REASONING**: decide the mode first. To gate the build on UB use
  `-fno-sanitize-recover=undefined` (fatal); to enumerate all UB keep recover on and count
  distinct `(check, file, line)`. Exit code alone is meaningless in recover mode.
- **EXAMPLE**: `ubsan-shift.txt` — `tools/bitpack.c:19:12: runtime error: left shift of
  negative value -3`: signed `flags << n` is UB (C11 6.5.7p3-4); fix is casting to unsigned.
- **COUNTEREXAMPLE**: "fixing" by masking the shifted result — the UB is in the operation itself.
- **VERIFICATION**: re-run with `-fno-sanitize-recover=undefined`; the error becomes fatal.
- **SOURCE**: clang-docs (UndefinedBehaviorSanitizer, `-fsanitize-recover`); gcc-manual
  (`-fsanitize-recover`); iso-c11-n1570 6.5.7.

## 7. TSan: a race is defined by TWO stacks

- **RULE**: `WARNING: ThreadSanitizer: data race` shows a READ/WRITE access and a `Previous`
  access from another thread, plus the object and thread-creation context. Both traces are
  one bug; the fix is synchronization (mutex/atomics/happens-before), not a change in one thread.
- **WHY AI GETS IT WRONG**: "fix thread 1" — the race is the pair of unsynchronized accesses.
- **CORRECT REASONING**: two accesses to one memory location, at least one a write, with no
  ordering edge between them. Adding a lock around both, or an atomic, creates the missing
  happens-before edge; a fence on one side only is insufficient.
- **EXAMPLE**: `tsan-race.txt` — T1 reads `stats->processed` at `tools/jobsched.c:33:17`,
  T0 wrote it at `tools/jobsched.c:58:9`; shared heap block from `stats_new`
  (`tools/jobsched.c:12:9`), no lock.
- **COUNTEREXAMPLE**: adding a barrier in `worker_report` only — the scheduler write stays
  unsynchronized and TSan still reports the race.
- **VERIFICATION**: re-run `-fsanitize=thread` — report gone. A parallel ASan run stays
  clean because ASan cannot see races.
- **SOURCE**: clang-docs (ThreadSanitizer); cwe CWE-362; memory-ordering-reasoning skill.

## 8. MSan: use-of-uninitialized-value and origin tracking

- **RULE**: MSan marks reads of poisoned (never-written) bytes. The `Uninitialized value was
  created by` block is the origin: the malloc that returned uninitialized heap, or the frame
  whose local was never written. Track origins with `-fsanitize-memory-track-origins=2`.
- **WHY AI GETS IT WRONG**: blames the read site (e.g. a checksum function) — the read is
  correct; the missing write is the bug.
- **CORRECT REASONING**: memory was allocated but a field was never initialized; the origin
  block names the allocation or the stack frame. Fix by initializing at the origin.
- **EXAMPLE**: `msan-uninit.txt` — `checksum` reads `pkt->hdr.seq` at `tools/pktcheck.c:25:16`;
  origin is the heap allocation in `alloc_packet` (`tools/pktcheck.c:40:16`) that did not
  zero the struct.
- **COUNTEREXAMPLE**: "fixing" checksum to skip `seq` — the packet content is undefined, not
  merely unused.
- **VERIFICATION**: MSan requires ALL code (including libraries) to be MSan-instrumented;
  a non-instrumented libc yields false negatives. Run with
  `-fsanitize-memory-track-origins=2`.
- **SOURCE**: clang-docs (MemorySanitizer, origin tracking); cert-c EXP33-C; cwe CWE-457.

## 9. LeakSanitizer: exit-time reporting, dedup by allocation site

- **RULE**: LSan (part of ASan) reports leaks at process exit:
  `ERROR: LeakSanitizer: detected memory leaks`, then `Direct leak of N byte(s) in K object(s)
  allocated from:` blocks. Deduplicate by allocation site, not by address or count.
- **WHY AI GETS IT WRONG**: treats each block as a separate bug, or dismisses leaks in
  long-running tools as harmless.
- **CORRECT REASONING**: a leak is an ownership bug: something that should free never did.
  Leaks reached only from globals may be intentional singletons; report the direct/indirect
  leaks that grow without bound.
- **EXAMPLE**: `Direct leak of 4096 byte(s) in 1 object(s) allocated from: #1 in alloc_table
  src/tab.c:33` — the table is never freed at shutdown.
- **COUNTEREXAMPLE**: "it exits anyway, so it does not matter" — in a loop the allocation
  repeats and memory grows linearly.
- **VERIFICATION**: `ASAN_OPTIONS=detect_leaks=1 ./prog`; after adding the free, the block
  disappears.
- **SOURCE**: clang-docs (LeakSanitizer).

## 10. Real bug vs tool artifact

- **RULE**: a report is a real bug unless a documented exception applies: intentional
  redzone/poisoning tests, `__attribute__((no_sanitize(...)))` regions, custom allocators
  that re-poison memory, or library/interceptor frames masquerading as the bug site. Most
  reports are real.
- **WHY AI GETS IT WRONG**: dismisses reports as "tool false positive" without evidence, or
  "fixes" intentional test code.
- **CORRECT REASONING**: before calling a report a false positive, check for explicit
  poisoning, `no_sanitize` attributes, and `ASAN_OPTIONS` overrides; also skip
  `__interceptor_*` and `__libc_start_main` frames — they are plumbing, not bug sites.
- **EXAMPLE**: a unit test that deliberately writes past its buffer to prove the redzone
  fires — the report is expected and benign; do NOT "fix" the deliberate OOB.
- **COUNTEREXAMPLE**: an "intentional overflow test" that actually ships in a production
  data path — still a real bug.
- **VERIFICATION**: reproduce with `-O0 -fno-omit-frame-pointer`; if the report persists
  without the special build, it is real.
- **SOURCE**: clang-docs (AddressSanitizer FAQ, poisoning API); cert-c ARR30-C.

## 11. Deduplication by (category, file, line)

- **RULE**: one root cause fires once per input; deduplicate by (category, access file,
  access line) — and for UAF, group by the free site too. Report distinct findings, not raw
  lines. Priority: null-deref/double-free first, then UAF, then OOB, then leaks.
- **WHY AI GETS IT WRONG**: pastes 50 identical reports and claims 50 bugs, or deduplicates
  only by exact text.
- **CORRECT REASONING**: the finding count is the number of distinct (category, file, line)
  triples; the same off-by-one may fire from many lines after inlining — group by root cause.
- **EXAMPLE**: `asan-heap-oob` fires on 40 test inputs — one finding at
  `tools/pktparse.c:42:18`.
- **COUNTEREXAMPLE**: fixing "the report at line 42" while the same off-by-one also fires at
  line 60 through inlining — both must map to the same loop-bound fix.
- **VERIFICATION**: `python tools/eval/sanitizer_parse.py <report>` emits deduplicated
  findings; distinct count == finding count.
- **SOURCE**: sanitizer-agent-ci-loop skill (dedup rule); clang-docs.

## 12. False negatives: what each sanitizer cannot see

- **RULE**: ASan sees dynamic OOB/UAF but NOT: reads of struct padding (padding between
  fields is unpoisoned), races (TSan's job), UB (UBSan's job), or uninitialized reads (MSan's
  job). A clean run of one sanitizer is not a safety proof.
- **WHY AI GETS IT WRONG**: equates "ASan clean" with "correct", especially for threaded code.
- **CORRECT REASONING**: each tool covers one slice of the failure space; choose the tool for
  the suspected class and combine them in the CI loop. A report the wrong tool misses is a
  false negative, not evidence.
- **EXAMPLE**: a data race passes ASan (no memory violation) and is only visible under TSan;
  an uninit read of a fully-sized buffer passes ASan and needs MSan.
- **COUNTEREXAMPLE**: "ASan and UBSan are clean, ship it" for a multithreaded program.
- **VERIFICATION**: run the same concurrency fixture under ASan and under TSan; the race
  appears only under TSan.
- **SOURCE**: clang-docs (limitations sections of each sanitizer); memory-ordering-reasoning
  skill.

## Quick recognition table

| Header / line | Category | Tool |
|---|---|---|
| `ERROR: AddressSanitizer: heap-buffer-overflow` | OOB on heap | ASan |
| `ERROR: AddressSanitizer: stack-buffer-overflow` | OOB on stack | ASan |
| `ERROR: AddressSanitizer: heap-use-after-free` | lifetime (use after free) | ASan |
| `ERROR: AddressSanitizer: attempting double-free` | lifetime (second free) | ASan |
| `ERROR: LeakSanitizer: detected memory leaks` | ownership (leak) | LSan |
| `file.c:N:C: runtime error: <check>` | UB | UBSan |
| `WARNING: ThreadSanitizer: data race` | race | TSan |
| `WARNING: MemorySanitizer: use-of-uninitialized-value` | uninit read | MSan |
