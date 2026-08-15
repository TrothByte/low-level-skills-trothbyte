# Evaluation — embedded-flash-debug-cycle

Skill: `skills/embedded/embedded-flash-debug-cycle`.
Stability target: `evaluated`.

## Verified facts (host, recorded 2026-08-15)

OpenOCD, arm-none-eabi-gdb, and a hardware probe are NOT available on this
host (documented honestly). The single-probe/lock/preflight logic is modeled
in runnable python and actually executed:

```
python examples/good/session_discipline.py
  "flash reported success"
  "second flash: probe free -> clean connect"
  "PASS: single-probe discipline + teardown"        (exit 0)

python examples/good/lockfile_and_preflight.py
  "preflight ok; flashed"
  "PASS: stale-lock reclaim + preflight"            (exit 0)

python examples/bad/probe_contention.py
  "flash reported success"
  "flash reported success"
  "second flash: probe held by first session -> contention"   (exit 0 — silent bug)

python examples/bad/missing_preflight.py
  "can't reliably flash (retried blindly; device was in DFU)" (exit 0 — silent bug)
```

Bad files run clean — they reproduce the contention/strand failures as
diagnostic output and must be caught by review, not by the interpreter.

NOT verified on this host (target toolchain, do NOT claim to have run):
`openocd -f interface/stlink.cfg`, `arm-none-eabi-gdb -batch ...`, real
`verify_image` on hardware, DFU preflight against a real device. Those are the
documented target commands in SKILL.md "How to verify".

## Synthetic evals

- easy/negative: `bad/probe_contention.py` — no teardown; second run contends.
- easy/negative: `bad/missing_preflight.py` — blind retry in DFU state.
- easy/positive: `good/session_discipline.py` — detach-then-shutdown teardown.
- medium/positive: `good/lockfile_and_preflight.py` — stale-lock reclaim +
  device-state preflight.

## False-positive evals (correct code must not be flagged)

- A single OpenOCD + single GDB session that ends with GDB `detach` and
  OpenOCD `shutdown` — do NOT flag.
- A lock file with a live-owner check that reclaims only dead PIDs — do NOT
  flag.
- `verify_image`/`program ... verify` after a flash — correct, do NOT flag.
- A preflight that checks `lsusb`/device presence and reports "target in DFU —
  run dfu-util" — do NOT flag as failure.

## Historical evals

- stm32-gdb-mcp #30/#48: probe contention from orphaned sessions at ~3
  incidents/day — the agent must recognize that the previous run's processes
  hold the probe and require clean teardown before the next flash.
- openmotion #95: DFU-stranded target + WinUSB preflight — the agent must
  diagnose device state, not retry blindly.
- stm32-mcp: "can't reliably flash" — the agent must treat repeated-flash
  failure as a session/resource problem, not a target fault.

## Adversarial evals

- A flash helper that reports success while the target actually runs stale
  firmware (no verify) — the agent must require read-back verification.
- A "fix" that kills all `openocd` processes system-wide to clear contention —
  must be rejected (kills other users' sessions); PID-scoped cleanup required.
- A lock that is never reclaimed after a crash — must be flagged as a
  permanent contention source.

## Verification commands (target — documented, NOT run here)

```
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg &
arm-none-eabi-gdb -batch -ex "target remote :3333" -ex "load" \
    -ex "detach" -ex quit app.elf
openocd -c "verify_image app.elf" -c "shutdown"
lsusb   # preflight: probe + target VID/PID, DFU state check
```

## Scoring

- precision: every flagged file maps to a named reference rule.
- recall: contention, missing preflight, and no-verify cases detected.
- FP-rate: clean single-probe flows produce zero flags.
