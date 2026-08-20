# PCIe Config Space Reference

Status: KNOWN (PCI Local Bus rev 3.0 + PCIe spec facts), VERIFIED for layout
math on host models (2026-08-20); target-hardware behavior UNVERIFIED on this
host.

## Address space overview

- Legacy config space: 256 bytes, 0x00-0xFF. Required for all PCI and PCIe
  devices. Accessible via config I/O ports 0xCF8/0xCFC on x86, via the Linux
  sysfs `config` file (size depends on the platform), and via
  `pci_bus_read_config_*`.
- Extended config space: 0x100-0xFFF (another 3840 bytes), only defined for
  PCIe. Holds PCIe extended capabilities: AER (0x0001), VC (0x0002), SERR
  (0x0003), Power Budgeting (0x0004), RC Link Declaration (0x0005), RC Internal
  Link (0x0006), RC Event Collector (0x0007), MFVC (0x0008), VC2 (0x0009), RCRB
  (0x000A), Vendor (0x000B), ACS (0x000D), ARI (0x000E), ACS2 (0x0010), SR-IOV
  (0x0010), LTR (0x0018), and others.
- Access: ECAM (memory-mapped, 4KB per function, the PCIe standard mechanism,
  exposed as MMCONFIG on x86) or legacy config I/O for the first 256 bytes.

## Header layout (type 0, endpoint)

Offsets (all little-endian, byte addresses):

| Offset | Size | Field |
|---|---|---|
| 0x00 | 2 | Vendor ID |
| 0x02 | 2 | Device ID |
| 0x04 | 2 | Command |
| 0x06 | 2 | Status |
| 0x08 | 1 | Revision ID |
| 0x09 | 3 | Class Code: prog-if @0x09, subclass @0x0A, base class @0x0B |
| 0x0C | 1 | Cache Line Size |
| 0x0D | 1 | Latency Timer |
| 0x0E | 1 | Header Type (bit 7 multi-function; 0=type0, 1=type1) |
| 0x0F | 1 | BIST |
| 0x10-0x27 | 6x4 | Base Address Registers (BARs) |
| 0x28 | 4 | Cardbus CIS Pointer |
| 0x2C | 2 | Subsystem Vendor ID |
| 0x2E | 2 | Subsystem Device ID |
| 0x30 | 4 | Expansion ROM BAR |
| 0x34 | 1 | Capabilities Pointer (type 0/1) |
| 0x38 | 4 | Reserved |
| 0x3C | 1 | Interrupt Line |
| 0x3D | 1 | Interrupt Pin |
| 0x3E-0x3F | 2 | Min Grant / Max Latency (type 0) |

Type 1 (bridge) header: BARs 0-1, Primary/Secondary/Subordinate bus numbers at
0x18-0x1A, Secondary Latency Timer at 0x1B, I/O base/limit 0x1C-0x1D, Memory
base/limit 0x20-0x21, etc. The Capabilities Pointer is still at 0x34.

## Class code

Three bytes at 0x09-0x0B (little-endian byte order): base class in the HIGH
byte of a 32-bit load. Example: a network controller has base class 0x02 at
0x0B, subclass 0x00 at 0x0A, prog-if 0x00 at 0x09.

## Capability linked list

- The Capabilities Pointer at 0x34 (type 0/1) gives the byte offset of the
  first capability; 0x00 means none.
- Each capability: byte[0] = capability ID, byte[1] = next pointer (offset of
  the next capability in legacy space, 0x00 = end of list).
- Common IDs: 0x01 PM, 0x03 VPD, 0x05 MSI, 0x06 PCI-X, 0x07 AGP, 0x09 slot
  numbering, 0x0A MSI-X (renumbered later to 0x11 in PCIe; OSDev documents
  the history), 0x10 PCIe, 0x11 MSI-X, 0x12 SATA, 0x19 LTR... The ID space is
  crowded; always decode from the spec table, not from memory.
- Extended capabilities (0x100-0xFFF): header is 4 bytes — capability ID
  (16 bits), version (4 bits), next capability pointer (12 bits, in units of 4
  bytes). Walking extended caps starts from the PCIe capability (ID 0x10)
  in legacy space, whose next pointer indexes into extended space.

## BAR semantics

