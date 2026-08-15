# Bootloader: UEFI Firmware — Reference Rules

Knowledge layer for `bootloader-uefi-firmware`. Format: RULE → WHY AI
GETS IT WRONG → CORRECT REASONING → EXAMPLE (bad) → COUNTEREXAMPLE (good)
→ VERIFICATION → SOURCE. Uncertainty marked KNOWN / INFERRED / UNVERIFIED.

QEMU/OVMF/edk2 are NOT installed on this host; commands are documented and
marked UNVERIFIED as runs. UEFI semantics are KNOWN from uefi-spec,
edk2-docs. Relative paths assume the skill directory as CWD.

## 1. Boot Services die at ExitBootServices; never touch gBS afterward

- **RULE**: `gBS->ExitBootServices()` marks the end of the Boot Services
  phase. After it returns, the Boot Services function table is not
  available: no `AllocatePool`, no `GetMemoryMap`, no `LocateProtocol`.
  Runtime code may use only Runtime Services (`gRT`) and memory marked
  `EfiRuntimeServicesData`/`EfiRuntimeServicesCode` (with
  `ConvertPointer` applied if relocated).
- **WHY AI GETS IT WRONG**: agents write an "OS loader" and continue
  using familiar `gBS` calls after the exit because the function pointer
  table still exists in memory and often still "works" in an emulator with
  weak stub tables — until real firmware tears it down.
- **CORRECT REASONING**: the transition is a one-way door. All Boot-Service
  work (map capture, protocol discovery, pool allocation) must be done
  BEFORE the call. After the call, only `gRT` + runtime-capable memory.
  Check ordering on EVERY path that crosses the boundary.
- **EXAMPLE** (bad): `examples/bad/boot_services_after_exit.c` — calls
  `GetMemoryMap` and `AllocatePool` (Boot Services) after
  `gRT->ResetSystem` is prepared and after an implied exit.
- **COUNTEREXAMPLE** (good): `examples/good/boot_services_before_exit.c` —
  the memory map is read and allocated while BS is alive;
  `ExitBootServices` is the last `gBS` call.
- **VERIFICATION**: static review of call ordering per path; on-device the
  failure is an immediate abort at the first post-exit `gBS` call.
  UNVERIFIED as a run; the rule is KNOWN.
- **SOURCE**: uefi-spec (Boot Services, the transition);
  edk2-docs (module phases).

## 2. Variable attributes: RUNTIME_ACCESS is required for the OS to read a variable

- **RULE**: `SetVariable` attributes are a bitmask:
  `EFI_VARIABLE_NON_VOLATILE` (persists across reboots),
  `EFI_VARIABLE_BOOTSERVICE_ACCESS` (readable by BS),
  `EFI_VARIABLE_RUNTIME_ACCESS` (readable by Runtime/OS). A variable the
  OS must read after the transition requires RUNTIME_ACCESS; without it,
  `GetVariable` from the OS returns EFI_NOT_FOUND.
- **WHY AI GETS IT WRONG**: agents set only NON_VOLATILE + BOOTSERVICE and
  assume persistence means runtime visibility; the two properties are
  independent.
- **CORRECT REASONING**: enumerate consumers. BS-only variable → the two
  BS-compatible bits. OS-visible variable → all three. The attribute set
  is a contract with the runtime environment.
- **EXAMPLE** (bad): `examples/bad/runtime_flag_missing.c` — a
  "PersistentCounter" the OS should read, set without RUNTIME_ACCESS.
- **COUNTEREXAMPLE** (good): `examples/good/runtime_flag_correct.c` — all
  three bits set.
- **VERIFICATION**: on-device: read the variable from the OS runtime —
  EFI_NOT_FOUND for the bad case. UNVERIFIED as a run; the bit
  requirements are KNOWN.
- **SOURCE**: uefi-spec (variable services, attributes); edk2-docs.

## 3. VFR bounds are a UI contract; the driver must enforce them

- **RULE**: HII/VFR numeric opcodes declare `minimum`/`maximum`/`step`
  for the FORM ENGINE to display and to constrain keyboard entry. The
  stored value is only as good as the driver's validation: the driver's
  save path must clamp or reject out-of-range values because the form
  engine does not guarantee enforcement for all input paths (e.g. program
  write, direct NV storage).
- **WHY AI GETS IT WRONG**: agents read "maximum = 100" in the VFR and
  conclude the value is bounded; they do not check the driver's
  `CheckBox`/numeric `Callback` and save logic.
- **CORRECT REASONING**: the VFR is the visible UI spec; the enforcement
  is in code. Review both: the VFR declares the contract, the driver
  validates it. Flag any path where a stored value can exceed the declared
  range.
- **EXAMPLE** (bad): `examples/bad/vfr_unenforced_bounds.vfr` — the VFR
  says 0..100 but the driver stores raw input without validation.
- **COUNTEREXAMPLE** (good): `examples/good/vfr_validated_bounds.vfr` —
  the VFR declares the range AND the driver validates on save.
- **VERIFICATION**: trace the save path (VFR opcode → driver callback →
  NV storage) for a range check. UNVERIFIED as a run; the split is KNOWN.
- **SOURCE**: uefi-spec (HII/VFR); edk2-docs (VFR compiler, callback
  patterns).

## 4. Phase identity: DXE vs BDS vs Runtime

- **RULE**: the firmware executes in phases with distinct service
  availability: SEC/PEI (pre-DXE, no Boot Services), DXE (Boot Services
  live), BDS (still BS), Runtime (only `gRT`). Code must be written for
  the phase it runs in, and a DXE driver must not rely on BS-only services
  from a Runtime callback.
