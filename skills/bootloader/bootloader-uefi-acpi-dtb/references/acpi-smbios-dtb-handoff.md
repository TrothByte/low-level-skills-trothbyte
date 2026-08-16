# Bootloader: UEFI ACPI / SMBIOS / DTB Handoff — Reference Rules

Knowledge layer for `bootloader-uefi-acpi-dtb`. Format: RULE → WHY AI
GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE →
VERIFICATION → SOURCE. Uncertainty marked KNOWN / INFERRED / UNVERIFIED.
The ACPI checksum math and FDT walker were executed on this host (gcc,
python); firmware-level runs (QEMU/OVMF) are UNVERIFIED. Relative paths
assume the skill directory as CWD.

## 1. Under UEFI, find tables via the EFI System Table ConfigurationTable, not by scanning memory

- **RULE**: In a UEFI bootloader the canonical way to locate ACPI tables
  is the EFI System Table's `ConfigurationTable` array, matched against
  the standard GUIDs: `EFI_ACPI_20_TABLE_GUID` (XSDT, ACPI 2.0+) and
  `EFI_ACPI_TABLE_GUID` (RSDT, ACPI 1.0). Legacy BIOS-style scanning of
  the EBDA / top-of-low-memory is not reliable under UEFI.
- **WHY AI GETS IT WRONG**: agents remember "find RSDP by scanning
  0xE0000–0xFFFFF" from BIOS-era documentation and apply it to UEFI
  loaders, where the firmware owns that memory region.
- **CORRECT REASONING**: UEFI publishes discovery via GUID-keyed
  configuration-table entries. Enumerate `gST->ConfigurationTable`,
  match the ACPI 2.0 GUID, dereference the entry pointer as the XSDT.
- **EXAMPLE** (bad): `examples/bad/acpi_scan_memory.c` — scanning
  physical memory for "RSD PTR " inside a UEFI loader.
- **COUNTEREXAMPLE** (good): `examples/good/acpi_checksum.c` — the table
  walk is driven from a config-table-provided address, with checksum
  validation at every hop.
- **VERIFICATION**: compare the entry enumeration in a real OVMF boot
  (UNVERIFIED here). The GUID-keyed lookup rule is KNOWN.
- **SOURCE**: uefi-spec (EFI System Table, ConfigurationTable GUIDs);
  acpi-spec (INFERRED for RSDP scan-legacy claim, verify).

## 2. Every ACPI table and the RSDP must pass a modulo-256 checksum

- **RULE**: ACPI RSDP (`"RSD PTR "` signature) and every table header
  carry a checksum field such that the sum of all bytes of the structure
  is 0 modulo 256 (checksum == (0 - sum(other bytes)) & 0xFF). A corrupt
  table is undefined behavior for consumers; validate before parsing.
- **WHY AI GETS IT WRONG**: agents parse offsets from a remembered
  layout and never sum the bytes; a single flipped byte yields fields the
  OS trusts and crashes on.
- **CORRECT REASONING**: byte-sum validation is O(n) and mandatory. If
  checksum fails, the pointer is garbage — refuse the table.
- **EXAMPLE** (bad): `examples/bad/acpi_bad_checksum.c` — accepts a table
  whose header checksum is wrong.
- **COUNTEREXAMPLE** (good): `examples/good/acpi_checksum.c` — sums bytes
  and rejects on mismatch.
- **VERIFICATION**: `gcc ... acpi_checksum.c; acpichk` — PASS output
  recorded in evals/README.md (executed on this host).
- **SOURCE**: acpi-spec (RSDP/XSDT/table header checksum rules);
  uefi-spec (ACPI integration). acpi-spec is proposed NEW, INFERRED until
  registered; the modulo-256 rule is KNOWN.

## 3. ACPI 2.0 XSDT (64-bit) is preferred over the legacy 32-bit RSDT

- **RULE**: ACPI 2.0+ exposes the XSDT (Extended System Description
  Table), an array of 64-bit physical addresses of tables. It supersedes
  the 32-bit RSDT. A loader must honor the XSDT when the ACPI 2.0 GUID is
  present; the RSDT is the ACPI 1.0 fallback.
- **WHY AI GETS IT WRONG**: agents hard-code 32-bit RSDT parsing and
  truncate XSDT entries, or read the XSDT when only an RSDT exists.
- **CORRECT REASONING**: read the revision in the RSDP (`revision >= 2` →
  XSDT), parse 64-bit entries, validate each table's checksum, and only
  then descend to FADT/MADT/DSDT.
- **EXAMPLE** (bad): treating an XSDT entry array as 32-bit entries.
- **COUNTEREXAMPLE** (good): revision dispatch — XSDT for rev 2+, RSDT
  otherwise, both checksum-validated.
- **VERIFICATION**: field-width dispatch is static structure work;
  executable checksum logic in the good fixture. UNVERIFIED against real
  firmware tables on this host.
- **SOURCE**: acpi-spec (table descriptor layout); uefi-spec.

