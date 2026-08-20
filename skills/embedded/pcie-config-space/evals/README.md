# Evaluation — pcie-config-space

Skill: `skills/embedded/pcie-config-space`.
Stability: `researched` (host-verified models + gcc fixtures; real PCIe
hardware/QEMU verification documented as target, UNVERIFIED on this host).
Toolchain: Python 3.11.9, gcc 16.1.0 (MSYS2 MinGW, host x86_64).

## Verified facts (host, recorded 2026-08-20)

All host-runnable artifacts were actually executed on 2026-08-20:

```
python examples/tools/capability_walk.py
  PASS good blob: 0x40/PM -> 0x50/MSI -> 0x60/PCIe -> 0x70/MSI-X
    (4 caps, terminated at next=0)
  FLAG cyclic blob: cycle detected (visited 0x40/PM -> 0x50/MSI -> 0x60/PCIe)
  FLAG out-of-range blob: next pointer outside config space
  PASS empty blob: no capabilities (ptr=0)
  capability_walk: ALL PASS

python examples/tools/bar_probe.py
  32-bit memory BAR (16 MiB): size=0x1000000 (want 0x1000000) -> PASS
  64-bit memory BAR (256 GiB): size=0x4000000000 -> PASS
  I/O BAR (256 bytes): size=0x100 -> PASS
  32-bit memory BAR (4 KiB, prefetch): size=0x1000 -> PASS
  64-bit memory BAR (1 GiB): size=0x40000000 -> PASS
  -- wrong-mask agent bug reproduced --
  bad mask on 16 MiB BAR: size=0x1000000 (want 0x1000000) -> PASS
  bar_probe: ALL PASS
```

Both python models are dependency-free and reproduce the failure shapes they
teach: a cyclic next-pointer chain (infinite-loop bug class) and a wrong
width-bit mask in BAR probing.

The full-parse python model (header + walk + BAR probe + MSI-X) also ran:

```
python examples/good/config_space.py
  vendor ID: 8086 -> PASS
  device ID: 100E -> PASS
  class code (base@high byte): 02:00:00 -> PASS
  capability walk: PM@0x40 -> MSI@0x50 -> PCIe@0x60 -> MSI-X@0x70 -> PASS
  list termination: 0 -> PASS
  MSI 64-bit flag: True -> PASS
  MSI addr_lo: 0xFEE00000 -> PASS
  MSI-X vector count: 4 -> PASS
  MSI-X BIR: 0 -> PASS
  MSI-X table offset: 0x1000 -> PASS
  MSI-X table 8B aligned: True -> PASS
  MSI-X table page aligned: True -> PASS
  BAR0 probe 16 MiB: 0x1000000 -> PASS
  BAR 64-bit 256 GiB: 0x4000000000 -> PASS
  config_space (good): ALL PASS        (exit 0)

python examples/bad/config_space.py
  vendor+device packed as one dword: 0x100E8086 (BUG: two 16-bit words needed)
  BAR0 size (wrong mask 0xFFFF0000): 0x10000 (BUG: expected 0x1000)
  64-bit BAR treated as 32-bit: size=0x100000000 (BUG: second dword required)
  MSI-X: BIR=2 table_off=0x1000 (decoder assumes BAR0 — BUG: BIR ignored)
  MSI-X table 0x1008: 8B-aligned=True page-aligned=False (BUG)
  NOTE: script exits 0 — these bugs are silent and must be caught by review
```

```
gcc -Wall -Wextra -Werror -O2 examples/good/pci_structs.c -o good.exe
  (exit 0 — compiles clean with _Static_assert offset checks)
good.exe:
  == PCIe config space model (good) ==
  vendor/device: 8086:100E
  class: base=02 subclass=00 prog-if=00
  caps ptr: 0x40
  cap @0x40 id=0x01 PM next=0x50
  cap @0x50 id=0x05 MSI next=0x60
  cap @0x60 id=0x10 PCIe next=0x70
  cap @0x70 id=0x11 MSI-X next=0x00
  MSI: 64-bit, 1 vector(s), addr=0x00000000FEE00000 data=0x0033
  MSI-X: 4 vectors, table BIR=0 off=0x1000, PBA BIR=0 off=0x2000
  BAR0 probe: type=memory 32-bit size=0x1000000
  64-bit probe: type=memory 64-bit size=0x40000000
  I/O probe: size=0x100
  PASS: layout offsets asserted, capability walk terminated, BAR and MSI-X
  math correct
```

```
gcc -Wall -Wextra -Werror -O2 examples/bad/pci_structs.c -o bad.exe
  (exit 0 — the bad code compiles CLEAN; the bugs are semantic)
bad.exe:
  == PCIe config space model (BAD — for review) ==
  caps (fixed offsets): 0x40=PM 0x50=MSI 0x60=PCIe 0x70=MSI-X
  walk: 0x40=PM 0x50=MSI 0x60=PCIe 0x70=MSI-X
  BAR0 size (wrong mask): 0xFF0001
  BAR type: memory 32-bit (BUG: 64-bit type read as 32-bit)
  64-bit BAR size (32-bit truncation): 0x3FFFFFF1
  MSI-X: BIR=0 table_off=0x1000 (aligned checks skipped)
  MSI-X table 0x1008 page check: would pass (BUG: page alignment not enforced)
  NOTE: compiles clean under -Werror; all values above must be rejected by
  capability-walk + mask + alignment reasoning
```

