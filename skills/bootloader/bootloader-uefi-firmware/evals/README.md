# Evaluation — bootloader-uefi-firmware

Skill: `skills/bootloader/bootloader-uefi-firmware`. Stability target:
`evaluated`. UEFI semantics KNOWN from uefi-spec/edk2-docs. Toolchain
(QEMU/OVMF/edk2 build) NOT available on this host — tool runs marked
UNVERIFIED.

## Synthetic evals

| Case | Fixture | Expected | Status |
|---|---|---|---|
| easy/negative | `bad/boot_services_after_exit.c` | gBS usage after ExitBootServices flagged | structural |
| easy/negative | `bad/runtime_flag_missing.c` | missing RUNTIME_ACCESS flagged | structural |
| medium/negative | `bad/vfr_unenforced_bounds.vfr` | VFR bounds not enforced by driver flagged | structural |
| easy/positive | `good/boot_services_before_exit.c` | BS work before exit, gRT after | structural |
| easy/positive | `good/runtime_flag_correct.c` | all three variable attributes present | structural |
| medium/positive | `good/vfr_validated_bounds.vfr` | bounds declared AND driver-validated | structural |

Detection rule: for each code path crossing ExitBootServices, list every
service used on each side and reject any gBS call after the exit. For each
variable, list its consumers and require the matching attribute bits. For
each VFR numeric, trace the save path for a range check.

## False-positive evals (correct code must NOT be flagged)

- `good/runtime_flag_correct.c` — all three bits set correctly.
- A variable that is ONLY used during Boot Services (no OS consumer) with
  BOOTSERVICE_ACCESS and no RUNTIME_ACCESS is CORRECT.
- `good/vfr_validated_bounds.vfr` — bounds declared AND driver-validated;
  not flagged.
- Secure Boot disabled for a legitimate development/unsigned-target build
  with a documented policy decision is not automatically "wrong" — the
  decision must be explicit.

## Historical evals

- UEFI phase-transition bugs (post-ExitBootServices gBS use) and
  variable-attribute errors are documented failure classes in uefi-spec
  and edk2 community reports. UNVERIFIED as named incidents on this host
  (no QEMU/OVMF, no crash database).

## Adversarial evals

- `bad/boot_services_after_exit.c`: in a stub/emulated environment with
  permissive tables the post-exit gBS calls may "work" and the bug is
  invisible until real firmware tears the table down — exactly the
  unconditional-pass trap from `meta-verification-harness-validity`. The
  serial log on real OVMF shows the abort; on this host the ordering rule
  is the only detector.
- `bad/runtime_flag_missing.c`: the variable is set successfully in BS;
  the failure (EFI_NOT_FOUND) appears only after the OS takes over —
  invisible to a boot-time unit test.
- `bad/vfr_unenforced_bounds.vfr`: the VFR looks complete (bounds are
  declared); the defect is the missing driver-side validation, which a
  reviewer only sees by reading the save path.

## Verification commands (RESEARCHED, toolchain not available)

```
qemu-system-x86_64 -bios OVMF.fd -m 256 -serial stdio -display none
build -a X64 -t GCC5 -p OvmfPkg/OvmfPkgX64.dsc
acpidump -o acpi.dat && iasl -d acpi.dat
```

QEMU/OVMF/edk2 toolchain not installed (Windows MSYS2). The commands are
the documented verification flow (qemu-docs, edk2-docs). The bad files
carry the `// intentionally incorrect` (c) or `// intentionally
incorrect` (vfr) marker.

## Verified facts

- KNOWN: ExitBootServices is a one-way transition (no gBS after);
  variable attribute bits and their consumer mapping; VFR is a UI
  contract with driver-side enforcement; phase service availability;
  ACPI/SMBIOS spec-defined fields; Secure Boot signature-database model.
  Sources: uefi-spec, edk2-docs, qemu-docs.
- UNVERIFIED (toolchain absent): QEMU+OVMF boot logs, edk2 build, ACPI
  decode on this host.

## Scoring

- precision: every flagged issue maps to a reference rule (1-7).
- recall: all three bad fixtures detected (post-exit gBS, missing
  RUNTIME_ACCESS, unenforced VFR bounds).
- FP-rate: the three good fixtures and the legal variants produce zero
  flags.
- Decisive test: "after ExitBootServices, which services does this path
  touch?" and "which consumers read this variable, and are the attribute
  bits sufficient for them?"
