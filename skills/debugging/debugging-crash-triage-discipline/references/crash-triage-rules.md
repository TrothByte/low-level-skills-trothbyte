# Crash Triage Discipline — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE
→ VERIFICATION → SOURCE. Source ids refer to registry/sources.yaml.

## 1. The crash site is a symptom, not the root cause

- **RULE**: a faulting instruction only proves that some earlier operation
  produced the state it dereferenced. For memory corruption (heap OOB, UAF,
  uninitialized data) the write that broke the state and the read that faults
  are usually in different functions, often in different modules. Fixing what
  is at the top of the backtrace is fixing the symptom.
- **WHY AI GETS IT WRONG**: the backtrace names one function, so the agent
  edits that function ("strlen got NULL — make callers check NULL"); it does
  not ask *why the field is NULL*, which requires reading the producer of the
  data, not the consumer.
- **CORRECT REASONING**: treat the crash frame as evidence about state, then
  walk ownership of the faulting value: who wrote the pointer, what was the
  allocation size, what writes are not bounds-checked. In `crash_site.c` the
  fault is in `strlen` called from `print_owner`, but the corrupting write is
  `ingest()` copying 48 bytes into an 8-byte field of a neighbor record in the
  same heap block. The backtrace tells you *where it died*, not *what broke*.
- **EXAMPLE** (bad): seeing
  `#1 print_owner (owner=0x0)` and "fixing" it by adding `if (owner == NULL)
  return;` in `print_owner`. The program stops crashing, the overflow remains.
- **COUNTEREXAMPLE** (good): keep the crash, then inspect the neighbor state in
  gdb: `p/x acc[2].name`, `p/x acc[2].owner` show `0x41414141…` — bytes of the
  48-byte payload spilled over. That points at `ingest()`, the actual defect.
- **VERIFICATION**: `gdb -batch -ex run -ex bt --args prog.exe`; then
  `-ex "frame 3"` and print the struct fields around the corrupted record.
  Recorded (2026-08-15, gdb 17.2): `$3 = 0x4141414141414141` for the field
  adjacent to the overflow, `$4 = 0x0` for the field read at the crash site.
- **SOURCE**: gdb-manual; empirical (mropert 2026).

## 2. Reproduce reliably and capture the EXACT crash before touching code

- **RULE**: before any edit, produce a crash you can repeat at will and record
  the backtrace plus registers (for the faulting frame: `rip`, faulting
  address, the offending pointer value). A fix verified against a
  non-reproducing crash is a guess.
- **WHY AI GETS IT WRONG**: it edits based on a single captured stack that may
  be stale or from a different input, then cannot tell whether the change
  helped because the crash was never reproduced on demand.
- **CORRECT REASONING**: one reproducible case + exact state = a baseline.
  Every subsequent change is tested against that baseline. `-g -O0` keeps
  line numbers and locals; capture registers for the faulting frame so a
  later "fixed but different crash" is distinguishable.
- **EXAMPLE** (bad): looking at a production minidump once, patching the top
  frame, and marking the incident closed without being able to rerun the
  input.
- **COUNTEREXAMPLE** (good): build with `gcc -g -O0`, run under
  `gdb -batch -ex run -ex bt -ex "info registers rip"` and keep that output in
  the ticket before changing anything.
- **VERIFICATION**: run the same `gdb -batch` command twice; identical signal,
  identical frames. For `crash_site.exe`: both runs stop in
  `ucrtbase!strlen` with `owner=0x0`.
- **SOURCE**: gdb-manual.

## 3. Minimize the input before reasoning about the root cause

- **RULE**: reduce the failing input to the smallest case that still crashes,
  and reduce the code path by eliminating unrelated modules (stub the
  network/UI parts, drive the crashing function directly). A small case has
  fewer candidate producers, so hypothesis space shrinks.
- **WHY AI GETS IT WRONG**: it diagnoses the full production input, in which
  dozens of writes could have corrupted the field, so any hypothesis is
  plausible and the agent bounces between them.
- **CORRECT REASONING**: the smaller the input, the fewer writes touch the
  faulting memory. In `crash_site.c` the minimal case is: allocate 4 records,
  call `ingest` once with a 48-byte buffer, read `acc[3].owner`. Every other
  call is provably irrelevant, which leaves exactly one unbounded `memcpy`.
- **EXAMPLE** (bad): debugging the full 10 MB packet stream and swapping
  between hypotheses about the decoder, the hash table, and the pool
  allocator.
- **COUNTEREXAMPLE** (good): extract the 40-line reproducer; the bug is
  visible in a single `memcpy` whose `n` is not bounded by `sizeof a->name`.
- **VERIFICATION**: deleting any other `seed()`/`show()` call from
  `crash_site.c` does not change the crash; deleting `ingest()` makes it
  vanish. That is the minimal invariant.
- **SOURCE**: empirical (Gemini CLI parser 2025).

## 4. One-variable changes; break the hypothesis merry-go-round

- **RULE**: change one thing per iteration and re-run the same reproducer
  before and after. Fixing A, observing B broken, fixing B, observing A broken
  again is the classic merry-go-round: it happens when each "fix" is itself a
  new untested hypothesis layered onto the old state.
- **WHY AI GETS IT WRONG**: it batch-applies several candidate edits, then
  attributes the new failure to whichever variable it last touched; state is
  never snapshotted, so the two bugs are conflated.
- **CORRECT REASONING**: each experiment has exactly one independent variable.
  Record the crash baseline (rule 2), apply one change, re-run: either the
  baseline crash is gone or it is not. If a new failure appears, it is a
  separate variable — isolate it before declaring the first bug fixed.
- **EXAMPLE** (bad): the agent changes the buffer size, the loop bound, and
  the free order in one diff; the next crash looks "different" and it reverts
  the wrong one, ping-ponging between the two defects.
