---
name: embedded-mpu-trustzone
description: Use when writing, reviewing, or debugging Cortex-M firmware that configures the MPU (ARMv7-M RBAR/RASR or ARMv8-M RBAR/RLAR regions, PRIVDEFENA background region, AP privilege) or ARMv8-M TrustZone (SAU regions, NSC veneers, secure/non-secure transitions, flash/SRAM partitioning), or when secure software faults on non-secure calls.
---

# Embedded MPU & TrustZone (ARMv7-M / ARMv8-M)

## When to use

- Configuring or auditing a Cortex-M MPU — ARMv7-M (M3/M4/M7, RBAR/RASR) or
  ARMv8-M (M23/M33/M55, RBAR/RLAR + MAIR): region size/alignment, per-region
  enable, PRIVDEFENA background region, AP/privilege.
- ARMv8-M TrustZone bring-up: SAU regions, NSC veneers and SG gateways,
  secure-to-non-secure calls, flash/SRAM partitioning and boot handover.
- Debugging MemManage faults, SecureFaults, or non-secure code that faults on
  its first access after handover.

## When not to use

- ARMv8-A/AArch64 (MMU/TTBR/TCR, EL1/EL2/EL3) — a different regime.
- RISC-V PMP or x86 protection — different mechanisms.
- Cortex-M0/M0+ without an MPU — the default memory map applies.

## What the agent often gets wrong

- "Regions are configured, so the MPU protects" — a region is inert unless its
  ENABLE bit AND `MPU_CTRL.ENABLE` are both set.
- Arbitrary sizes/bases — regions must be power-of-two sized (min 32 B), base
  aligned to size.
- PRIVDEFENA covers privileged accesses only; unprivileged accesses to
  unmapped addresses still fault.
- Writing MAIR on ARMv7-M — MAIR is ARMv8-M-only; ARMv7-M uses TEX/S/C/B.
- "TrustZone needs no SAU" — with the SAU disabled everything stays Secure and
  the non-secure image faults on first access.
- Marking the veneer region Non-secure instead of NSC, or an NSC entry whose
  first instruction is not SG.
- Calling non-secure functions without `cmse_nonsecure_call` (+ `-mcmse`): the
  callee runs in Secure state.
- MMIO via non-volatile pointers or cacheable regions — stale reads at `-O2`.
- Non-secure interrupts cannot preempt Secure state; they stay pending until
  control returns to Non-secure.

## How to reason correctly

1. Name the target: ARMv7-M (RBAR/RASR) vs ARMv8-M (RBAR/RLAR + MAIR) vs no
   MPU. The register model differs; mixing them is a bug.
2. For every address the firmware touches: which MPU region covers it (AP,
   attributes), and what is its SAU/IDAU attribution (ownership).
3. Build descriptors through a helper validating power-of-two size and
   base-aligned-to-size; never hand-encode them.
4. Enable explicitly: per-region EN, then MPU ENABLE plus a deliberate
   PRIVDEFENA choice, then DSB/ISB.
5. Configure the SAU in Secure state before the first transition to
   Non-secure; provide an NSC region if NS-to-S calls are required; annotate
   NS function pointers with `cmse_nonsecure_call` and build with `-mcmse`.
6. Verify on target (QEMU or hardware), not by "it compiled".

## What to verify

- Every region: power-of-two size >= 32 B, base aligned to size, limit =
  base + size - 1.
- ENABLE bits (per-region and MPU-level); PRIVDEFENA matches intent.
- ARMv8-M: MAIR slots Device for MMIO; SAU enabled with an NSC region when
  needed; the flash/SRAM partition matches the linker scripts.
- Secure->NS calls compile to BLXNS; NSC entries begin with SG.
- MMIO pointers volatile; no SecureFault in `SAU_SFSR`/`SFAR` after NS boot.

## How to verify

```
# host gate (this repository): syntax + invariant asserts
gcc -Wall -Wextra -Werror -O2 examples/good/mpu_trustzone_good.c -o build/tz_good
build/tz_good                        # must exit 0
gcc -Wall -Wextra -Werror -O2 examples/bad/mpu_trustzone_bad.c -o build/tz_bad
build/tz_bad                         # aborts on the first failed invariant

# target verification (ARM toolchain + QEMU or hardware):
arm-none-eabi-gcc -mcpu=cortex-m33 -mthumb -mcmse -Wall -Wextra -Werror -O2 \
  -c examples/good/mpu_trustzone_good.c
qemu-system-arm -machine mps2-an505 -cpu cortex-m33 -nographic    # ARMv8-M TZ
qemu-system-arm -machine mps2-an385 -cpu cortex-m3  -nographic    # ARMv7-M MPU
```

## Where the knowledge comes from

- ARMv8-M ARM (ARM DDI 0553A); ARMv7-M ARM (ARM DDI 0403)
- CMSIS-5 headers `core_cm4.h`, `core_cm33.h`, `mpu_armv7.h`, `mpu_armv8.h`
  (Apache-2.0) — register bit positions and attribute encodings
- Arm CMSE requirements (ARM DUI 0648); GCC manual (`-mcmse`,
  `cmse_nonsecure_entry`/`cmse_nonsecure_call`); QEMU docs

## Related skills

- `embedded-volatile-and-memory-ordering` — required: volatile MMIO, barriers
- `memory-ordering-reasoning` — DSB/ISB and device attributes
- `embedded-linker-script` — flash/SRAM partition, NSC section placement
- `compiler-ub-assumptions` — why `-O2` breaks non-volatile MMIO

## Evaluation

- Synthetic: easy (missing ENABLE), medium (misaligned base/size), hard
  (SAU + NSC + veneer placement), adversarial (config "works" on the host
  model but faults on target).
- False-positive: a correct MPU + SAU setup must NOT be flagged.
- Target gate: QEMU `mps2-an385` / `mps2-an505` or Cortex-M33 hardware; see
  `evals/README.md` for commands and recorded results.
