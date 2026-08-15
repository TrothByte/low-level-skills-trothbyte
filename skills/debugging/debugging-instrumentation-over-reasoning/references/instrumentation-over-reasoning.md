# Instrumentation over Reasoning — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE
→ VERIFICATION → SOURCE. Source ids are registered: gdb-manual,
`empirical (Pypersistent 2026)`, `empirical (codemine 2026)`,
`empirical (localghost)`.

## 1. When reasoning stalls, switch to deterministic instrumentation

- **RULE**: the first thing to add to a stalled debugging session is measurement,
  not another hypothesis. An append-only file log with checkpoints and counters is
  the evidence baseline; every later theory must be checked against it.
- **WHY AI GETS IT WRONG**: reasoning feels like progress — it produces a stream of
  plausible theories that are never executed, so the session makes no measured
  progress and the confidence is unfounded.
- **CORRECT REASONING**: measure first. If a debugger and pure reasoning both
  failed, the state transition that causes the bug is not visible; a per-call trace
  of entry parameters and before/after values makes that transition visible.
- **EXAMPLE** (bad): theorizing "the value must be corrupted by function X" and
  writing the fix for X without ever logging whether X ran near the corruption.
- **COUNTEREXAMPLE** (good):
  ```c
  fprintf(log, "ENTRY len=%d total=%d\n", len, total);
  /* the write under suspicion */
  fprintf(log, "AFTER slots[0]=0x%x was=0x%x\n", slots[0], before);
  fflush(log);
  ```
- **VERIFICATION**: run once and read the log; the corruption must map to one
  call's before/after pair. Executed: `examples/good/instrumented_trace.c` —
  mismatch first appears at the call with `total=11`.
- **SOURCE**: empirical (Pypersistent 2026).

## 2. Write full traces to a file; never reason from a tail-truncated console

- **RULE**: capture the full trace to a file with append mode and flush after every
  write; a truncated console tail hides the first corrupting call.
- **WHY AI GETS IT WRONG**: the console shows the last N lines, and agents treat
  that tail as the whole failure, then rerun the whole program repeatedly — same
  capture width, same truncation, no new information.
- **CORRECT REASONING**: the first bad line matters more than the last. Redirect
  full output: gdb `set logging on` writes the complete transcript; `bt full` dumps
  locals per frame; program logs use append mode so nothing is dropped and the run
  is not repeated.
- **EXAMPLE** (bad): `gdb -batch -ex run -ex bt` on a terminal, reading only the
  tail, then running again "to see more".
- **COUNTEREXAMPLE** (good):
  ```
  gdb -batch -ex "set logging file full.log" -ex "set logging on" \
       -ex run -ex "bt full" ./prog
  ```
- **VERIFICATION**: `full.log` must contain the complete backtrace and locals after
  a single run. The gdb command is documented from the manual (UNVERIFIED on this
  host — no gdb installed); the append-only file-log pattern is verified by the
  executed run, where the buggy iteration 11 is preceded by its full history.
- **SOURCE**: gdb-manual; empirical (codemine 2026).

## 3. Log input parameters at function entry and before/after each write

- **RULE**: entry logging captures the inputs that produced the corruption; a
  before/after pair at each write localizes which write changed the victim.
- **WHY AI GETS IT WRONG**: without entry logging, an intermittent bug that depends
  on an accumulated counter or on iteration N looks random, so the agent broadens
  the search instead of seeing the exact input at the faulting call.
- **CORRECT REASONING**: corruption is a function of the input at a specific call.
  Record the input (length, index, counter) at entry and the victim value before and
  after every write. The first mismatch is the corrupting write.
- **EXAMPLE** (bad):
  ```c
  for (i = 0; i < len; i++)
      buf[i] = src[i];   /* no log: which call overflowed, and with what len? */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  int before = slots[0];
  fprintf(log, "ENTRY len=%d total=%d\n", len, total);
  if (total >= 11) len += 4;
  for (i = 0; i < len; i++) buf[i] = fill(i);
  total++;
  fprintf(log, "AFTER len=%d total=%d slots[0]=0x%x was=0x%x\n", len, total, slots[0], before);
  ```
- **VERIFICATION**: the log must show the exact input length and counter at the
  first corrupting call. Executed: the buggy run logs `ENTRY len=14 total=11`
  followed by `AFTER ... slots[0]=0x00007271 was=0x00000000`.
