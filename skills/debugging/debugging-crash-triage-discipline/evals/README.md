# Evaluation — debugging-crash-triage-discipline

Skill: `skills/debugging/debugging-crash-triage-discipline`.
Stability target: `evaluated`. Toolchain (recorded 2026-08-15):
gcc 16.1.0 (MSYS2 MinGW, x86_64-w64-mingw32, UCRT), gdb 17.2, Windows x64.
clang: NOT INSTALLED. `gcc -fsanitize=address` does NOT link on this host
(`ld: cannot find -lasan`) — verified below — so crash analysis uses gdb
manual field inspection.

## Synthetic evals

| Case | Fixture | Expected | Recorded |
|---|---|---|---|
| easy/negative | `bad/crash_site.c` | reproducible SIGSEGV | exit -1073741819 (0xC0000005) |
| easy/negative | `bad/crash_site.c` | backtrace top frame is the misleading consumer, not the writer | bt shows `ucrtbase!strlen` ← `print_owner` ← `show` ← `main` |
| medium/negative | `bad/crash_site.c` | root cause found only by field inspection | `acc[2].balance`/`owner` clobbered with `0x41`, `acc[3].owner` = 0 |
| medium/negative | `bad/symptom_guard.c` | exits 0 but corrupts data (false "fixed") | exit 0, `total: -2105375976` (expected 185) |
| hard/negative | `bad/crash_site.c` | corrupting write is `ingest()`, not anything in the bt | removing the `ingest` call removes the crash; guarding NULL does not fix the total |

## False-positive evals (correct code must not be flagged)

- `good/disciplined.c` — exits 0, prints the rejection message, `total: 185`,
  and a `total != 185` assertion would fail if corruption remained. Must NOT
  be flagged as buggy.
- A NULL check that is part of a real contract (e.g. `if (p)`) is not a
  symptom guard; flag only when the writer is left unbounded.

## Historical evals (registered incidents must map to rules)

- `empirical (Gemini CLI parser 2025)` — hypothesis merry-go-round: fixing A
  breaks B and vice versa → reference rule 4 (one-variable changes).
- `empirical (claude-code#78133)` — wrong-layer diagnosis + false "fixed"
  verdict while the root cause remained → reference rule 6.
- `empirical (mropert 2026)` — one corrupt texture triggered a full lighting
  subsystem rewrite → reference rule 7.

## Adversarial evals

- Hide the same overflow behind a different consumer: read `acc[3].balance`
  instead of `acc[3].owner` in `show`. The crash moves, the field-inspection
  signature (`0x41414141` across the record boundary) still names `ingest`.
- Give the bad input a plausible non-crashing cover (e.g. payload that makes
  the corrupted pointer look like a legitimate heap address) — the classifier
  in rule 5 must still compare bytes against the producing write, not the
  pointer's plausibility.

## Verified facts (ALL actual recorded output, 2026-08-15)

ASan on this toolchain:

```
gcc -fsanitize=address -g -O0 crash.c -o crash.exe
  .../x86_64-w64-mingw32/bin/ld.exe: cannot find -lasan: No such file or directory
  collect2.exe: error: ld returned 1 exit status
```

So ASan is UNVERIFIED/UNAVAILABLE here; clang is not installed. gdb manual
inspection is the verification method.

`gdb -batch -ex "set pagination off" -ex run -ex bt -ex "info registers rip rsp rdi rsi rdx" --args ./evals/crash_site.exe`:

```
[New Thread 24752.0x4028]
[New Thread 24752.0x2fa4]
[New Thread 24752.0x4bd8]
name:
owner (5): alice

Thread 1 received signal SIGSEGV, Segmentation fault.
0x00007ffe97e6d921 in ucrtbase!strlen () from C:\WINDOWS\System32\ucrtbase.dll
#0  0x00007ffe97e6d921 in ucrtbase!strlen () from C:\WINDOWS\System32\ucrtbase.dll
#1  0x00007ff6d2ab1566 in print_owner (owner=0x0) at examples\bad\crash_site.c:43
#2  0x00007ff6d2ab15ba in show (a=0x69e308) at examples\bad\crash_site.c:50
#3  0x00007ff6d2ab16cc in main () at examples\bad\crash_site.c:71
rip            0x7ffe97e6d921      0x7ffe97e6d921 <ucrtbase!strlen+49>
rsp            0x5ffda8            0x5ffda8
rdi            0x79                121
rsi            0x1                 1
rdx            0x10000             65536
```

(Thread/PID numbers and instruction addresses vary per run due to ASLR; the
signal, function names, line numbers, and register values are stable across
repeats.)

The top frame (`ucrtbase!strlen`) is opaque CRT code; the first named frame
`print_owner (owner=0x0)` is the consumer. Nothing in the backtrace names the
writer. This is the misleading crash site the skill trains against.

Field inspection at the fault (`gdb -batch ... -ex run -ex bt -ex "frame 3" -ex "p/x acc[2].name" -ex "p/x acc[2].balance" -ex "p/x acc[2].owner" -ex "p/x acc[3].owner" -ex "p/x acc[3].name" --args ./evals/crash_site.exe`):

```
#3  0x00007ff6d2ab16cc in main () at examples\bad\crash_site.c:71
71	    show(&acc[3]);
$1 = {0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41}
$2 = 0x41414141
$3 = 0x4141414141414141
$4 = 0x0
$5 = {0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41}
```

Payload bytes (`0x41` = 'A') sit in `acc[2].balance` and `acc[2].owner`, and
`acc[3].owner` is NULL. The 48-byte `ingest` copy wrote past `acc[2].name`
into both neighbor records — the root cause is `ingest()`'s missing bounds
check, which no frame of the backtrace mentions.

Binary-level runs:

```
evals/crash_site.exe     -> name: / owner (5): alice / exit -1073741819
evals/symptom_guard.exe  -> name: / owner: alice / total: -2105375976 / exit 0
evals/disciplined.exe    -> ingest: rejected 48-byte payload (field is 8 bytes)
                            name: / owner: alice / name: / owner: dave
                            total: 185 / exit 0
```

`-1073741819` == `0xC0000005` (STATUS_ACCESS_VIOLATION). `symptom_guard`
exiting 0 with a corrupted total is the false "fixed" verdict; `disciplined`
is the correct outcome. Reproduction is deterministic: all runs were repeated
with identical signals and frames (gdb 17.2, gcc 16.1.0, -g -O0).

## Scoring (for routing eval)

- precision: every flagged case maps to a named reference rule (1-7).
- recall: crash, false-fix, and correct-fix fixtures are all detected; the
  root-cause-only field-inspection step is mandatory.
- FP-rate: `good/disciplined.c` and contract NULL checks produce zero flags.
