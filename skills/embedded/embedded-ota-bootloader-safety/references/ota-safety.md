# OTA & Bootloader Safety — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE
→ VERIFICATION → SOURCE. Source ids refer to registry/sources.yaml.

## 1. Two-slot (A/B) layout with validated boot is the baseline

- **RULE**: a robust OTA design has two image slots, a bootloader that picks
  the pending slot and validates it (signature, CRC, magic) before booting, and
  a clear active/pending state. Single-slot OTA is inherently unsafe.
- **WHY AI GETS IT WRONG**: agents propose "just overwrite the one image" as a
  minimal solution, forgetting that a half-written or wrong image leaves no
  fallback; the bootloader then either refuses to boot (brick) or boots
  garbage.
- **CORRECT REASONING**: the bootloader must know which slot to boot and must
  verify the image before handing over. MCUboot's model: images live in
  `slot0`/`slot1`, a boot status header tracks the swap, and the new image is
  only "active" after the swap completes and the app confirms it runs.
- **EXAMPLE** (bad): writing the update over the boot slot with no second
  image; a failed write bricks the device.
- **COUNTEREXAMPLE** (good): write the new image to the inactive slot, then
  request a swap; the bootloader boots the new slot and can swap back.
- **VERIFICATION**: simulate a corrupt new image — the bootloader must refuse
  it and boot the previous slot.
- **SOURCE**: mcuboot (design/architecture); esp-idf-docs (esp_ota APIs).

## 2. Trial window + commit; rollback is mandatory

- **RULE**: after booting the new image, it is in TRIAL state until the app
  commits it (marks valid). Each reboot before commit increments a failure
  counter; exceeding the limit rolls back to the previous image.
- **WHY AI GETS IT WRONG**: the model validates the image's checksum and
  declares the update done — but a checksum-valid image can still crash on
  boot; without a boot-failure counter there is no rollback trigger, and the
  fleet stays bricked (the ESP32-S3 2026 class).
- **CORRECT REASONING**: "validated" and "works" are different states. The app
  must call the commit API (`esp_ota_mark_app_valid_cancel_rollback`) only
  after N stable boots or a health timeout; the bootloader counts failed boots
  of the pending image and reverts on overflow.
- **EXAMPLE** (bad): an update that boots but crashes in 2 seconds; no counter,
  no rollback — every device reboots into the crashing image forever.
- **COUNTEREXAMPLE** (good): the boot failure counter reaches the limit on the
  4th reboot; the bootloader boots slot0, the old working image.
- **VERIFICATION**: host-runnable model (examples/good/rollback_logic.py)
  demonstrates the counter deciding to roll back; target test boots a crashing
  image and observes recovery.
- **SOURCE**: mcuboot (boot status, rollback); esp-idf-docs
  (`esp_ota_mark_app_valid_cancel_rollback`).

## 3. Power-loss safety for slot writes

- **RULE**: slot writes must survive power loss: write to scratch/standby
  areas, track a status flag per phase, and let the bootloader distinguish
  "complete", "pending", "aborted". Never leave the active slot half-written
  with no recovery path.
- **WHY AI GETS IT WRONG**: agents write the image as one `erase + write` to
  the target slot; a power cut mid-write leaves a corrupt active image and the
  device cannot boot at all.
- **CORRECT REASONING**: OTA flash is a transaction. The bootloader keeps the
  current image intact until the new one is complete and verified in the other
  slot (or scratch), then switches. Power-loss recovery = boot the still-valid
  slot.
- **EXAMPLE** (bad): `erase_slot(target); write_image(target);` with no
  ordering guarantee — a cut between the two bricks the device.
- **COUNTEREXAMPLE** (good): write image to inactive slot; set "pending" flag;
  verify; atomically flip the boot slot selector; clear flag.
- **VERIFICATION**: drop power at random points during a simulated write
  (QEMU/documented target) and confirm the bootloader always recovers to a
  valid image.
- **SOURCE**: mcuboot (swap/scratch model); esp-idf-docs (partition write
  flow).

## 4. Staged rollout with trial window and telemetry

- **RULE**: production OTA is staged: small trial cohort first, telemetry
  gate, then increasing percentages. A bad config affects only the trial
  cohort and can be paused before reaching the fleet.
- **WHY AI GETS IT WRONG**: the ESP32-S3 2026 incident class — a single bad OTA
  config pushed to the entire fleet at once, no trial, no kill switch, mass
  bricking.
- **CORRECT REASONING**: the on-device rollback (rules 1–2) is the last line
  of defense; the rollout plan is the first. Every batch must have a health
  criterion (boot success rate, crash rate, connectivity) and the ability to
  pause before the next batch.
- **EXAMPLE** (bad): 100% rollout of an unverified config in one push.
- **COUNTEREXAMPLE** (good): 1% cohort for 24 h with crash-rate gate → 10% →
  100%; any regressions pause the pipeline.
- **VERIFICATION**: telemetry from the trial cohort reviewed before widening;
  a canary failure halts the rollout.
- **SOURCE**: empirical (ESP32-S3 2026 fleet-bricking incident); esp-idf-docs
  (device telemetry integration).

## 5. Task code in OTA paths must not return tasks to a fatal idle hook

- **RULE**: in FreeRTOS, a task that returns (falls off the end of its
  function) is deleted by the idle task and, with `configUSE_IDLE_HOOK`, hits
  the idle hook — commonly a fatal assert/halt. A `break` inside a loop the
  task does not own (a macro, a foreign loop, a nested callback) can make the
  task return and take the whole device down mid-update.
- **WHY AI GETS IT WRONG**: agents place OTA logic inside a helper loop and use
  `break`/`continue` expecting structured control flow; in FreeRTOS context the
  "loop" may be the task's own wrapper, and breaking out of it ends the task.
- **CORRECT REASONING**: tasks are functions; control flow that exits the task
  function is fatal by default. In OTA code, exit paths must be explicit:
  `vTaskSuspend(NULL)` for pause, `vTaskDelete(NULL)` only deliberately.
  Inspect every `break`/`return` in task-shaped code for what it exits.
- **EXAMPLE** (bad): a `for` loop in an OTA download helper where a malformed
  chunk `break`s out of the caller's task loop → task returns → idle hook
  faults the device mid-flash.
- **COUNTEREXAMPLE** (good): the loop sets an error flag and `continue`s; the
  task itself decides to suspend/delete explicitly.
- **VERIFICATION**: code review of every `break`/`continue`/`return` in
  task-shaped OTA code; a host model that detects "task function would return"
  (examples/bad/task_break.c marked intentionally incorrect).
- **SOURCE**: empirical (FreeRTOS task-return semantics);
  zephyr-docs/esp-idf-docs (thread/event-loop models for the same hazard).

## Quick reference table

| Topic | Rule in one line |
|---|---|
| A/B slots | two images + validated boot; single-slot OTA is unsafe |
| Trial & rollback | commit only after stable boot; failure counter reverts |
| Power loss | writes staged with status flags; bootloader recovers a valid slot |
| Staged rollout | trial cohort + telemetry gate before wider push |
| Task exits | `break`/return that ends a FreeRTOS task is fatal — make exits explicit |
