---
name: bootloader-uefi-firmware
description: Use when writing, reading, or debugging UEFI firmware and edk2 code: Boot vs Runtime Services, the PI spec boundary, ExitBootServices ordering, HII/VFR forms, ACPI/SMBIOS tables, and Secure Boot. Prevents boot-services-after-exit crashes, missing RUNTIME_ACCESS flags, and VFR bounds that the driver ignores.
---

# Bootloader: UEFI Firmware (edk2, PI, HII, ACPI, Secure Boot)

## When to use

- Writing or reviewing edk2 modules (DXE drivers, UEFI applications).
- Debugging firmware that crashes or fails at specific boot stages
  (DXE vs BDS vs after ExitBootServices).
- Authoring HII/VFR forms or variable policy (SetVariable attributes).
- Reading or generating ACPI tables / SMBIOS structures.
- Implementing or auditing Secure Boot / signature policy logic.
- Simulating firmware in QEMU with OVMF and tracing serial logs.

## When not to use

- Bare-metal microcontroller firmware (Cortex-M, no UEFI) — use
  `embedded-*` skills and `bootloader-stages` for the pre-UEFI boot path.
- OS-level UEFI access from the OS runtime (efivarfs) — that is a kernel
  concern.
- Hardware bring-up/board support packages in edk2 that are pure silicon
  drivers — see `embedded-*` for register/MMIO patterns.

## What the agent often gets wrong

- Calls Boot Services (AllocatePool, GetMemoryMap, LocateProtocol) AFTER
  `ExitBootServices()` — the API table is gone; this is a guaranteed
  crash on real firmware. The ordering discipline (PI spec chapter on the
  transition) is the core fact.
- Sets variables without `EFI_VARIABLE_RUNTIME_ACCESS` and claims the OS
  can read them — after the transition the variable is invisible
  (EFI_NOT_FOUND).
- Treats HII/VFR `minimum`/`maximum` as enforced bounds. The VFR form
  engine displays the limits, but the DRIVER must validate on save; the
  form alone does not clamp stored values.
- Confuses DXE, BDS, and Runtime phases: what is legal in DXE (Boot
  Services, `gBS`) is illegal in Runtime (only `gRT` plus memory marked
  Runtime).
- Hand-writes ACPI/SMBIOS byte offsets from memory instead of using the
  spec's structure fields, producing tables the OS rejects.
- Assumes Secure Boot is "just a flag": the signature database
  (db/dbx/KEK/PK), the signed payload chain, and the self-contained
  verification order are all mandatory; disabling enforcement is a
  security decision, not a workaround.
- Forgets that firmware debugging is done over a serial log: QEMU + OVMF
  with `-serial stdio`/`-serial file:` and edk2 DEBUG prints are the
  primary visibility tool.

## How to reason correctly

1. **Phase discipline first**: name the phase your code runs in (DXE, BDS,
   Runtime) and the services available there (`gBS` vs `gRT`). Any code
   path that crosses ExitBootServices must stop using `gBS` at that
   point; allocations for runtime must be `EfiRuntimeServicesData` and
   converted with `gRT->ConvertPointer`.
2. **For variables, enumerate the three attribute requirements**:
   NON_VOLATILE (persist), BOOTSERVICE_ACCESS (readable in BS),
   RUNTIME_ACCESS (readable after exit). A variable needed by the OS needs
   all three.
3. **For HII, treat the VFR as the UI contract, not the enforcement**:
   bounds are enforced by the driver's validation callback
   (`CheckBox/Numeric` value callbacks) — verify the driver rejects
   out-of-range input before storing.
4. **For tables, go to the spec fields**: ACPI (`RSDP`, `FADT`, `DSDT`),
   SMBIOS (type 0/1/2/3...) field-by-field; never invent offsets.
5. **Debug via serial + simulator**: run `qemu-system-x86_64 -bios
   OVMF.fd -serial stdio`; read the edk2 DEBUG log for the phase where the
   failure occurs, then inspect the exact services that code path uses.

## What to verify

- No Boot Service is referenced on any path after ExitBootServices.
- Variables that must survive into the OS carry RUNTIME_ACCESS (plus the
  other required bits).
- HII numeric bounds are enforced in the driver's save/validate callback,
  not only in the VFR.
- ACPI/SMBIOS fields are produced from the spec's structure definitions.
- Secure Boot logic checks the signature database in the mandated order
  (db → dbx revocation first, then KEK/PK for key enrollment).
- The QEMU+OVMF serial boot log shows the expected phase sequence and no
  unexpected abort/ASSERT.

## How to verify

```
# Target flow (QEMU + OVMF documented; toolchain not installed here):
qemu-system-x86_64 -bios OVMF.fd -m 256 -serial stdio -display none
#   boot to the shell/OS, capture the serial log; grep for DEBUG lines.

# edk2 build (documented):
build -a X64 -t GCC5 -p OvmfPkg/OvmfPkgX64.dsc
#   produces OVMF.fd; ASSERT/abort messages appear in the serial log.

# For ACPI/SMBIOS (documented):
acpidump -o acpi.dat && iasl -d acpi.dat    # decode and review tables
```

Researched — QEMU/OVMF/edk2 not installed on this Windows host. Commands
above are the documented verification; marked UNVERIFIED as runs.

## Where the knowledge comes from

- `uefi-spec` — UEFI + PI specification: Boot/Runtime Services, phase
  boundaries, variable attributes, HII/VFR, ACPI/SMBIOS integration,
  Secure Boot (db/dbx/KEK/PK).
- `edk2-docs` — edk2 build system, module INF/DSC layout, DEBUG macros,
  OVMF platform build.
- `qemu-docs` — QEMU system emulation, `-bios`, `-serial` log capture.

## Related skills

- `bootloader-stages` — the pre-UEFI boot path (Power-on → SEC/PEI
  transition).
- `embedded-*` — MMIO/register patterns used inside silicon drivers.
- `meta-verification-harness-validity` — the serial log is the harness
  here; a boot log that "looks fine" must be shown to fail on a broken
  driver (ablation) before it proves anything.
- `elf-dynamic-linking-got-plt` — PE/COFF loading mechanics inside edk2.

## Evaluation

- Synthetic: `bad/boot_services_after_exit.c`,
  `bad/runtime_flag_missing.c`, `bad/vfr_unenforced_bounds.vfr` must be
  flagged; `good/boot_services_before_exit.c`,
  `good/runtime_flag_correct.c`, `good/vfr_validated_bounds.vfr` must NOT
  be.
- False-positive: a variable with only BOOTSERVICE_ACCESS that is only
  used in BS is CORRECT (no RUNTIME_ACCESS needed) — do not flag.
- Adversarial: the boot-services-after-exit code "works" in a unit test
  environment (stubs) and only crashes on real firmware at runtime.
- Historical: UEFI phase-transition and variable-attribute bugs
  documented in uefi-spec/edk2 (UNVERIFIED as named incidents on this
  host).
- Researched commands and status: `evals/README.md`.
