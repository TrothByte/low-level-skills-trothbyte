# HIL CI Testing — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE
→ VERIFICATION → SOURCE. Source ids refer to registry/sources.yaml.

## 1. Gate HIL tests on device-ready STATE, never on a sleep

- **RULE**: after flashing, a HIL test may start only when the target proves it
  is ready: app banner on serial, USB device enumerated with the expected
  VID/PID, or a stable port. Time-based waits are guesses and race the device.
- **WHY AI GETS IT WRONG**: agents insert `sleep(2)` after flash and start
  tests; the device boots slower on a cold start, the USB stack re-enumerates,
  and the first test fails spuriously — the openmotion #164 device-ready race
  class.
- **CORRECT REASONING**: readiness is a state, probed and polled. The app
  prints a fixed banner ("<app> v1.0 ready"); the harness waits for that string
  (with timeout) before running tests. A test run without a confirmed-ready
  device is not a HIL test — it is noise.
- **EXAMPLE** (bad):
  ```python
  flash(); time.sleep(2); run_tests()   # races boot + USB re-enumeration
  ```
- **COUNTEREXAMPLE** (good):
  ```python
  flash()
  wait_for(serial, "<app> v1.0 ready", timeout=15)   # state, not time
  run_tests()
  ```
- **VERIFICATION**: run the sequence repeatedly; a device-ready gate yields
  zero spurious "device not found" failures across runs (host model in
  examples/good/hil_readiness.py).
- **SOURCE**: empirical (openmotion #164); zephyr-docs (device handling).

## 2. Re-enumeration after flash is the expected transition

- **RULE**: flashing resets the device; USB/COM endpoints disappear and
  reappear (possibly under a new port name). The harness must close the old
  endpoint, await the new one, and reconnect — treating "port gone" as the
  normal step, not a fatal error.
- **WHY AI GETS IT WRONG**: agents keep a single port handle from before the
  flash and the first post-flash read times out or hits the dead endpoint; or
  they fail the job when the port vanishes mid-flash.
- **CORRECT REASONING**: the flash command itself changes the device state.
  After `west flash`/`idf.py flash`, close handles, poll for the device to
  reappear (USB enumeration or new COM name), then open the fresh endpoint.
  The re-enumeration wait is part of the readiness gate (rule 1).
- **EXAMPLE** (bad): reusing `serial = open('/dev/ttyUSB0')` across the flash
  boundary.
- **COUNTEREXAMPLE** (good): `close(); wait_for_reenum(); serial = open(port)`
  with a discovery loop that accepts the renamed port.
- **VERIFICATION**: a HIL script that flashes and then finds the device again
  by scanning `lsusb`/port list until the expected VID/PID or banner appears.
- **SOURCE**: zephyr-docs; esp-idf-docs; empirical (openmotion #164).

## 3. Flash-verify is part of the HIL gate

- **RULE**: before running tests, verify the flashed image by read-back
  (`verify_image`, checksum comparison). A HIL run against an unverified image
  tests unknown bytes.
- **WHY AI GETS IT WRONG**: agents treat the flash tool's exit code as the
  image's truth; a bad write (wrong offset, interrupted programming) passes and
  the tests silently exercise stale code — the "compile passes so it ships"
  trap in disguise.
- **CORRECT REASONING**: the image on the device is the tested artifact. The
  flash step must be `flash + verify`; only a verified image enters the test
  phase. This is also the guardrail the ESP-IDF CLAUDE.md and Zephyr
  copilot-instructions.md impose on AI agents using the toolchain.
- **EXAMPLE** (bad): `west flash; run_tests()` with no read-back.
- **COUNTEREXAMPLE** (good): `west flash --verify` (or explicit
  `verify_image`), then a checksum comparison in the harness before tests.
- **VERIFICATION**: corrupt a byte in the image, flash it, and confirm the
  verify gate fails (never reaches tests).
- **SOURCE**: zephyr-docs; esp-idf-docs; empirical (openmotion #164
  flash-verify gap).

## 4. HIL is the only real gate; compile/host tests are not sufficient

- **RULE**: for firmware, a change is mergeable only after a passing HIL run.
  Compile + host-side unit tests are necessary but never sufficient
  (arxiv-2606-16190: deploy approval without hardware feedback is 0%).
- **WHY AI GETS IT WRONG**: agents mark firmware PRs "CI green" when the CI
  only builds and runs host logic; the board is never touched, yet the change
  is approved.
- **CORRECT REASONING**: register layout, timing, peripherals, and boot
  behavior exist only on hardware. The merge gate must include a hardware run
  with a device-ready gate and flash-verify. If no hardware is available, the
  honest state is "not verified on hardware", not "passed".
- **EXAMPLE** (bad): a PR that changes a register write, "verified" by a host
  unit test that never executes the write.
- **COUNTEREXAMPLE** (good): the same PR runs on the board: device boots,
  banner appears, functional test passes — then it merges.
- **VERIFICATION**: the CI config requires the HIL job as a merge check; a
  compile-only job cannot satisfy it.
- **SOURCE**: arxiv-2606-16190; empirical (openmotion #164).

## 5. AI toolchain guardrails belong in the repo guidance

- **RULE**: embedded repos that welcome AI agents ship guidance
  (ESP-IDF CLAUDE.md, Zephyr copilot-instructions.md) pinning the exact
  flash/verify commands, the readiness gate, and forbidden actions (blind
  force-flash, shared-probe contention, skipping verify).
- **WHY AI GETS IT WRONG**: unguided agents misuse the toolchain — flashing
  without readiness, erasing shared probes, running tests against the wrong
  device — which is precisely why those ecosystem guides were written.
- **CORRECT REASONING**: the guidance file is part of the engineering
  contract: "run `idf.py flash verify`; wait for the app banner; never
  `--force` erase a shared probe; use one probe per session". Reviewing a
  change to that file is reviewing the toolchain safety rules.
- **EXAMPLE** (bad): an agent editing an ESP-IDF build adding an unconditional
  erase step with no readiness gate.
- **COUNTEREXAMPLE** (good): the repo guidance mandates flash+verify and the
  banner gate; agent changes must conform.
- **VERIFICATION**: a lint/CI check that the guidance file's commands exist and
  are the ones HIL uses.
- **SOURCE**: esp-idf-docs (CLAUDE.md); zephyr-docs (copilot-instructions.md).

## Quick reference table

| Topic | Rule in one line |
|---|---|
| Readiness | gate tests on device state (banner/VID-PID), never a sleep |
| Re-enumeration | port disappearing after flash is expected — await the new endpoint |
| Flash-verify | read-back the image before testing; unverified = unknown bytes |
| HIL gate | hardware run is the only merge gate (arxiv-2606-16190) |
| AI guardrails | repo guidance pins commands + forbids force-flash/probe contention |
