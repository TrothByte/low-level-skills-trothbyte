---
name: pcie-config-space
description: Use when reading, writing, or reviewing PCIe config space — BARs, MSI/MSI-X capabilities, the capability linked list, class codes, or ECAM. Teaches the 4KB config layout and how to parse and probe it correctly.
---

# PCIe Config Space

## When to use

- Reading or writing PCI/PCIe config space: BARs, MSI/MSI-X capabilities,
  capability linked lists, class codes, PCIe extended capabilities.
- Probing a device's resources (BAR sizes, MSI-X table location) before
  writing a driver.
- Debugging why an interrupt, DMA mapping, or device BAR is misconfigured.
- Reviewing LLM-generated PCI code that hardcodes config offsets or skips the
  capability walk.

## When not to use

- Config space is already fully abstracted by a vendor HAL or by
  `pci_alloc_irq_vectors`-style helpers and the question is about interrupt
  handling, not config layout.
- The task is DMA streaming/cache coherency for a device that is already
  enumerated — see `dma-cache-coherency`.
- The task is USB device descriptors or USB enumeration — a different
  discovery mechanism entirely; see `usb-device-stack`.
- Setting up QEMU machines or -device models — see `qemu-system-setup`.
- Writing a character-device driver lifecycle around an already-probed PCI
  function — see `kernel-driver-char-device-lifecycle`.

## What the agent often gets wrong

- Reading/writing only 256 bytes when the capability lives in extended space
  (above 0xFF) — AER, ACS, SR-IOV, LTR are all in extended config — or trying
  to read 4KB through legacy I/O 0xCF8/0xCFC, which only reaches 256 bytes.
- Capability walk: assuming capabilities sit at fixed offsets, or ignoring the
  next-pointer indirection; not treating next=0 as the end of the list.
- BAR probing: writing all-1s and reading the wrong bits back; not masking the
  width bits (bits 0-3 for a memory BAR); assuming 32-bit when the type bits
  say 0x3 (64-bit BAR, needs a second dword).
- MSI-X specifics: table must be 8-byte aligned and the table region
  page-aligned inside its BAR; the BIR field selects WHICH BAR; agents read the
  table offset from the wrong BAR or skip the alignment checks.
- Byte order: vendor/device ID read as one 32-bit dword instead of two 16-bit
  words, or class code read as a plain int without placing the base class in
  the high byte.
- Reading config as 32-bit words when fields are 8/16-bit and adjacent
  (Command+Status at 0x04-0x07, Revision+Class+Header Type at 0x08-0x0F);
  also misreading the bus:device.function encoding of a bdf.
