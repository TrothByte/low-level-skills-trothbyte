---
name: bootloader-uefi-acpi-dtb
description: Use when a UEFI bootloader must hand off system-description data: locating ACPI tables (RSDP/XSDT/MADT), SMBIOS structures, or a Flattened Device Tree (DTB), and deciding which interface the platform exposes. Teaches table parsing, checksum validation, and architecture-dependent firmware-handoff conventions.
---
# Bootloader: UEFI ACPI, SMBIOS, and Device Tree Handoff

## When to use

- Writing or reviewing a UEFI bootloader/OS loader that must find ACPI
  tables, SMBIOS structures, or a DTB and pass them to the next stage.
- Deciding whether a target platform hands off via ACPI, SMBIOS, or a
  Flattened Device Tree (FDT/DTB) — and reading the right configuration
  table entries.
- Parsing and validating an RSDP/XSDT header chain or SMBIOS entry point
  from raw firmware memory.
- Writing the DTB pass-through code for an ARM64 boot (x0 handoff) or a
  legacy x86 `boot_params`/EFI-stub handoff.

## When not to use

- General UEFI lifecycle (Boot Services, phases, variables, Secure Boot)
  — use `bootloader-uefi-firmware`.
- The pre-UEFI power-on path (SEC/PEI) — use `bootloader-stages`.
- Authoring DTS sources or devicetree bindings — use
  `embedded-device-tree-and-kconfig`.
- The kernel-side ACPI subsystem or AML interpreter internals — that is
  kernel territory, not bootloader handoff.

## What the agent often gets wrong

- Locating ACPI tables by scanning EBDA/memory from a UEFI loader. In
  UEFI the canonical path is the EFI System Table `ConfigurationTable`
  (ACPI 2.0 GUID for XSDT, ACPI 1.0 GUID for RSDT); raw-memory scanning
  is legacy-BIOS behavior and fails under UEFI.
- Trusting a table header without validating its checksum: every ACPI
  table header (and the RSDP) carries a checksum (all bytes sum to 0
  modulo 256); a corrupted table accepted blindly produces OS-visible
  crashes. This is the ACPI analog of the UEFI "looks fine" trap.
- Assuming ACPI is universal: ARM64/ARM servers and many SoCs expose a
  DTB, not ACPI. Reading an XSDT on a DT-only platform reads garbage.
- Copying SMBIOS entry-point offsets from memory instead of parsing the
  SMBIOS 3.0 entry point (64-bit table address) vs the 32-bit legacy
  entry point — table address width differs.
- Passing a DTB pointer in the wrong register: ARM64 boot protocol puts
  the DTB in x0 (with x1=0, x2=0); putting it in a different register
  makes the kernel fail to parse it silently.

## How to reason correctly

1. **Identify the handoff interface first**: on UEFI x86, ACPI is
   standard (XSDT via the ACPI 2.0 configuration-table GUID) and SMBIOS is
   standard (via its entry-point GUID). On ARM64 UEFI, both ACPI (on
   server platforms) and a DTB (SoC platforms) are legal — check which
   configuration-table entries the firmware actually published, then
   choose ACPI if present, else DTB. Never assume.
2. **Walk the table chain from the spec, not memory**: RSDP ("RSD PTR ")
   → XSDT (64-bit entry array; prefer over RSDT) → table header
   (Signature, Length, Revision, Checksum) → parse fields from the
   spec-defined structure. Validate the checksum at each hop.
3. **SMBIOS**: parse the entry point (anchor `_SM3_` for SMBIOS 3.0,
   `_SM_` for 2.x), read the table address (64-bit for 3.0), walk
   structures via their Type/Length/Handle headers, stop at the end-of-table
   (Type 127).
4. **DTB**: validate the FDT magic `0xd00dfeed`, then read
   `off_dt_struct`, `off_dt_strings`, `totalsize`, `off_mem_rsvmap` from
   the header; walk nodes (`FDT_BEGIN_NODE` 0x1, `FDT_PROP` 0x3,
   `FDT_END_NODE` 0x2, `FDT_END` 0x9) with 8-byte property alignment.
   Pass the DTB to the kernel per the architecture boot protocol.
5. **When in doubt, enumerate the configuration table** (`gRT`/`gST`
   `ConfigurationTable`) and log every GUID; the presence of the ACPI 2.0
   GUID is the single most reliable "which one does the firmware expose"
   signal under UEFI.

## What to verify

- The RSDP signature, checksum, revision, and the XSDT/RSDT address it
  points to; every table header checksum sums to 0 modulo 256.
- ACPI 2.0 GUID (`EFI_ACPI_20_TABLE_GUID`) is present in the config table
  before any ACPI read is attempted.
- SMBIOS entry point chosen matches the table address width (32 vs 64 bit).
- DTB magic/version/totalsize are sane before walking the node stream.
- The handoff register/argument matches the target boot protocol (ARM64:
  DTB in x0; x86: `boot_params` or multiboot EBX/EAX magic).

## How to verify

```
# Host-verifiable: table-chain and DTB parser logic (real runs on this host)
gcc -Wall -Wextra -Werror -O2 examples/good/acpi_checksum.c -o acpichk
acpichk                          # validates RSDP checksum math, prints PASS
python examples/good/fdt_walk.py # builds + parses a synthetic DTB, prints PASS
python examples/bad/fdt_bad_magic.py   # must FAIL the magic check (exit != 0)
python examples/bad/smbios_wrong_ep.py # must FAIL the entry-point check

# Target (documented; no QEMU/OVMF here):
qemu-system-x86_64 -bios OVMF.fd -m 256 -serial stdio -display none
#   then: acpidump -o acpi.dat && iasl -d acpi.dat   (decode real tables)
```

The checksum math and FDT-walk logic are host-verifiable (ran on this
host, output recorded in `evals/README.md`); real-firmware runs need
QEMU/OVMF, marked UNVERIFIED.

## Where the knowledge comes from

- `uefi-spec` (ACPI/SMBIOS config-table integration) — locating tables.
- `devicetree-spec` (FDT structure, node/property encoding).
- `aarch64-boot-protocol` (DTB in x0, x1=0, x2=0).
- `acpi-spec` (proposed NEW source; RSDP/XSDT/checksum layout) —
  INFERRED, verify before stable.
- `qemu-docs` (OVMF serial boot for target verification).

## Related skills

- `bootloader-uefi-firmware` (extend; this skill adds table handoff).
- `bootloader-stages` (require; power-on → SEC/PEI context).
- `embedded-device-tree-and-kconfig` (recommend; DTS/binding authoring).
- `qemu-system-setup` (recommend; OVMF verification harness).
- `meta-verification-harness-validity` (recommend; the checksummed table
  is only as trustworthy as the harness that validates it).

## Evaluation

- Synthetic: `bad/acpi_bad_checksum.c` and `bad/fdt_bad_magic.py` must be
  rejected; `good/acpi_checksum.c` and `good/fdt_walk.py` must pass.
- False-positive: a single-ACPICorrect RSDT-only system (ACPI 1.0, 32-bit
  RSDT, no XSDT) must NOT be flagged; DT-only ARM64 must not be flagged
  for "missing ACPI".
- Historical: ACPI table-checksum corruption and wrong-handoff-register
  bugs documented in uefi-spec/devicetree-spec communities (UNVERIFIED as
  named incidents on this host).
- Adversarial: a table that parses but fails checksum, or a DTB with
  valid magic but corrupt internals, must be caught — the "looks valid"
  trap.
- Verified facts and commands: `evals/README.md`.
