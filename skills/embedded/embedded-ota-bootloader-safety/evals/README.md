# Evaluation — embedded-ota-bootloader-safety

Skill: `skills/embedded/embedded-ota-bootloader-safety`.
Stability target: `evaluated`.

## Verified facts (host, recorded 2026-08-15)

ESP-IDF (`idf.py`), MCUboot (`west`), and QEMU are NOT available on this host
(documented honestly). The rollback-decision and two-slot logic was modeled in
runnable C/python and actually executed:

```
gcc -Wall -Wextra -Werror -O2 examples/good/rollback_counter.c -o rbc
  run:
  "after 3 failed boots: booted_new=1 (still trial)"
  "after 4th reboot: booted_new=0 (rolled back)"
  "stable image committed: booted_new=1, no rollback"
  "PASS: rollback counter behaves per spec"              (exit 0)

python examples/good/rollback_logic.py
  "corrupt image: rolled back to slot A"
  "staged rollout: trial failure blocks 100% push"
  "PASS: two-slot + rollback + staged rollout model"     (exit 0)

gcc -Wall -Wextra -Werror -O2 examples/bad/single_slot.c -o ss
  run: "single-slot update: active_slot_valid=1 (no rollback path)"
gcc -Wall -Wextra -Werror -O2 examples/bad/task_break.c -o tb
  run: "task returned (fatal in FreeRTOS)"
gcc -Wall -Wextra -Werror -O2 examples/bad/power_loss_write.c -o pl
  run: "write reported complete (but power-cut leaves brick)"
```

Bad files compile and run clean — they reproduce each hazard as diagnostic
output and must be caught by review:

- `bad/single_slot.c`: no second slot, no rollback path.
- `bad/task_break.c`: `break` exits the task's own loop → task returns → fatal
  in FreeRTOS (idle hook).
- `bad/power_loss_write.c`: erase+write of the active slot in one step.

NOT verified on this host (target toolchain, do NOT claim to have run):
`idf.py build flash monitor` OTA cycle, MCUboot swap on hardware, power-cut
tests in QEMU.

## Synthetic evals

- easy/negative: `bad/single_slot.c` — no A/B, no rollback.
- easy/negative: `bad/power_loss_write.c` — single-shot slot write.
- medium/negative: `bad/task_break.c` — task-returning break in OTA code.
- easy/positive: `good/rollback_counter.c` — trial counter + commit + rollback.
- easy/positive: `good/rollback_logic.py` — two-slot + staged rollout model.

## False-positive evals (correct code must not be flagged)

- `esp_ota_mark_app_valid_cancel_rollback()` after stable boot — correct, do
  NOT flag.
- A two-slot swap with bootloader CRC/signature validation — approve.
- A `vTaskSuspend(NULL)` (explicit, deliberate pause) in OTA code — do NOT
  flag as a task exit.
- A staged rollout plan (1% → 10% → 100% with telemetry gates) — approve.

## Historical evals

- ESP32-S3 2026 fleet-bricking: a bad OTA config pushed to the whole fleet at
  once — the agent must require staged rollout + trial window + rollback.
- FreeRTOS task-return semantics: `break`/fall-through ending a task triggers
  the fatal idle hook — the agent must flag any control flow that ends a task
  in OTA/update code.

## Adversarial evals

- An image that passes CRC but crashes on boot (no failure counter) — the
  agent must require the trial/commit/rollback mechanism, not just checksum
  validation.
- A "fix" that removes the boot-failure counter "because the image is
  checksummed" — must be rejected.
- A rollback path that requires the crashed image to run to restore the old
  slot (rollback code only in the new image) — must be flagged; rollback must
  live in the bootloader / always-present image.

## Verification commands (target — documented, NOT run here)

```
# ESP-IDF OTA flow:
idf.py set-target esp32s3 && idf.py build flash monitor
# MCUboot:
west build -b <board> samples/bootloader/mcuboot
west flash
# power-cut test (QEMU or bench): interrupt power mid-write and verify the
# bootloader boots the last valid slot
```

## Scoring

- precision: every flagged file maps to a named reference rule.
- recall: single-slot, no-rollback, power-loss, and task-return hazards detected.
- FP-rate: correct two-slot/commit/rollback flows produce zero flags.