- The 0x34 capabilities pointer covers ONLY legacy capabilities in 0x00-0xFF;
  extended capabilities (starting from the first PCIe cap's next-cap offset)
  live at 0x100-0xFFF and are found differently.

## How to reason correctly

1. Start from the Header Type (0x0E): type 0 = endpoint, type 1 = bridge.
   The header layout after 0x10 differs — type 1 reuses BAR0-1 for
   secondary/subordinate bus numbers and has a secondary latency timer.
2. Always walk capabilities via the linked list: read caps ptr at 0x34 (type
   0/1), then follow byte[1] (next pointer) from each capability, terminating
   at next=0. Never assume capability IDs at fixed offsets. For extended
   space, walk from the first PCIe capability's next offset.
3. To probe a BAR size: read current value, save it, write 0xFFFFFFFF, read
   back, mask out the width bits (bits 0-3 for memory: bit0 space=0,
   bits1-2 type, bit3 prefetchable), invert + 1 = size, then restore the
   saved value. For 64-bit (type 0x3) probe the second dword too and combine
   to a 64-bit size.
4. For MSI-X: decode the BIR field (table offset bits 2-0) to pick the BAR
   index, take the table offset from bits 31-3, then enforce 8-byte entry
   alignment and page alignment for the table region within that BAR. Same
   process for the PBA.
5. Match the access method to the space: legacy config I/O (0xCF8/0xCFC) and
   the Linux /sys config file typically expose only the first 256 bytes;
   extended config requires ECAM/MMCONFIG or full 4KB read (e.g. `lspci -vvx`
   reads legacy, `lspci -xxx` reads 256, `setpci` with extended offsets, or
   the kernel pci_cfg_access paths).
6. Verify every parse against a real device on the target machine
   (`lspci -vvxx`, `setpci`) before trusting the offsets in generated code.

## What to verify

- Capability chain parsed correctly: IDs match known capability definitions,
  next pointers are followed, list terminates at 0, no cycles, no out-of-range
  offsets.
- BAR probe math yields the correct size for a known device (compare against
  `lspci` memory region sizes).
- MSI/MSI-X fields decoded: vector count, 64-bit vs 32-bit address, table
  BIR/offset, PBA BIR/offset, alignment invariants hold.
- Access method matches the space: 256-byte legacy for legacy caps vs
  ECAM/4KB for extended caps.

## How to verify

```
# host (python 3.11 + gcc 16.1 present on this repo's CI host):
python examples/tools/capability_walk.py        # good blob -> PASS, cyclic -> FLAG
python examples/tools/bar_probe.py              # 32/64-bit, I/O BAR sizes -> PASS
python examples/good/config_space.py            # full parse model -> ALL PASS
python examples/bad/config_space.py             # prints wrong values (review target)
gcc -Wall -Wextra -Werror -O2 examples/good/pci_structs.c -o good_cfg.exe
good_cfg.exe                                     # prints decoded caps + BAR probe
gcc -Wall -Wextra -Werror -O2 examples/bad/pci_structs.c -o bad_cfg.exe
bad_cfg.exe                                      # fixed offsets, wrong mask, wrong size

# target (document, run on real machine or QEMU):
lspci -vvxx -s <bus:dev.fn>
setpci -s <bus:dev.fn> CAP_EXP+10.b             # byte access to a capability
dd if=/sys/bus/pci/devices/<bdf>/config bs=4096 count=1   # legacy file, often 256B
# extended config needs ECAM/MMCONFIG; Linux exposes 4KB when the platform uses ECAM
```

## Where the knowledge comes from

- PCI Express Base Specification (https://pcisig.com/specifications/pciexpress)
- PCI Local Bus Specification rev 3.0 — config space (https://pcisig.com/specifications/conventional)
- Linux PCI driver API (https://docs.kernel.org/PCI/pci.html)
- lspci (https://man.7z.org/lspci), setpci man pages
- OSDev PCI documentation (https://wiki.osdev.org/PCI)

## Related skills

- `embedded-hw-register-datasheet-verification` — same datasheet-layout-as-code
  discipline applied to peripheral registers.
- `embedded-board-bringup-peripheral-init` — enumerating and initializing
  devices on a new board, including PCI functions.
- `dma-cache-coherency` — what happens to the BAR/MMIO window after DMA is
  mapped.
- `usb-device-stack` — config-space-style descriptor parsing for USB instead
  of PCI.
- `qemu-system-setup` — run the verification commands against QEMU's emulated
  devices when no real hardware is available.
- `kernel-driver-char-device-lifecycle` — lifecycle code that sits on top of a
  probed PCI function.

## Evaluation

- Synthetic: `examples/tools/capability_walk.py` must PASS on a good blob
  (PM->MSI->PCIe->MSI-X, terminated) and FLAG a cyclic next-pointer list;
  `examples/tools/bar_probe.py` must print PASS for 32-bit, 64-bit, and I/O
  BARs.
- False-positive: a correct full-config 4KB read, a valid 64-bit BAR probe, and
  a correctly-aligned MSI-X table must NOT be flagged.
- Historical: early PCI drivers' BAR-probe disasters (unmasked width bits
  giving nonsense sizes) and MSI-X alignment bugs (table straddling page
  boundaries, BIR misread) — the models reproduce the failure shapes.
- Adversarial: a plausible capability list with a cycle or an out-of-range next
  pointer; a BAR value with type 0x3 treated as 32-bit; an MSI-X table at an
  unaligned offset must all be caught.
- Verified host runs recorded in `evals/README.md`; target verification on real
  PCIe hardware or QEMU is documented and UNVERIFIED on this host.