## 4. SMBIOS: entry-point anchor selects the table address width

- **RULE**: SMBIOS 3.0 entry point starts with `_SM3_` and carries a
  64-bit table address; the 2.x entry point starts with `_SM_` and carries
  a 32-bit table address in the middle struct. The loader must parse the
  correct entry point and walk structures (Type/Length/Handle) until Type
  127 (end of table).
- **WHY AI GETS IT WRONG**: agents copy a 32-bit `tableAddress` layout
  and truncate the SMBIOS 3.0 64-bit address, or read a wrong offset.
- **CORRECT REASONING**: match the anchor bytes, read the documented
  fields, use the 64-bit address for `_SM3_`, walk Type/Length/Handle.
- **EXAMPLE** (bad): `examples/bad/smbios_wrong_ep.py` — parses the 2.x
  entry point but reads the 64-bit address field, yielding a garbage
  table base.
- **COUNTEREXAMPLE** (good): `examples/good/smbios_parse.py` — dispatches
  on anchor and walks the structure table correctly.
- **VERIFICATION**: `python examples/bad/smbios_wrong_ep.py` must exit
  non-zero; `python examples/good/smbios_parse.py` prints PASS (executed).
- **SOURCE**: smbios-spec (proposed NEW source; entry-point formats) —
  INFERRED until registered; the anchor/width rule is KNOWN.

## 5. Device Tree (FDT): validate magic/header, then walk nodes with alignment

- **RULE**: The Flattened Device Tree (DTB) starts with a 32-bit magic
  `0xd00dfeed`; the header contains `totalsize`, `off_dt_struct`,
  `off_dt_strings`, `off_mem_rsvmap`, and version fields. The structure
  block is a sequence of `FDT_BEGIN_NODE` (0x1), `FDT_END_NODE` (0x2),
  `FDT_PROP` (0x3), `FDT_NOP` (0x4), terminated by `FDT_END` (0x9).
  Property strings are aligned to 4 bytes, node/string data to 8 bytes in
  the structure block.
- **WHY AI GETS IT WRONG**: agents hard-code node-offset arithmetic from
  memory (e.g. guessing property alignment) and produce a walker that
  silently misparses; or they accept a DTB with a bad magic.
- **CORRECT REASONING**: check magic, then use the header offsets
  (`off_dt_struct`, `off_dt_strings`) — never guess; each `FDT_PROP`
  entry references a string from the string block by offset and stores
  its value with the spec'd alignment padding.
- **EXAMPLE** (bad): `examples/bad/fdt_bad_magic.py` — walks a DTB whose
  magic byte is flipped.
- **COUNTEREXAMPLE** (good): `examples/good/fdt_walk.py` — builds a
  synthetic DTB and walks it, asserting node names and property values.
- **VERIFICATION**: `python examples/bad/fdt_bad_magic.py` must fail;
  `python examples/good/fdt_walk.py` prints PASS (executed on this host).
- **SOURCE**: devicetree-spec (FDT header and structure-block encoding);
  aarch64-boot-protocol (handoff register).

## 6. ARM64: hand the DTB in x0 with x1=0 and x2=0

- **RULE**: The Linux AArch64 boot protocol passes the DTB physical
  address in x0, with x1=0 and x2=0, and requires the DTB to be 8-byte
  aligned; booting without a DTB (or with the wrong register) fails
  early. EFI-on-ARM64 firmwares may publish the DTB via the standard
  ACPI 2.0 GUID (server) or expose a DT; the bootloader must know which
  one the platform uses.
- **WHY AI GETS IT WRONG**: agents write "jump to entry with a DTB" but
  put the pointer in a convenient register (e.g. x1) that the kernel
  ignores.
- **CORRECT REASONING**: the DTB register is part of the architecture
  boot contract, not a convention: x0=DTB, x1=0, x2=0, and the DTB must
  be aligned and in memory accessible to the kernel.
- **EXAMPLE** (bad): passing the DTB in x2 with x0=x1=0.
- **COUNTEREXAMPLE** (good): a boot stub that loads x0 with the DTB
  physical address and zeroes x1/x2 before the kernel entry.
- **VERIFICATION**: UNVERIFIED as a run (no ARM64 toolchain/QEMU here);
  the register contract is KNOWN from aarch64-boot-protocol.
- **SOURCE**: aarch64-boot-protocol (boot protocol requirements).

## Quick reference table

| Topic | Rule in one line |
|---|---|
| finding ACPI under UEFI | config table GUIDs (ACPI 2.0 → XSDT), not memory scan |
| table validation | modulo-256 checksum at RSDP and every header |
| XSDT vs RSDT | RSDP revision >= 2 → 64-bit XSDT; else 32-bit RSDT |
| SMBIOS entry points | `_SM3_` 64-bit / `_SM_` 32-bit; anchor selects width |
| DTB walk | magic 0xd00dfeed, header offsets, 4/8-byte alignment |
| ARM64 DTB handoff | x0=DTB, x1=0, x2=0, 8-byte aligned |
