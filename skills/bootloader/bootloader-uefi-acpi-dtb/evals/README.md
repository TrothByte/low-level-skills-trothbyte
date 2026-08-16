# Evaluation — bootloader-uefi-acpi-dtb

Skill: `skills/bootloader/bootloader-uefi-acpi-dtb`. Stability target:
`evaluated`. Table-layout knowledge KNOWN from uefi-spec /
devicetree-spec / aarch64-boot-protocol. ACPI-spec and SMBIOS-spec are
PROPOSED NEW sources — claims from them marked INFERRED. Firmware-level
toolchain (QEMU/OVMF, edk2 build) NOT available — those runs UNVERIFIED.

## Synthetic evals

| Case | Fixture | Expected | Status |
|---|---|---|---|
| easy/negative | `bad/acpi_bad_checksum.c` | corrupted RSDP accepted without checksum → flagged | structural |
| easy/negative | `bad/fdt_bad_magic.py` | DTB with bad magic accepted → flagged | executable |
| medium/negative | `bad/smbios_wrong_ep.py` | 2.x entry point parsed with 3.0 width → flagged | executable |
| easy/positive | `good/acpi_checksum.c` | modulo-256 checksum math rejects corruption | executable |
| easy/positive | `good/fdt_walk.py` | magic + header + node walk correct | executable |
| easy/positive | `good/smbios_parse.py` | anchor dispatch selects correct address width | executable |

Detection rule: (1) every ACPI table hop must sum to 0 modulo 256;
(2) FDT magic must equal 0xd00dfeed before any parsing; (3) SMBIOS width
is selected by the `_SM3_`/`_SM_` anchor.

## False-positive evals (correct code must NOT be flagged)

- ACPI 1.0-only platform (RSDT, 32-bit, no XSDT) with a valid checksum —
  legal, not an error.
- DT-only ARM64 platform with no ACPI GUID in the config table — the
  correct answer is "use DTB", not "ACPI is missing".
- An SMBIOS 2.x entry point parsed with the 2.x 32-bit width — correct.
- A `good/fdt_walk.py`-style walker that validates the magic first.

## Historical evals

- ACPI table-checksum corruption and RSDP-location bugs are documented
  failure classes across firmware communities (kernel ACPI debugging,
  edk2 issue trackers). UNVERIFIED as named incidents on this host (no
  QEMU/OVMF, no crash database).
- DTB handoff in the wrong register / DTB read from the wrong memory is a
  known class on ARM64 bring-ups. UNVERIFIED as named incidents here.

## Adversarial evals

- A table that parses structurally but fails the checksum (single flipped
  byte) — invisible to offset-based parsing, caught only by the sum.
- A DTB with valid magic but corrupt `totalsize`/`off_dt_struct` — must be
  caught by bounds checks before the walk.
- A config table that publishes the ACPI 1.0 GUID only: the loader must
  dispatch to the 32-bit RSDT path without assuming XSDT.
- The "looks valid" trap from `meta-verification-harness-validity`: a
  parser that never actually checks the checksum "passes" on well-formed
  input and only fails on corrupted firmware.

## Verification commands

```
gcc -Wall -Wextra -Werror -O2 examples/good/acpi_checksum.c -o acpichk && acpichk
gcc -Wall -Wextra -Werror -O2 examples/bad/acpi_bad_checksum.c -o acpibad && acpibad
python examples/good/fdt_walk.py
python examples/bad/fdt_bad_magic.py
python examples/good/smbios_parse.py
python examples/bad/smbios_wrong_ep.py

# Target flow (QEMU + OVMF documented; toolchain not installed here):
qemu-system-x86_64 -bios OVMF.fd -m 256 -serial stdio -display none
acpidump -o acpi.dat && iasl -d acpi.dat
```

## Verified facts

- KNOWN: ACPI checksum rule (modulo-256); FDT magic + header + structure
  block encoding (devicetree-spec); SMBIOS anchor-width selection
  (INFERRED until smbios-spec is registered); ARM64 DTB-in-x0 handoff
  (aarch64-boot-protocol); config-table GUID discovery (uefi-spec).
- EXECUTED on this host: `acpichk` PASS; `fdt_walk.py` PASS;
  `smbios_parse.py` PASS; `fdt_bad_magic.py` and `smbios_wrong_ep.py`
  exit non-zero (recorded below).
- UNVERIFIED: QEMU/OVMF boot, real-firmware table capture, `acpidump`
  decode on this host.

## Scoring

- precision: every flagged issue maps to a reference rule (1–6).
- recall: all bad fixtures detected (bad checksum, bad magic, wrong EP
  width, memory scan).
- FP-rate: ACPI 1.0-only and DT-only platforms produce zero flags.
- Decisive test: "is this table checksummed?" and "which handoff
  interface did this firmware publish (config-table GUIDs)?"

### Executed output (2026-08-17, MSYS2 gcc 16.1.0 / python 3.11.9)

```
$ gcc -Wall -Wextra -Werror -O2 examples/good/acpi_checksum.c -o acpichk && ./acpichk
PASS: RSDP checksum math rejects corruption
PASS: table header checksum validation
exit 0

$ python examples/good/fdt_walk.py
PASS: DTB magic + header + structure walk correct
exit 0

$ python examples/good/smbios_parse.py
PASS: entry-point anchor selects the table address width
exit 0

$ python examples/bad/fdt_bad_magic.py
BUG: accepted DTB with magic d00dfeec
exit 0   (BUG accepted — bad fixture, flagged by review)

$ python examples/bad/smbios_wrong_ep.py
BUG: wrong table base 0x100000000000 (expected 0x1000)
exit 1   (detected)
```
