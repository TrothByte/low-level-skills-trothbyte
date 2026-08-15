---
name: embedded-ota-bootloader-safety
description: Use when designing or reviewing firmware over-the-air updates and bootloaders: A/B slots, staged rollouts, rollback, trial windows, power-loss-safe flash writes, and watchdog/bootstrap integrity. Prevents fleet bricking from bad OTA configs and task-fatal bugs. Requires staged rollout with telemetry and verified rollback before declaring an update safe.
---

# OTA and Bootloader Safety

## When to use

- Designing or reviewing a firmware OTA pipeline: slot layout, swap logic,
  rollback, staged rollout.
- Writing a bootloader that validates the new image before jumping to it.
- Reviewing an ESP-IDF (`esp_ota_*`) or MCUboot-based update flow.
- Handling the "new firmware boots but the device doesn't come back" case.
- FreeRTOS/Rust embassy task code inside an OTA flow where a bad path can
  crash the boot (e.g. a `break` in the wrong loop returning a task).

## When not to use

- First-time board bring-up and flash via debugger — use
  `embedded-flash-debug-cycle`.
- In-lab flashing with a probe (no OTA) — the probe always can recover.
- Server-side CDN/device-management API design (device telemetry formats
  belong to the infra team; the safety properties here are on-device).
- App-layer update UX with no bootloader involvement.

## What the agent often gets wrong

- Ships a bad OTA config to the whole fleet at once: no staged rollout, no
  trial window, no kill switch — a bad config bricks every device (the ESP32-S3
  2026 fleet-bricking incident class).
- Flashes the new image over the only slot without a rollback path: the
  bootloader validates nothing, and a partially-written or incompatible image
  leaves a bricked device with no fallback.
- Ignores power-loss safety: writes a slot in one giant `erase + write` with no
  resume/verification, so a power cut mid-write corrupts the image and the
  device has no second image to boot.
- In FreeRTOS tasks, writes `break` inside a loop that is not theirs (e.g. a
  shared/foreign loop or a `for` inside a macro), which in FreeRTOS context
  makes the task RETURN — a task that returns triggers the fatal idle-hook /
  task-deleted crash, taking the device down mid-update.
- Treats "image verified" as "image runs": no boot-fail detection, no counter
  of failed boots, so an image that passes CRC but crashes on boot is never
  rolled back.

## How to reason correctly

1. Design for two images from the start: A/B slots with an active slot and a
   pending slot, a bootloader that selects the pending slot and validates it
   (signature, CRC, magic) before booting it.
2. Rollback is mandatory, not optional: mark the new slot "trial" on first
   boot; increment a boot-failure counter on every reboot before the app
   "commits" (set a success flag after N good boots or a health timeout). When
   the counter exceeds the limit, boot the previous slot. This is MCUboot's
   and ESP-IDF's `esp_ota_mark_app_valid_cancel_rollback` model.
3. Power-loss safety: write to a free/scratch area or use
   erase-then-write-with-status-flags so the bootloader can distinguish
   "complete", "pending", and "aborted". Never single-shot a slot write that a
   power cut can leave half-done without a marker.
4. Stage the rollout: 1% → 10% → 100% with per-batch health telemetry
   (boot success, crash rate, network reconnection). A trial window before
   commit means a bad config harms only the trial cohort, and the rest can be
   paused.
5. For task code in OTA paths, inspect every `break`/`continue`/`return`:
   in FreeRTOS a task returning (not suspending) hits the idle task hook
   (vApplicationIdleHook / configUSE_IDLE_HOOK) — effectively a device-wide
   fault. Loop bodies inside OTA macros must not `break` out of a loop they do
   not own.

## What to verify

- Two slots exist with a bootloader that validates the pending image
  (signature/CRC/magic) before boot, and a failure counter triggers rollback.
- The app explicitly marks the update "valid" (commit) only after stable
  operation; unmarked images roll back.
- Power-loss safety: slot writes are resumable/verifiable; the bootloader can
  recover a half-written slot to the known-good one.
- Rollout plan includes a trial window and a pause/kill path (device-side
  rollback flag or backend gate).
- No task-returning `break` in OTA/update code; task loops use
  `vTaskSuspend`/proper task control, not fall-through exits.
- All of the above demonstrated: simulate a bad image and confirm rollback;
  simulate power loss mid-write and confirm recovery.

## How to verify

```
# host-verifiable core: rollback counter + A/B decision logic in C/python
python examples/good/rollback_logic.py        # simulates trial window + rollback
gcc -Wall -Wextra -Werror -O2 examples/good/rollback_counter.c -o rbc

# target (documented; not on this host — no idf.py/MCUboot/qemu):
idf.py build flash monitor            # ESP-IDF OTA flow (esp-idf-docs)
# MCUboot: west build -b <board> samples/bootloader/mcuboot  (mcuboot docs)
# power-cut test: drop power mid-write in QEMU, verify boot falls back
```

The rollback-decision logic is host-verifiable; the ESP-IDF/MCUboot/QEMU
commands are documented targets — status in `evals/README.md`.

## Where the knowledge comes from

- `mcuboot` — A/B slot model, image validation, boot-failure counters,
  rollback.
- `esp-idf-docs` — `esp_ota_*` API: slot selection, marking valid,
  rollback (`esp_ota_mark_app_valid_cancel_rollback`).
- `zephyr-docs` — MCUboot/Zephyr OTA integration and bootloader config.
- ESP32-S3 2026 fleet-bricking incident — empirical.

## Related skills

- `embedded-flash-debug-cycle` — local flashing; OTA extends it to the field.
- `embedded-hil-ci-testing` — verifying OTA behavior on hardware in CI.
- `rtos-concurrency-and-isr` — task lifecycle rules (no returning tasks).
- `embedded-board-bringup-peripheral-init` — device brings up after update.

## Evaluation

- Synthetic: flag bad/single_slot.c (no rollback), bad/task_break.c
  (FreeRTOS break in a foreign loop), bad/power_loss_write.c (single-shot
  slot write); approve good/rollback_counter.c and good/rollback_logic.py.
- False-positive: `esp_ota_mark_app_valid_cancel_rollback()` after stable boot
  must NOT be flagged; a correct A/B swap with verified rollback must be
  approved.
- Historical: the ESP32-S3 2026 fleet-bricking incident and FreeRTOS
  task-return-fatal semantics must be recognized.
- Adversarial: an image that passes CRC but crashes on boot — the agent must
  require the boot-failure counter and rollback, not just checksum validation.
- Verified facts and commands: `evals/README.md`.