- **COUNTEREXAMPLE** (good): one diff = one bounds check on `ingest`; re-run;
  the `strlen` crash is gone and the total is correct. Only then is the second
  concern (e.g. error reporting) touched, as a second, separate change.
- **VERIFICATION**: `git diff` shows a single hunk per experiment; the
  reproducer result changes exactly once per experiment.
- **SOURCE**: empirical (Gemini CLI parser 2025).

## 5. Classify the fault before fixing: local fault vs. corruption that surfaces later

- **RULE**: decide which of the crash patterns you are in: (a) the faulting
  read itself is the bug (bad logic, real NULL from a failed check that must be
  propagated); (b) the state was valid earlier and corrupted later — heap OOB,
  use-after-free, uninitialized data, buffer over-read. The fix differs:
  (a) is fixed at the crash site, (b) at the write site.
- **WHY AI GETS IT WRONG**: it treats every NULL deref as "add a NULL check",
  which converts (b) into silent corruption (rule 6) and never fixes the
  writer.
- **CORRECT REASONING**: interrogate the value at the crash: is the pointer
  NULL, a stale freed address, or ASCII/garbage bytes? Garbage bytes that look
  like payload ('A' = 0x41) are the signature of an overflow that copied data
  over the field; a pointer into a freed block is a UAF; a NULL that no caller
  path can legitimately produce is a candidate for uninitialized data. ASan
  detects the write site directly — on hosts where it is unavailable (this
  MinGW toolchain cannot link `-lasan`), the field-byte inspection above is
  the substitute.
- **EXAMPLE** (bad): `owner=0x0` → "caller forgot to check NULL" → guard in
  `print_owner`.
- **COUNTEREXAMPLE** (good): `p/x acc[3].owner` = `0x0` AND `p/x acc[3].name`
  = all `0x41` AND `p/x acc[2].balance` = `0x41414141` → payload bytes leaked
  across the record boundary → unbounded write, not a missing check.
- **VERIFICATION**: run `gcc -fsanitize=address` if the toolchain provides it;
  here it fails with `ld: cannot find -lasan`, so verify by gdb field dumps
  and by removing the corrupting write (the crash disappears with the write,
  not with the guard).
- **SOURCE**: gdb-manual; empirical (claude-code#78133).

## 6. "Fixed" means root cause removed and regression tests pass, not "stops crashing"

- **RULE**: a verdict of "fixed" requires (1) the original crash is gone under
  the same reproducer, (2) no new crash under a broader run, (3) output
  correctness checks pass. Suppressing the symptom without removing the cause
  is a false positive fix.
- **WHY AI GETS IT WRONG**: it uses "program now exits 0" as the success
  criterion. `symptom_guard.c` demonstrates why that fails: the NULL-owner
  guard silences the crash, the program exits 0, but `settle()` sums a balance
  that was overwritten with `0x4141414141414141` and reports
  `total: -2105375976` instead of 185.
- **CORRECT REASONING**: the corruption still exists; only its observable
  crash was removed. A correct fix removes the *write* (bounds check in
  `ingest`), after which the guard is dead code and the total is correct.
- **EXAMPLE** (bad): `symptom_guard.c` — exit 0, total `-2105375976`, reported
  as resolved in a commit message.
- **COUNTEREXAMPLE** (good): `disciplined.c` — exit 0, total `185`, and an
  explicit `if (total != 185) return 1;` regression assertion.
- **VERIFICATION**: run all three binaries on the same input: `crash_site.exe`
  exits `-1073741819` (0xC0000005), `symptom_guard.exe` exits 0 with a wrong
  total, `disciplined.exe` exits 0 with total 185. Only the last is "fixed".
- **SOURCE**: empirical (claude-code#78133).

## 7. Do not rewrite a whole module for a single corrupt field

- **RULE**: when one field (or one texture, one struct, one component) is
  corrupt, the minimum-change fix is at the writer of that field. Rewriting
  the surrounding system on the hypothesis that "the module is fundamentally
  broken" is expensive and usually leaves the actual overflow in place.
- **WHY AI GETS IT WRONG**: a wrong-layer diagnosis ("this subsystem is
  broken") makes the rewrite look justified; the agent also confuses *the
  object that looks corrupt* with *the code that corrupts it*.
- **CORRECT REASONING**: corrupt-looking state is produced by a write
  somewhere; find the write. In `crash_site.c` the corrupted object is the
  record array, but replacing the record array or the `show`/`print_owner`
  module would not fix `ingest`'s missing bounds check.
- **EXAMPLE** (bad): replacing an entire lighting/texture pipeline because one
  texture sampled as garbage — when the actual cause was a single unchecked
  copy of a texture header field (mropert 2026).
- **COUNTEREXAMPLE** (good): locate the writer of the corrupt field (gdb field
  inspection, then read the producing function), fix the bounds in one place,
  keep the rest of the module untouched.
- **VERIFICATION**: a `git diff --stat` of the real fix should be ~1 file / a
  handful of lines for the writer; a multi-file rewrite of the reader module
  with the writer unchanged is the failure signature.
- **SOURCE**: empirical (mropert 2026).

## Quick reference table

| Step | Discipline | Anti-pattern |
|---|---|---|
| 1. Reproduce | repeatable crash + saved backtrace/registers | edit from a stale stack |
| 2. Minimize | smallest input, stub unrelated modules | diagnose full input |
| 3. Classify | local fault vs. late-surfacing corruption | every NULL is "add a check" |
| 4. Change | one variable per experiment | batch edits, merry-go-round |
| 5. Verify | crash gone + output correct + regression | exit 0 = fixed |
| 6. Scope | fix the writer, not the reader/module | rewrite the subsystem |
