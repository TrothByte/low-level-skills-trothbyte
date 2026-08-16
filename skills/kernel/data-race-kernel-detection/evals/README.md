# Evaluation — data-race-kernel-detection

Skill: `skills/kernel/data-race-kernel-detection`. Stability target:
`evaluated`. Current stability: `source-backed` for the host-side logic —
the Python model and C pthread fixtures below were compiled and run on this
host (gcc 16.1.0, python 3.11.9) and outputs recorded. Kernel KCSAN runs
are documented-as-target, NOT executed here.

## Synthetic evals

| Case | Fixture | Expected | Recorded |
|---|---|---|---|
| easy/negative | `bad/race_plain_counter.c` | non-atomic RMW data race | lost updates in 7/8 runs (see commands) |
| medium/negative | `bad/kcsan_masking.py` | data_race() with no rationale | prints masked "KCSAN clean", exit 0 |
| medium/positive | `good/kcsan_model.py` | plain-plain reported; marked/marked not | prints both report shapes, exit 0 |
| hard/negative | flag+payload fixed with READ_ONCE only | stale payload on some schedule | `race_fix_demo.py`: marking leaves races |
| hard/positive | flag+payload fixed with release/acquire | no stale payload on any schedule | `race_fix_demo.py`: release/acquire closes |
| hard/positive | `good/race_free_counter.c` | mutex-protected, count exact | exit 0, count == expected |

Detection rule: for each fixture, the agent must classify accesses (plain vs
marked), decide intentionality, and pick the fix that matches the protocol
(marking / lock / release-acquire / annotated data_race with reason).

## False-positive evals (correct code must NOT be flagged)

- `good/race_free_counter.c`: mutex-protected increment, exact count, no
  conflicting plain accesses — no flag.
- `good/kcsan_model.py` case2: a marked/marked conflict is correctly *not*
  reported as a data race (it IS an exclusivity violation, handled by the
  second pass) — no flag.
- A statistics counter annotated with `data_race()` plus an explicit
  "benign: best-effort diagnostic" comment — acceptable, no flag.

## Historical evals

- CVE-2016-5195 (Dirty COW): check-then-act TOCTOU that is NOT a plain
  access data race — KCSAN stays silent; the fix re-checks under the
  page-table lock. Shape classified in race-classes-kernel.md rule 4. KNOWN.
- KCSAN-found races in the Linux tree (networking/fs reports where fixes
  were READ_ONCE, locks, or release/acquire): mechanism reproduced by
  `good/kcsan_model.py` and `good/race_fix_demo.py`. KNOWN abstract;
  specific incident list UNVERIFIED on this host.

## Adversarial evals

- `bad/kcsan_masking.py` runs and prints "KCSAN clean" — the agent must
  refuse the masking and demand a benign-race rationale for each report.
- `bad/race_plain_counter.c` demonstrates the race observably (volatile
  non-atomic RMW; lost updates in 7/8 runs on this host) — but the race is
  the UB in the source, not the observed counter; an agent that reads the
  exit code as the verdict misses that the race exists even in the 1/8 run
  that happened to sum correctly.
- `race_fix_demo.py` is the flip side: marking-only "fix" passes a naive
  reviewer but fails the schedule enumeration.

## Verification commands (host, ACTUAL)

```
python examples/good/kcsan_model.py
  case1: BUG: KCSAN: data-race in writer+0x1d / reader+0x10
  case2: no data race, but ASSERT_EXCLUSIVE_WRITER violated by writer_b+0x0
  GOOD: ...                                                     exit 0
python examples/good/race_fix_demo.py
  READ_ONCE/WRITE_ONCE: <n>/<n> schedules see a stale payload
  release/acquire:      0/<n> schedules see a stale payload
  GOOD: ...                                                     exit 0
python examples/bad/kcsan_masking.py
  prints "KCSAN clean: all reports annotated data_race()" + BAD line
                                                                exit 0 (MASKED)
gcc -Wall -Wextra -Werror -O2 -pthread examples/good/race_free_counter.c -o /tmp/c1.exe
  exit 0
/tmp/c1.exe
  GOOD: count=400000 == expected=400000, no lost updates         exit 0
gcc -Wall -Wextra -Werror -O2 -pthread examples/bad/race_plain_counter.c -o /tmp/c2.exe
  exit 0
/tmp/c2.exe   (run 8 times)
  count < 400000 on 7 of 8 runs -> exit 2 "lost updates observed -> data
  race is real"; the remaining run printed count=400000 with the
  "the data race is still present (UB)" note.
  The exit code is NOT the verdict: the source-level data race exists
  in every run.
```

## Verification commands (target, RESEARCHED — not run on this host)

```
scripts/config -e KCSAN -e KCSAN_STRICT -e KCSAN_WEAK_MEMORY \
               -e KCSAN_REPORT_RACE_UNKNOWN_ORIGIN -e KASAN
make -j$(nproc)
qemu-system-x86_64 -kernel arch/x86/boot/bzImage -append "console=ttyS0 kcsan.udelay_task=100"
# exercise the reviewed path; grep dmesg for "BUG: KCSAN: data-race"
```

## Verified facts

- All five host fixtures ran and produced the recorded outputs above (KNOWN).
- The plain `count++` RMW is a data race by the LKMM definition — KNOWN
  (kernel-kcsan-docs definition; matches the model output).
- The non-atomic RMW counter lost updates in 7/8 runs on this host (x86
  TSO; gcc -O2) — KNOWN, recorded. The 1/8 lucky run is the "passes on my
  machine" trap: the race persists as UB in every run.
- KCSAN sampling, strict/permissive configs, weak-memory modeling,
  ASSERT_EXCLUSIVE_* family, unknown-origin reports — KNOWN from the KCSAN
  documentation (fetched 2026-08-17), cited to proposed source
  `kernel-kcsan-docs`.
- Target KCSAN boot behavior (actual splat output on a real kernel) —
  UNVERIFIED on this host.

## Scoring

- precision: every flagged pattern must map to a concrete KCSAN-class
  finding (plain-plain conflict, unknown origin, masking without rationale,
  wrong-fix-for-protocol).
- recall: plain/marked classification, intentionality, fix-protocol match,
  and strict-config verification are each demanded.
- FP-rate: `good/race_free_counter.c`, the marked/marked case, and the
  annotated-benign counter produce zero flags.
- Strongest single fact: `race_fix_demo.py` records N stale-payload
  schedules for the READ_ONCE "fix" and 0 for release/acquire — the
  fix-selection delta is measured, not assumed.