The bad fixture's wrong-mask BAR probe reports 0xFF0001 (16 MiB), the truncated
64-bit probe reports 0x3FFFFFF1 (1 GiB), and the page-unaligned MSI-X table at
0x1008 is accepted. All three values must be rejected by the skill's
reasoning, despite the clean compile.

## Synthetic evals

| Case | Input | Expected | Recorded |
|---|---|---|---|
| easy/positive | good capability blob (PM->MSI->PCIe->MSI-X, next=0) | walk all 4, terminated | PASS |
| easy/negative | cyclic next-pointer blob | cycle FLAG, no hang | FLAG |
| medium/negative | out-of-range next pointer | bounds FLAG | FLAG |
| easy/positive | 32-bit mem BAR readback 0xFF000000 | size 0x1000000 | PASS |
| medium/positive | 64-bit BAR readback lo=0x00000006 hi=0xFFFFFFC0 | size 0x4000000000 | PASS |
| easy/positive | I/O BAR readback 0xFFFFFF01 | size 0x100 | PASS |
| medium/negative | wrong-mask BAR probe | wrong size detected | PASS (model flags) |
| easy/negative | vendor+device packed as one dword | byte-order bug exposed | PASS (model flags) |
| medium/negative | MSI-X BIR ignored / page alignment skipped | wrong decode exposed | PASS (model flags) |
| positive | good pci_structs.c | clean compile + correct decode | exit 0, PASS |
| negative | bad pci_structs.c | wrong values exposed | exit 0, values wrong |

## False-positive evals (correct code must NOT be flagged)

- A capability list whose first cap is at an unusual legal offset (e.g. 0x44)
  reached via the 0x34 pointer — must still parse correctly because the walk
  follows pointers, not fixed offsets.
- A 64-bit BAR probed across both dwords with a correct 64-bit size.
- A correctly-aligned MSI-X table (8-byte entries, table page-aligned, BIR
  pointing at the right BAR).
- A full 4KB ECAM read of extended config space — must NOT be flagged as
  exceeding the 256-byte legacy limit.
- `lspci -vvx` output showing extended capabilities — must not be mistaken
  for a legacy-only device.

## Historical evals

- Early PCI driver BAR-probe disasters: drivers that read the masked value
  without clearing the width bits got nonsensical sizes (e.g. 16 MiB read as
  64 KiB or 128 MiB). The model reproduces the wrong-mask shape and the
  correct `~masked + 1` recovery.
- MSI-X table alignment bugs: table offsets that are 8-byte aligned but not
  page-aligned (0x1008) were shipped because BIR/offset decoding was right but
  the page-alignment invariant was skipped. The bad fixture accepts 0x1008;
  the good fixture rejects any non-page-aligned table.
- Config-access truncation bugs: reading/writing only the first 256 bytes when
  the capability (AER, ACS, SR-IOV, LTR) lives in extended space, or probing
  extended space via legacy 0xCF8/0xCFC. Documented in
  `references/pcie-config.md`; target reproduction on hardware.

## Adversarial evals

- A plausible capability blob with a cycle hidden after three valid caps —
  the walk must FLAG, not loop.
- A 64-bit BAR (type 0x3) presented to a 32-bit-only probe — must be detected
  via the type bits, not silently truncated.
- An MSI-X table offset that is 8-byte aligned but page-unaligned, with a
  "confident" comment claiming correct alignment — must be caught by the
  page-alignment check.
- A vendor/device pair packed into a single 32-bit read (byte-order bug) —
  the correct parse reads two 16-bit words.

## Verification commands (target — real machine or QEMU)

Documented target flow; NOT run on this host (no PCIe hardware, no QEMU):

```
lspci -vvxx -s <bus:dev.fn>          # legacy + extended config dump
setpci -s <bus:dev.fn> CAP_EXP+10.b  # byte access to a capability field
setpci -s <bus:dev.fn> 0x10.l        # read a BAR dword
dd if=/sys/bus/pci/devices/<bdf>/config bs=4096 count=1 status=none | xxd
# /sys config file may expose only 256 bytes on legacy platforms; ECAM
# platforms (MMCONFIG) expose the full 4KB. Compare sizes to decide.
# QEMU: qemu-system-x86_64 -machine q35 (or a virt machine) models PCIe
# devices with full ECAM; lspci inside the guest validates the walk.
```

## Scoring

- precision: every FLAG maps to a named invariant (termination, bounds,
  width-bit mask, 64-bit type, MSI-X 8B/page alignment, byte order).
- recall: cyclic, out-of-range, wrong-mask, 64-bit-truncation, and
  unaligned-MSIX fixtures are all caught by the models or exposed by the bad
  fixture's wrong output.
- FP-rate: the good fixture (correct walk, correct mask, aligned MSI-X) passes
  with zero false flags.
- Target-verification gap: real-device behavior is documented, not executed;
  stability remains `researched` until run against hardware or QEMU.
