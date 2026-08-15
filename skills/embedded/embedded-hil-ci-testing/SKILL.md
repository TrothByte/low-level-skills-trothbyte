---
name: embedded-hil-ci-testing
description: Use when setting up or running embedded hardware-in-the-loop CI: device-ready gating, re-enumeration after flashing, flash-verify gates, and toolchain-use guardrails from ESP-IDF/Zephyr AI guidance. Prevents flaky HIL runs from device-ready races and treating compile as deploy approval. Requires verifying on hardware before declaring a change ready.
---

# Hardware-in-the-Loop (HIL) CI Testing

## When to use

- Setting up CI that flashes firmware to real hardware and runs tests on it.
- Debugging flaky HIL runs: device not ready, re-enumeration after flash,
  flash not verified.
- Deciding whether a "compiles + host test passes" result is enough to merge —
  for firmware, it is not.
- Reviewing or writing the AI-toolchain guidance files in an embedded repo
  (ESP-IDF CLAUDE.md, Zephyr copilot-instructions.md) that govern how AI
  agents use flash/build tools.
- Any "0% deploy without hardware feedback" gate discussion for firmware.

## When not to use

- Non-embedded CI (pure host-side unit tests) — no hardware gate.
- One-off local flashing without CI — use `embedded-flash-debug-cycle`.
- OTA fleet rollout design — use `embedded-ota-bootloader-safety`.
- Coverage/quality metrics unrelated to hardware feedback.

## What the agent often gets wrong

- Starts testing immediately after the flash command returns, racing the
  target's reboot: the device hasn't re-enumerated (USB) or the app hasn't
  started, so the first test fails spuriously (openmotion #164 class).
- Treats "flash reported success" as the gate and never re-reads flash to
  verify — a bad write passes CI and ships (flash-verify missing).
- Models "device ready" as a fixed sleep instead of a state probe: waits 2 s
  and still races; HIL needs readiness gating (device present, app banner,
  port stable), not time guessing.
- Ignores re-enumeration: after flashing, the USB/COM port disappears and
  reappears with the same or a different name — tests attached to the old port
  hang or hit a dead device.
- Lets AI agents call the flash/probe tools with no guardrails: ESP-IDF
  CLAUDE.md and Zephyr copilot-instructions.md exist precisely because agents
  misused the toolchain (flash without readiness, run against the wrong
  device, corrupt shared probes).

## How to reason correctly

1. Gate on device-ready STATE, not time: after flash, poll for the expected
  condition — serial banner printed by the app, USB device with the expected
  VID/PID/COM name, or a test-runner "ready" message. Only then start the
  tests.
2. Re-enumerate deliberately: after flashing, close the old port/session,
  wait for the device to reappear (or to announce itself), then open the new
  port. Treat "port vanished" as the expected transition, not an error.
3. Verify flash by read-back (`verify_image`/checksum) before declaring the
  build flashed; a HIL run on an unverified image tests unknown code.
4. Treat HIL as the only real gate for firmware changes: compile + host tests
  are necessary but never sufficient (arxiv-2606-16190: 0% deploy without
  hardware feedback). Merge criteria must include a green HIL run.
5. Put toolchain guardrails in the repo's AI guidance: pin the exact
  flash/verify commands, forbid blind `--force`/erase, require the readiness
  gate and flash-verify before tests, and specify single-probe discipline so
  AI agents do not contend for the same hardware.

## What to verify

- Every HIL job gates on an explicit device-ready probe (banner, USB
  enumeration, stable port), never a bare sleep.
- Flash is verified by read-back before tests start.
- Re-enumeration is handled: the post-flash port/USB change is awaited and the
  new endpoint is used.
- The CI merge gate includes the hardware run; a compile-only result cannot
  pass the gate.
- The repo's AI-guidance file (CLAUDE.md/copilot-instructions.md) states the
  flash/verify commands and forbids misuse (blind force-flash, shared-probe
  contention, skipping readiness).

## How to verify

```
# host-verifiable core: readiness gate + re-enumeration + flash-verify in python
python examples/good/hil_readiness.py
python examples/bad/missing_readiness.py        # reproduces the race

# target (documented; not on this host — no west/idf.py/qemu):
west build -b <board> && west flash              # Zephyr
idf.py build flash monitor                       # ESP-IDF
# HIL runner: wait for device banner, run pytest/hardware tests, collect
```

The readiness-gating logic is host-verifiable; the west/idf.py/QEMU commands
are documented targets — status in `evals/README.md`.

## Where the knowledge comes from

- `zephyr-docs` — `west flash`, device naming, re-enumeration notes;
  `copilot-instructions.md` ecosystem guidance.
- `esp-idf-docs` — `idf.py` flash/monitor semantics, CLAUDE.md AI toolchain
  guidance.
- `arxiv-2606-16190` — empirical evidence that deploy approval requires
  hardware feedback (0% without it).
- `openmotion` (#164) — empirical HIL device-ready race and flash-verify gap.

## Related skills

- `embedded-flash-debug-cycle` — the flash/verify primitives HIL automates.
- `embedded-ota-bootloader-safety` — when HIL-verified firmware goes to the
  field.
- `embedded-board-bringup-peripheral-init` — making the app boot so HIL can
  probe it.

## Evaluation

- Synthetic: flag bad/missing_readiness.py (test before device ready);
  flag bad/skip_verify.py (no flash read-back); approve good/hil_readiness.py
  and good/flash_verify_gate.py.
- False-positive: a real device-ready probe (banner/VID-PID wait) with
  post-flash re-enumeration must NOT be flagged; a `west flash` + verify
  sequence must be approved.
- Historical: openmotion #164 device-ready race and the ESP-IDF CLAUDE.md /
  Zephyr copilot-instructions.md rationale must be recognized.
- Adversarial: a CI job that "passes" because the host test never ran on
  hardware — the agent must require the HIL gate.
- Verified facts and commands: `evals/README.md`.