- **SOURCE**: empirical (Pypersistent 2026).

## 4. Verify each hypothesis with a minimal experiment, not a "logical" argument

- **RULE**: a hypothesis is accepted or rejected by a minimal experiment — change
  one variable, rerun, read the log — not by how convincingly it is argued.
- **WHY AI GETS IT WRONG**: model output looks like reasoning; an untested theory
  is presented with the same confidence as a measured result, and the next theory
  is generated before the previous one is tested.
- **CORRECT REASONING**: every theory yields a prediction about the log — e.g. "if
  the off-by-N is in `copy_in`, the corrupt value appears when `len+N` exceeds the
  buffer capacity". Run, compare, keep or discard; the log is the arbiter.
- **EXAMPLE** (bad): "this must be a stack issue because the corrupt value is
  0x71" — argued, never tested.
- **COUNTEREXAMPLE** (good): "prediction: corruption appears at the first call
  where entry-total reaches 11 and entry-len is 14" — then the executed log shows
  exactly that; disabling the `+= 4` line (the one-variable change) yields
  `no corruption observed`.
- **VERIFICATION**: rerun with the single hypothesized change and diff the logs.
  Executed: `gcc -g -O0 -DNOBUG` on the same source flips the result from
  `corruption found` to `no corruption`.
- **SOURCE**: empirical (Pypersistent 2026); gdb-manual (inspect-after-step loop).

## 5. A passing run is not a fix; a fix must target the source, not the victim

- **RULE**: a fix is validated by evidence that the corrupting write no longer
  occurs, not by the symptom disappearing. The wrong object is the victim; the
  right object is the code that writes past it.
- **WHY AI GETS IT WRONG**: a wrong-object fix — e.g. `Dispose()` on the wrong
  instance, or re-zeroing a corrupted field every iteration — makes the current
  symptom vanish and the run turn green, so the fix is reported as success while
  the root cause stays and re-corrupts later.
- **CORRECT REASONING**: after the fix, re-run with instrumentation intact; the log
  must show the overflowing write gone, not the victim re-cleared. If only the
  symptom is removed, the fix is unproven.
- **EXAMPLE** (bad):
  ```c
  if (slots[0] != 0) slots[0] = 0;   /* fix the victim, not the overflow */
  printf("PASS\n");
  ```
- **COUNTEREXAMPLE** (good): fix the copy bound so no write can exceed the buffer,
  then re-run the instrumented log and confirm `was` equals `after` at every call.
- **VERIFICATION**: `examples/bad/wrong_object_fix.c` prints
  `PASS: slots[0]=0x00000000` while the corruption is masked every iteration —
  recorded; `examples/good/instrumented_trace.c` detects the corruption — recorded.
- **SOURCE**: empirical (localghost).

## 6. Keep instrumentation as the evidence baseline while narrowing

- **RULE**: the instrumented build is the baseline; theories and fixes are judged
  against it until the corrupting write is identified and fixed at its source.
- **WHY AI GETS IT WRONG**: instrumentation is removed as soon as the symptom is
  gone, so a masked bug is declared fixed and the regression resurfaces later with
  no trace left to recover.
- **CORRECT REASONING**: keep the log until the source line is fixed; then confirm
  a full clean run with instrumentation intact, and only then remove it. Counters
  and checkpoints are cheap and belong in the regression run.
- **EXAMPLE** (bad): deleting the trace code the moment `PASS` prints.
- **COUNTEREXAMPLE** (good): fixing `copy_in`, running the full instrumented pass,
  verifying zero `was`/`after` mismatches across all iterations, then stripping the
  logs and re-running to confirm the bug does not return.
- **VERIFICATION**: diff the instrumented log before and after the fix — the
  corruption line is present before and absent after. Executed on
  `examples/good/instrumented_trace.c` (buggy vs `-DNOBUG`).
- **SOURCE**: empirical (Pypersistent 2026); empirical (codemine 2026).

## Quick reference table

| Rule | One line |
|---|---|
| 1 | When reasoning stalls, add measurement before the next theory |
| 2 | Full trace to an append-only, flushed file — never a truncated console |
| 3 | Log entry inputs and before/after values at every write |
| 4 | Test each hypothesis with a one-variable experiment, not an argument |
| 5 | Fix the source that overflows, not the victim; a PASS is not proof |
| 6 | Keep instrumentation as the evidence baseline until the fix is confirmed |