- Bit 0: space — 0 = memory, 1 = I/O.
- Memory BAR:
  - Bits 1-2: type — 0x0 = 32-bit, 0x2 = 20-bit (obsolete), 0x3 = 64-bit.
  - Bit 3: prefetchable.
  - Bits 4-31: base address. For 64-bit, the next dword holds the upper 32
    bits.
- I/O BAR: bit 0 = 1, bits 1-31 base address (all 32 bits usable in the
  32-bit I/O space).

### BAR size probing

Procedure (classic, from PCI Local Bus 3.0):
1. Read and save the current BAR value.
2. Write 0xFFFFFFFF (both dwords for a 64-bit BAR).
3. Read back; the writable address bits come back 1, the fixed attribute bits
   and the size-determined low bits come back 0.
4. Mask off the attribute bits (0xF for memory BARs: space, type, prefetch;
   for I/O BARs mask 0x3).
5. Size = (~masked) + 1, in bytes; equivalently the low set bit position of
   the masked value gives the size.
6. Restore the saved value.

Example: 16 MiB memory BAR -> writing 0xFFFFFFFF reads back 0xFF000000,
masked 0xFF000000, size = 0x01000000. A 64-bit 256 GiB BAR reads back two
dwords; the combined 64-bit masked value yields 0x4000000000.

## MSI capability (ID 0x05)

- Byte 0: ID 0x05. Byte 1: next. Word 2: Message Control — bit 0 MSI enable,
  bit 1 per-vector masking capable, bits 4-6 multiple message capable,
  bits 7-9 multiple message enable, bit 7 (word bit 15) 64-bit capable
  (ordering per spec: bit 15 = 64-bit capable in MSI control).
- If 64-bit capable: dword 3 = Message Address (low), dword 4 = Message
  Address (high), word 5 = Message Data. 32-bit variant: dword 3 address,
  word 4 data.

## MSI-X capability (ID 0x11)

- Word 2: Message Control — bit 15 enable, bit 14 function mask, bits 0-10
  Table Size (N-1, number of entries minus one).
- Dword 3: Table Offset/BIR — bits 31-3 table offset (in the BAR selected by
  BIR), bits 2-0 BIR (which BAR, 0-5).
- Dword 4: PBA Offset/BIR — same encoding for the Pending Bit Array.
- Alignment invariants: the table offset must be 8-byte aligned (each table
  entry is 16 bytes: 8 address + 4 data + 4 vector control); the table region
  must be page-aligned (the spec requires the table within a BAR and the table
  not cross a page boundary — in practice 4KB page alignment is enforced by
  Linux's msix_table_size check and the BAR alignment).

## Access methods on Linux

- `lspci -vv` / `lspci -vxxx` — verbose capability dump (legacy space mostly).
- `setpci -s <bdf> <reg>` — raw config access with expressions like
  `CAP_EXP+10.b`; use `.b/.w/.l` for byte/word/long widths.
- `/sys/bus/pci/devices/<bdf>/config` — the config file; its size is the
  platform's config space size (often 256 bytes on legacy, 4096 with ECAM).
- Kernel: `pci_read_config_byte/word/dword`, `pci_bus_read_config_*`,
  `pci_resource_start`, and the `pci_select_bars`/`pci_request_region` helpers.
- ECAM note: x86-64 platforms use MMCONFIG/ECAM; the kernel must map the ECAM
  region. On ARM, the host bridge driver provides the config ops. If extended
  caps are missing, the access method (not the device) is suspect.

## bdf encoding

bus:device.function — bus is 8 bits, device 5 bits (0-31), function 3 bits
(0-7). Linux encodes as (bus << 8) | (device << 3) | function in its internal
`struct pci_bus`/devfn, but the `dd`/sysfs paths use the textual
`0000:00:1f.2` form.

## Common pitfalls summary

- Reading a full 4KB through legacy config I/O: 0xCF8/0xCFC only addresses the
  first 256 bytes; extended reads need ECAM.
- Fixed-offset capability reads: the spec allows any ordering/location, only
  the pointer chain is reliable.
- BAR probe with `0xFFFFFFFF` but reading the width bits as part of size.
- MSI-X table offset applied to the wrong BAR, or not checking 8-byte/page
  alignment.
- Confusing base/subclass/prog-if byte order (base class at the high byte of
  the dword load).
