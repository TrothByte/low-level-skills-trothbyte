# Flash & Debug Cycle — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE
→ VERIFICATION → SOURCE. Source ids refer to registry/sources.yaml.

## 1. Single-probe discipline: one OpenOCD, one GDB, clean teardown

- **RULE**: at any moment exactly one OpenOCD instance and at most one GDB
  session may hold a given SWD/JTAG probe. Every run must end with GDB
  detached and OpenOCD exited, or the probe stays claimed and the next run
  fails.
- **WHY AI GETS IT WRONG**: agents start `openocd` and `arm-none-eabi-gdb` in
  a script/agent and never clean them up; the stm32-gdb-mcp project logs this
  as issues #30/#48 at ~3 incidents/day — the dominant flash failure.
- **CORRECT REASONING**: OpenOCD opens the probe (CMSIS-DAP/J-Link/ST-Link)
  exclusively; GDB then opens a TCP session to OpenOCD. If GDB is killed,
  OpenOCD may keep the target asserted; if OpenOCD is killed with GDB attached,
  GDB becomes an orphan. Teardown must be explicit: detach GDB, then shut down
  OpenOCD (`-c "shutdown"` or a clean SIGTERM).
- **EXAMPLE** (bad): killing the terminal window that ran `openocd ... &` —
  the child OpenOCD survives and keeps the probe.
- **COUNTEREXAMPLE** (good):
  ```
  gdb -batch -ex "target remote :3333" -ex detach -ex quit
  openocd -c "shutdown"      # or: taskkill /F /T on the specific PID
  ```
- **VERIFICATION**: after a run, no process holds the probe (check
  `ps`/`Get-Process` for `openocd`/`gdb`; try `openocd -c "init"` and confirm
  it connects).
- **SOURCE**: openocd-docs; gdb-manual; empirical (stm32-gdb-mcp #30/#48).

## 2. Lock files serialize runs; stale locks must be reclaimable

- **RULE**: flash/debug tooling that can run concurrently must serialize with a
  lock file. A stale lock (from a crashed run) must be detected and reclaimed,
  not silently treated as a permanent failure.
- **WHY AI GETS IT WRONG**: agents create a lock, and when a prior crashed run
  left it, they either retry forever or report "can't reliably flash" — both
  observed in the stm32-mcp reports.
- **CORRECT REASONING**: a lock carries owner identity (PID + timestamp). On
  contention, check if the owner process still exists: if not, the lock is
  stale — remove and retry. Removing an owned lock is wrong (two flash jobs
  would collide on the probe); reclaiming a dead owner's lock is correct.
- **EXAMPLE** (bad): `while os.path.exists(lock): sleep(1)` — the crashed
  owner never cleans up, so the loop is infinite.
- **COUNTEREXAMPLE** (good): `if owner_pid_dead(): os.remove(lock)` then
  acquire.
- **VERIFICATION**: kill a "flash" process mid-run and confirm the next run
  reclaims the lock and succeeds (host-runnable in python — see examples).
- **SOURCE**: empirical (stm32-mcp, openmotion); openocd-docs (device claim).

## 3. Preflight device state before flashing (DFU / USB presence)

- **RULE**: before programming, verify the target is in the expected mode and
  the probe/device is present (USB enumeration). A target in DFU mode or a
  missing USB device is a reportable state, not a retryable transient.
- **WHY AI GETS IT WRONG**: openmotion #95 documents the DFU-stranded class:
  the device is in DFU mode, the flash tool can't reach it, and the agent
  retries/reboots instead of diagnosing the mode switch required (jump to
  bootloader, `dfu-util` erase, etc.). Windows WinUSB/preflight issues add a
  second failure mode: the USB driver is wrong and no device is visible.
- **CORRECT REASONING**: enumerate the device tree first
  (`lsusb` / `Get-PnpDevice`), check the VID/PID against the expected probe and
  target. If the target answers as a DFU device (or as an unknown VID/PID),
  report "target in DFU — expected flash, needs bootloader exit" rather than
  hammering the flash tool.
- **EXAMPLE** (bad): `openocd ... flash` loop against a target stuck in DFU,
  never checking which USB device is actually there.
- **COUNTEREXAMPLE** (good): `lsusb` shows the target as DFU; the tool prints
  "target in DFU mode; run dfu-util to exit DFU (or press reset)".
- **VERIFICATION**: `lsusb`/device-manager listing before each flash;
  assert the expected VID/PID.
- **SOURCE**: openocd-docs; empirical (openmotion #95).

## 4. Verify after flash — tool success is not flash correctness

- **RULE**: after programming, re-read the flash and compare with the image
  (`verify_image` in OpenOCD, or a read-back checksum). "Programming success"
  is a claim by the tool; only read-back confirms the target's memory.
- **WHY AI GETS IT WRONG**: the agent treats the tool's exit code as the
  firmware's state; a bad address, wrong image offset, or aborted write can
  leave stale code while the tool reports success.
- **CORRECT REASONING**: the only ground truth is the target's memory. OpenOCD
  `program` can include verify; a separate `verify_image` is the explicit
  read-back. The flash-debug cycle's exit criterion is "flashed AND verified".
- **EXAMPLE** (bad): `openocd -c "program app.elf" -c shutdown; echo OK` — no
  verify.
- **COUNTEREXAMPLE** (good):
  ```
  openocd -c "program app.elf verify" -c shutdown
  # or: openocd -c "verify_image app.elf" -c shutdown
  ```
- **VERIFICATION**: deliberately corrupt a word in flash and confirm the verify
  step reports a mismatch (the verify gate must fail, not pass).
- **SOURCE**: openocd-docs (verify_image, program verify).

## 5. Session teardown is part of the contract, not an afterthought

- **RULE**: a flash/debug helper's lifecycle contract includes teardown:
  release the probe, remove the lock, close TCP sockets. Tools that leave state
  behind (MCP servers, CI runners) produce the same contention for everyone
  using that host.
- **WHY AI GETS IT WRONG**: the model finishes at "flash succeeded" and never
  models the resources it opened; the next user on the same host pays the cost.
- **CORRECT REASONING**: treat OpenOCD's probe claim, the lock file, and GDB's
  TCP session as resources with scoped lifetimes (try/finally in code). The
  "can't reliably flash" reports from stm32-mcp are overwhelmingly residue from
  prior sessions.
- **EXAMPLE** (bad): an MCP flash tool that forks `openocd` and returns without
  waiting or cleaning up.
- **COUNTEREXAMPLE** (good): the tool waits for GDB detach, shuts down OpenOCD,
  and removes the lock in a `finally` block, then reports the probe is free.
- **VERIFICATION**: run the helper twice in a row on the same host — the second
  run must connect cleanly (host-runnable in python — see examples).
- **SOURCE**: gdb-manual (detach); openocd-docs; empirical (stm32-gdb-mcp).

## Quick reference table

| Topic | Rule in one line |
|---|---|
| Probe ownership | one OpenOCD + one GDB; teardown = detach GDB then shutdown OpenOCD |
| Lock files | serialize runs; reclaim stale locks by dead owner PID |
| Preflight | check device presence/mode (DFU) before flashing |
| Verify | `verify_image`/`program ... verify` read-back is the real gate |
| Teardown | resources (probe, lock, TCP) released in finally-style scope |
