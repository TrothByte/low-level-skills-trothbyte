# Evaluation — debugging-instrumentation-over-reasoning

Skill: `skills/debugging/debugging-instrumentation-over-reasoning`.
Toolchain: GCC 16.1.0 (MSYS2 UCRT64, target x86_64-w64-mingw32, PE/COFF),
compiled with `gcc -g -O0`. gdb is NOT installed on this host — the gdb
`set logging`/`bt full` capture command is documented from `gdb-manual` and is
UNVERIFIED here; the file-log pattern it promotes is verified by the executed run.

## Synthetic evals

| Case | Fixture | Expected | Recorded |
|---|---|---|---|
| threshold/positive | `good/instrumented_trace.c` | instrumentation localizes the corruption to the call where the counter first reaches 11 | exit 0, `CORRUPTION DETECTED at iteration 11 total=12 slots[0]=0x00007271` |
| threshold/negative | `bad/wrong_object_fix.c` | wrong-object fix masks the corruption and prints PASS | exit 0, `PASS: slots[0]=0x00000000 after 20 writes` (false confidence) |
| clean/positive | `good/instrumented_trace.c` built with `-DNOBUG` | no corruption reported; all `was` equal `after` | exit 0, `no corruption observed` |

## False-positive evals (correct code must not be flagged)

- `good/instrumented_trace.c` built with `-DNOBUG`: every ENTRY/AFTER pair shows
  `was=0x00000000 after=0x00000000` — no corruption, no false flag.
- Safe calls in the buggy run (iterations 0–10, where `len+4 <= BUF_CAP` or the
  threshold is not crossed) log `was` equal to `after` — not flagged.
- Entry logging that respects capacity must not be reported as a bug.

## Historical evals

- Pypersistent 2026 — debugger and pure reasoning failed; a file-based printf trace
  found the bug. Mapped to reference rules 1 and 3; the mechanism is reproduced by
  the executed run, where only the per-call before/after file log exposes the
  threshold corruption. Status: KNOWN-as-attributed (registered source), mechanism
  reproduced.
- codemine 2026 — tail-truncated stacktraces led to repeated full runs instead of
  widening capture. Mapped to reference rule 2; the fix is widening capture to a
  file, executed as append-mode file logging. gdb-side capture is UNVERIFIED (no
  gdb). Status: KNOWN-as-attributed (registered source).
- localghost — a confident-but-wrong `Dispose()` fix addressed the wrong object.
  Mapped to reference rule 5; reproduced as `bad/wrong_object_fix.c`, which re-zeros
  the victim and prints `PASS` while the overflow continues every iteration. Status:
  KNOWN-as-attributed (registered source), reproduced.

## Adversarial evals

- A wrong-object "fix" that makes the run pass must be rejected as masking unless
  the instrumented log shows the corrupting write gone — `bad/wrong_object_fix.c`
  is the fixture (prints `PASS`, exit 0, corruption still present each iteration).
- A tail-truncated console trace must not be used to conclude the cause; the full
  file log is the arbiter (rule 2).
- Removing instrumentation at the first green run must be caught (rule 6): the
  masked bug resurfaces later with no trace.

## Verified facts (ACTUAL output, recorded 2026-08-15, GCC 16.1.0, `-g -O0`)

Compile and run commands:

```
gcc -g -O0 -Wall -Wextra examples/good/instrumented_trace.c -o trace
./trace
cat instrumentation.log
```

Recorded console output:

```
> ./trace.exe
result: corruption found
> ./wrongfix.exe
PASS: slots[0]=0x00000000 after 20 writes
> ./trace_clean.exe
result: no corruption
```

Recorded file log, buggy run in full (corruption at the exact threshold call;
a second run with `-DNOBUG` later appended 41 further lines, all
`was=0x00000000 after=0x00000000`, ending `no corruption observed`):

```
RUN begin
ENTRY copy_in len=8 total=0
AFTER copy_in len=8 total=1 slots[0]=0x00000000 was=0x00000000
ENTRY copy_in len=14 total=1
AFTER copy_in len=14 total=2 slots[0]=0x00000000 was=0x00000000
ENTRY copy_in len=8 total=2
AFTER copy_in len=8 total=3 slots[0]=0x00000000 was=0x00000000
ENTRY copy_in len=14 total=3
AFTER copy_in len=14 total=4 slots[0]=0x00000000 was=0x00000000
ENTRY copy_in len=8 total=4
AFTER copy_in len=8 total=5 slots[0]=0x00000000 was=0x00000000
ENTRY copy_in len=14 total=5
AFTER copy_in len=14 total=6 slots[0]=0x00000000 was=0x00000000
ENTRY copy_in len=8 total=6
AFTER copy_in len=8 total=7 slots[0]=0x00000000 was=0x00000000
ENTRY copy_in len=14 total=7
AFTER copy_in len=14 total=8 slots[0]=0x00000000 was=0x00000000
ENTRY copy_in len=8 total=8
AFTER copy_in len=8 total=9 slots[0]=0x00000000 was=0x00000000
ENTRY copy_in len=14 total=9
AFTER copy_in len=14 total=10 slots[0]=0x00000000 was=0x00000000
ENTRY copy_in len=8 total=10
AFTER copy_in len=8 total=11 slots[0]=0x00000000 was=0x00000000
ENTRY copy_in len=14 total=11
AFTER copy_in len=18 total=12 slots[0]=0x00007271 was=0x00000000
CORRUPTION DETECTED at iteration 11 total=12 slots[0]=0x00007271
```

Key facts proven by this run:

- The ENTRY line `len=14 total=11` records the input at the faulting call; the
  AFTER line `len=18` shows the counter-dependent off-by-four (`14+4`), and
  `slots[0]=0x00007271 was=0x00000000` is the first before/after mismatch — the
  corrupting write is uniquely identified (rule 3).
- Bytes `0x71 0x72` land on the two low bytes of the adjacent `slots[0]`
  (little-endian) when the copy loop writes past `buf[15]` — the overflow source,
  not the victim, is the fix target (rule 5).
- The `-DNOBUG` build (single-variable experiment: the `+=4` line removed) reports
  `no corruption observed` with zero mismatches — the hypothesis is verified, not
  argued (rule 4), and serves as the false-positive case.
- `bad/wrong_object_fix.c` prints `PASS` while the same overflow fires every
  iteration past the threshold — the wrong-object fix is masked, not fixed.

## Scoring (for routing eval)

- precision: every corruption report maps to a named reference rule (1–6).
- recall: all three fixture classes detected — buggy instrumented (found),
  wrong-object fix (rejected as masking), clean (not flagged).
- FP-rate: the `-DNOBUG` run produces zero flags across 20 logged calls.
