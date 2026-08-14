# Evaluation — sanitizer-report-reading

Skill: `skills/sanitizers/sanitizer-report-reading`. Stability target: `evaluated`.

## Eval protocol

Each case gives a report excerpt (or a fixture from `examples/reports/`) and asks the agent
to state:
1. Category — the exact sanitizer header, e.g. `heap-buffer-overflow`, `data race`,
   `left shift of negative value`, `use-of-uninitialized-value`.
2. `file:line` of the access site AND of the allocation/free site (where present).
3. Root cause — one sentence naming the defect: off-by-one, missing refcount, missing
   synchronization, uninitialized field, signed shift.
4. Fix — targets the root cause, not the access line.

## Synthetic cases

- **easy/positive**: read the header of `asan-heap-oob.txt` and name the category and the
  access site `tools/pktparse.c:42`. Must not confuse heap with stack.
- **easy/negative**: `ubsan-shift.txt` — recognize `runtime error:` as UBSan and give the
  signed-shift fix (unsigned cast), not a bounds check.
- **medium/negative**: `asan-uaf.txt` — the agent must name BOTH the access site
  (`connserver.c:95`) and the free site (`connserver.c:210`) and place the root cause in the
  lifetime handling (timer callback holding a raw pointer), not inside `conn_poll`.
- **medium/positive**: given `tsan-race.txt`, the agent must demand BOTH stacks; a fix that
  changes only `worker_report` (one thread) is wrong — synchronization must cover the
  scheduler write too.
- **hard/negative**: a report whose access line is misleading (access fires in an inlined
  frame): the top frame is `parse_record` but the true culprit is the allocation/loop in the
  caller; the agent must use the `allocated by` stack and the region line, not the top frame.
- **hard/positive**: `msan-uninit.txt` with origin tracking — the agent must fix the origin
  (initialization in `alloc_packet`), not the `checksum` read.
- **adversarial**: a UBSan report from a recover-enabled run that exited 0 — the agent must
  state that exit code 0 is NOT a clean signal unless `-fno-sanitize-recover` was used or no
  `runtime error:` lines were printed.

## False-positive cases (must NOT be "fixed")

- An ASan report produced by a test that intentionally overflows its buffer to prove the
  redzone works: expected and benign — do NOT "fix" the deliberate OOB.
- A `heap-use-after-free` report whose access and free stacks do not actually connect (stray
  line from another build): the agent must check that the `freed by` stack chains to the
  access before flagging.
- A report pointing at the wrong line: the access frame is an interceptor
  (`__interceptor_malloc`) or a library frame (`__libc_start_main`, `libtsan.so`) — these are
  plumbing, not bug sites, and must not be treated as the fix location.
- A UBSan `runtime error:` for a line covered by an explicit
  `__attribute__((no_sanitize(...)))` region that was instrumented anyway — verify before
  reporting.

## Verification status in this environment

- **VERIFIED here**: fixture self-consistency. Addresses, region bounds, shadow-byte counts,
  and stack cross-references in `examples/reports/*` agree with
  `examples/good/interpretation.md` (checked by review; see the consistency notes below).
- **UNVERIFIED here**: live sanitizer runs. libasan/libubsan/libtsan/libmsan are not
  installed in this environment, so the fixtures were not produced by a real sanitizer.

Self-consistency notes (checked):
- `asan-heap-oob.txt`: 16-byte region `[0x602000000020,0x602000000030)` matches two `00`
  shadow bytes; READ starts at the end boundary (`0 bytes to the right`).
- `asan-uaf.txt`: access, free, and allocation addresses all equal
  `0x6020000000c0` inside a 64-byte region `[0x6020000000c0,0x602000000100)` (64-byte
  object, consistent 8-byte read).
- `tsan-race.txt`: the read and write address `0x7b100000a180` equals the heap block start;
  both are size 4 within a 24-byte block.
- `ubsan-shift.txt`: single-line UBSan format, `file:line:col` present, recover mode noted.
- `msan-uninit.txt`: the origin block (heap allocation) chains to the caller of the use site
  through `handle_packet`; SUMMARY names the use site.

## Target verification (live run, required for `evaluated`)

Compile and run the programs whose reports the fixtures model:

```
clang -O1 -g -fsanitize=address -fno-omit-frame-pointer -o pktparse pktparse.c && ./pktparse
clang -O1 -g -fsanitize=address -fno-omit-frame-pointer -o connserver connserver.c && ./connserver
clang -O1 -g -fsanitize=thread -fno-omit-frame-pointer -o jobsched jobsched.c && ./jobsched
clang -O1 -g -fsanitize=undefined -fno-omit-frame-pointer -o bitpack bitpack.c && ./bitpack
clang -O1 -g -fsanitize=memory -fsanitize-memory-track-origins=2 -fno-omit-frame-pointer -o pktcheck pktcheck.c && ./pktcheck
# after each fix: the same command must be clean and exit 0
```

When a live run is available, replace the synthetic fixture texts with captured output and
record the revision date in this file.

## Scoring

- precision: category named exactly from the header.
- recall: access site AND allocation/free site both located.
- root-cause correctness: the fix touches the cause, not the symptom.
- FP-rate: benign/adversarial cases produce no unnecessary fix.