- **WHY AI GETS IT WRONG**: agents write "firmware" as one undifferentiated
  blob and call any available table without checking which phase the
  caller executes in.
- **CORRECT REASONING**: name the phase at the top of every entry point
  and list allowed services. Runtime callbacks (e.g. `SetVirtualAddressMap`
  hooks, timer callbacks) must not touch `gBS`.
- **EXAMPLE** (bad): a Runtime driver's callback calling
  `gBS->LocateProtocol`.
- **COUNTEREXAMPLE** (good): `examples/good/boot_services_before_exit.c` —
  BS work done in the DXE/BDS entry, `gRT` used for the persistent write.
- **VERIFICATION**: phase is documented per entry; any `gBS` usage in a
  Runtime callback is flagged. UNVERIFIED as a run; the phase model is
  KNOWN.
- **SOURCE**: uefi-spec (PI phase model); edk2-docs.

## 5. ACPI/SMBIOS tables come from the spec, never from memory

- **RULE**: ACPI tables (RSDP, FADT, DSDT/SSDT, MADT, ...) and SMBIOS
  structures (type 0 firmware, type 1 system, type 2 baseboard, ...) have
  exact, spec-defined fields. Hand-writing byte offsets from memory
  produces tables the OS/acpi subsystem rejects; always generate from the
  spec's structure definitions (UEFI spec's ACPI chapter, SMBIOS spec).
- **WHY AI GETS IT WRONG**: agents write a struct with guessed padding or
  offset to "match what the OS expects", and the OS fails to parse the
  table with no obvious error at the driver level.
- **CORRECT REASONING**: derive every field from the published structure
  (name, offset, size, meaning). Cross-check with `acpidump`/`iasl` output
  during bring-up.
- **EXAMPLE** (bad): inventing a FADT field order from memory.
- **COUNTEREXAMPLE** (good): generating the tables from the spec's struct
  definitions and validating with the ACPI tooling.
- **VERIFICATION**: `acpidump -o acpi.dat && iasl -d acpi.dat` decodes and
  validates the tables (documented; iasl/acpidump not installed here).
  UNVERIFIED as a run.
- **SOURCE**: uefi-spec (ACPI integration); edk2-docs (table publishing).

## 6. Secure Boot is a signature database discipline, not a flag

- **RULE**: Secure Boot enforces a chain: each image must be signed by a
  key whose hash is in `db` (or an authenticated update via `dbx`
  revocations applied first, with `KEK`/`PK` controlling enrollment).
  The verification order (dbx → db → KEK/PK) and the signature algorithms
  are normative. Disabling enforcement is a policy decision, never a
  "make it boot" workaround.
- **WHY AI GETS IT WRONG**: agents model Secure Boot as "a boolean to
  enable" and ignore the database semantics; they also frequently
  misinterpret which variable holds the platform key.
- **CORRECT REASONING**: the databases (db/dbx/KEK/PK) are policy data
  signed by higher-authority keys. Verification checks revocation first,
  then signed images against db. All four variables participate;
  enrollment of new keys requires the PK/KEK signature.
- **EXAMPLE** (bad): a driver that disables Secure Boot enforcement to
  get an unsigned image to load and calls it "debug".
- **COUNTEREXAMPLE** (good): an image signed by a db key; enrollment done
  through the proper authenticated variable flow.
- **VERIFICATION**: OVMF with Secure Boot enabled rejects an unsigned
  image (serial log shows the failure); the signed image boots.
  UNVERIFIED as a run (no OVMF); the database model is KNOWN.
- **SOURCE**: uefi-spec (Secure Boot, signature databases); edk2-docs.

## 7. Debug firmware over serial with QEMU + OVMF

- **RULE**: firmware failures are diagnosed via the DEBUG serial log:
  run `qemu-system-x86_64 -bios OVMF.fd -serial stdio`, read the edk2
  DEBUG lines, and correlate the last successful phase with the failure.
  The serial log is the primary visibility tool for a headless firmware
  image.
- **WHY AI GETS IT WRONG**: agents debug firmware by reading source and
  guessing, because they cannot see the runtime — and they don't set up
  the serial capture that would show the ASSERT.
- **CORRECT REASONING**: reproduce in the simulator, capture the log, and
  find the phase boundary where execution stops or ASSERTS. Then trace the
  services that path uses (rule 1/4).
- **EXAMPLE** (bad): a DXE driver that crashes with no obvious source-level
  reason — the serial log shows the ASSERT at the post-exit `gBS` call.
- **COUNTEREXAMPLE** (good): the good fixtures' flow — serial log shows a
  clean transition through the expected phases.
- **VERIFICATION**: the documented qemu command with `-serial`; the log is
  the artifact. UNVERIFIED as a run (qemu absent); the technique is KNOWN.
- **SOURCE**: qemu-docs (system emulation, -bios, -serial); edk2-docs
  (DEBUG macros).

## Quick reference table

| Topic | Rule in one line |
|---|---|
| ExitBootServices | one-way door: no gBS calls afterward, only gRT + runtime memory |
| variables | NON_VOLATILE + BOOTSERVICE_ACCESS + RUNTIME_ACCESS as needed by consumers |
| VFR bounds | form engine displays; driver must validate on save |
| phases | SEC/PEI (no BS), DXE/BDS (BS live), Runtime (RT only) |
| ACPI/SMBIOS | generate from spec structs; validate with acpidump/iasl |
| Secure Boot | dbx revoke first, then db; KEK/PK for enrollment; not a flag |
| debugging | QEMU + OVMF + `-serial` log is the primary visibility tool |
