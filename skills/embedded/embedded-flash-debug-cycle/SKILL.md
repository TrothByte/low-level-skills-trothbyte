---
name: embedded-flash-debug-cycle
description: Use when flashing and debugging embedded targets: OpenOCD/GDB probe ownership, SWD probe contention, orphaned GDB sessions, DFU/USB preflight, lock files, and flash-verify steps. Prevents probe contention, "can't reliably flash" loops, and strand issues caused by leftover sessions. Requires single-probe discipline and clean session teardown.
---

# Embedded Flash and Debug Cycle (OpenOCD, GDB, DFU)

## When to use

- Flashing firmware with OpenOCD + GDB or a vendor tool and hitting
  "couldn't open the device", "Error: couldn't claim device", or
  "can't connect".
- Debug sessions where a previous run "left the probe busy" — the next flash
  fails until the leftover process is killed.
- Running any scripted flash/debug loop in CI or locally (flash → test →
  flash).
- Handling a target that is stuck in DFU mode or needs a USB preflight before
  flashing.
- Writing or reviewing tooling that spawns OpenOCD/GDB (MCP servers,
  build-tool integrations).

## When not to use

- Hardware bring-up of registers/clock tree — use
  `embedded-board-bringup-peripheral-init`.
- In-production OTA firmware update safety — use
  `embedded-ota-bootloader-safety`.
- Verifying firmware on hardware in CI — use `embedded-hil-ci-testing`.
- Pure on-target debugging of application logic with no flash step.

## What the agent often gets wrong

- Spawns OpenOCD and GDB without tracking them, so after the run the probe is
  left claimed: GDB keeps the SWD connection open, OpenOCD holds the
  CMSIS-DAP/J-Link device, and the next flash attempt fails with a busy/claim
  error. stm32-gdb-mcp reports this as issues #30/#48 with ~3 incidents per
  day — the dominant real-world failure mode.
- Kills OpenOCD but forgets GDB (or vice versa), or kills the terminal instead
  of the child processes, leaving the probe locked until the OS releases it.
- Does not clean up lock files / `*.lock` artifacts between runs, so a stale
  lock makes subsequent runs fail even though nothing is actually connected.
- Skips the USB/DFU preflight: tries to flash when the device is in DFU mode
  (openmotion #95) or when the USB device is not present, then reports "can't
  reliably flash" without diagnosing the state (openmotion, stm32-mcp).
- Flash-only flow without verify: reports "flashed" based on the programming
  tool's success but never re-reads the flash to confirm the image actually
  landed.

## How to reason correctly

1. Adopt single-probe discipline: exactly one OpenOCD instance and (at most)
   one GDB per probe. Before starting a run, kill any prior session cleanly:
   `gdb -batch -ex "target remote :3333" -ex detach` or kill the process tree.
2. Sequence the session: OpenOCD starts first (it owns the SWD probe), GDB
   connects to it via `target remote localhost:3333` (or the adapter port).
   Teardown is the reverse: GDB detaches, then OpenOCD exits, then verify no
   leftover process holds the device.
3. Use lock files to serialize flash/debug runs, and treat a stale lock as a
   first-class error: `rm -f <lock>` before a fresh run, or make the lock
   identify the PID so a dead owner can be detected and reclaimed.
4. Preflight the device state BEFORE flashing: check `lsusb`/`usb` for the
   probe and target; if the target is in DFU mode or the USB device is missing,
   report the state instead of retrying blindly.
5. After programming, verify: read back the flash (OpenOCD `verify_image` or a
   `program` with verify, GDB `x` over the image range) and compare. "Flash
   reported success" ≠ "flash is correct".

## What to verify

- Exactly one OpenOCD and one GDB per probe; no orphaned processes after a run
  (check `ps`/`Get-Process`).
- Lock files are created atomically and removed or reclaimed; stale locks are
  detected.
- The probe/device is present and not in an unexpected mode (DFU) before flash.
- Flash is verified by read-back after programming, not just by tool success.
- Repeated flash/debug cycles in a loop succeed without escalating contention.

## How to verify

```
# target toolchain (documented; not installed on this host — no openocd,
# no arm-none-eabi-gdb, no probe):
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg &
arm-none-eabi-gdb -batch \
    -ex "target remote :3333" -ex "load" -ex "detach" -ex quit app.elf
openocd -c "verify_image app.elf" -c "shutdown"   # read-back verify

# host-verifiable core: probe/lock discipline simulated in python (see evals)
python examples/good/session_discipline.py
```

The single-probe and lock-file logic is host-verifiable; the OpenOCD/GDB
commands are the documented target commands — status in `evals/README.md`.

## Where the knowledge comes from

- `openocd-docs` — OpenOCD server/client model, interface configs, verify_image,
  device claim behavior.
- `gdb-manual` — remote debugging protocol, detach semantics, batch mode.
- `stm32-gdb-mcp` (issues #30/#48), `openmotion` (#95), `stm32-mcp` — empirical
  probe-contention and DFU failures.

## Related skills

- `embedded-hil-ci-testing` — hardware-in-the-loop CI that automates this cycle
  safely.
- `embedded-board-bringup-peripheral-init` — what to debug once connected.
- `embedded-ota-bootloader-safety` — when flashing moves to the field.

## Evaluation

- Synthetic: flag bad/probe_contention.py (spawns OpenOCD/GDB and never tears
  down); flag bad/missing_preflight.py (flash without DFU/USB check); approve
  good/session_discipline.py and good/lockfile_reclaim.py.
- False-positive: a single well-sequenced OpenOCD+GDB session with clean
  teardown must NOT be flagged; a `verify_image` after `program` must be
  approved.
- Historical: stm32-gdb-mcp #30/#48 (~3 incidents/day) contention class and
  openmotion #95 DFU-strand must be recognized.
- Adversarial: a flash tool that reports success but the target resets and
  runs stale code — the agent must require flash verify.
- Verified facts and commands: `evals/README.md`.
