# Evaluation — embedded-hil-ci-testing

Skill: `skills/embedded/embedded-hil-ci-testing`.
Stability target: `evaluated`.

## Verified facts (host, recorded 2026-08-15)

Zephyr (`west`), ESP-IDF (`idf.py`), and QEMU are NOT available on this host
(documented honestly). The HIL discipline — readiness gating, re-enumeration,
flash-verify, HIL-as-merge-gate — is modeled in runnable python and actually
executed:

```
python examples/good/hil_readiness.py
  "device ready (banner) + image verified -> tests may run"
  "PASS: readiness gate + re-enumeration + flash-verify"   (exit 0)

python examples/good/flash_verify_gate.py
  "verified flash passes; compile-only CI does NOT pass the merge gate"
  "PASS: flash-verify gate + HIL-as-only-merge-gate"        (exit 0)

python examples/bad/missing_readiness.py
  "TEST FAILED: device not ready (race)"      (reproduces openmotion #164)

python examples/bad/skip_verify.py
  "flash reported success"
  "running tests on whatever is on the device"   (no read-back — silent gap)
```

Bad files run clean — they reproduce the readiness race and the missing
flash-verify gate as diagnostic output and must be caught by review.

NOT verified on this host (target toolchain, do NOT claim to have run):
`west build`/`west flash` on real or QEMU hardware, `idf.py flash monitor`,
hardware test-runner output collection.

## Synthetic evals

- easy/negative: `bad/missing_readiness.py` — fixed sleep instead of a state
  gate; race reproduced.
- easy/negative: `bad/skip_verify.py` — flash success without read-back.
- easy/positive: `good/hil_readiness.py` — banner gate + re-enumeration +
  verify.
- medium/positive: `good/flash_verify_gate.py` — verified flash + HIL merge
  gate.

## False-positive evals (correct code must not be flagged)

- A `wait_for(serial, "<app> ready")` probe with timeout — do NOT flag as "a
  sleep" (it is a state gate).
- `west flash --verify` / explicit `verify_image` after programming — approve.
- A merge gate that requires the HIL job plus compile — approve (compile is
  necessary, HIL sufficient).
- Closing the port before flashing and re-opening after re-enumeration — do
  NOT flag as an error.

## Historical evals

- openmotion #164: device-ready race and flash-verify gap in HIL CI — the
  agent must require a state gate and read-back verify, not a sleep.
- ESP-IDF CLAUDE.md / Zephyr copilot-instructions.md: written because AI
  agents misused the flash toolchain — the agent must recognize repo AI
  guidance as a toolchain-safety contract and follow/update it.

## Adversarial evals

- A CI job that is green but never touched hardware (compile-only) — the agent
  must flag it as NOT a valid merge gate (arxiv-2606-16190: 0% without
  hardware feedback).
- A "device ready" check that passes because a previous run's device is still
  connected (stale state) — the agent must require per-run re-enumeration.
- An AI agent proposing to skip the banner wait "because it worked in
  simulation" — must be rejected; simulation is not the HIL gate.

## Verification commands (target — documented, NOT run here)

```
# Zephyr:
west build -b <board> samples/hello_world
west flash --verify
# wait for the app banner, then run the hardware test suite

# ESP-IDF:
idf.py set-target esp32s3 && idf.py build flash monitor
# HIL runner gates on the app banner and verifies the image checksum

# QEMU (no hardware available):
qemu-system-arm -machine netduino2 -nographic -kernel app.elf
```

## Scoring

- precision: every flagged file maps to a named reference rule.
- recall: readiness race, missing flash-verify, and compile-only-gate cases
  detected.
- FP-rate: correct state-gated, verified HIL flows produce zero flags.
