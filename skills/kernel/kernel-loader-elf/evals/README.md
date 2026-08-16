# Evaluation — kernel-loader-elf

Skill: `skills/kernel/kernel-loader-elf`. Stability target: `evaluated`.
ELF semantics KNOWN from sysv-elf / gnu-ld-manual / kernel-source. Load
math and relocation fixtures EXECUTED on this host (gcc 16.1.0). Real
kernel-boot handoff (x86/ARM64 entry) UNVERIFIED — no bootable QEMU
environment here.

## Synthetic evals

| Case | Fixture | Expected | Status |
|---|---|---|---|
| easy/negative | `bad/elf_bss_missing.c` | BSS tail not zero-filled → flagged | executable |
| easy/positive | `good/elf_load_address.c` | alignment + congruence + BSS fill correct | executable |
| easy/positive | `good/elf_relocation.c` | relative relocations applied with bias | executable |

Detection rule: for each PT_LOAD verify (1) `p_vaddr ≡ p_offset
(mod p_align)`; (2) `[p_vaddr+p_filesz, p_vaddr+p_memsz)` zeroed;
(3) ET_DYN bias applied to every relative relocation; (4) `e_entry + bias`
inside a mapped RX segment; (5) compressed payloads routed through a
decompressor before the final entry.

## False-positive evals (correct code must NOT be flagged)

- ET_EXEC image with `p_memsz == p_filesz` on every segment (no BSS tail)
  — correct, not a bug.
- ET_DYN image where the loader happens to be loaded at the link-time
  base (bias = 0) — the relocation code still runs, harmlessly.
- A segment with `p_align == 1` (no alignment constraint) — congruence is
  trivially satisfied; do not flag.
- A kernel image that is already decompressed (no magic prefix) loaded
  directly — correct.

## Historical evals

- Misaligned segment mapping and missing-BSS bugs are documented failure
  classes in bootloader development (multiboot/kexec/boot-stage code in
  the wild). UNVERIFIED as named incidents on this host.
- PIE kernels failing when the loader skipped relative relocations is a
  known class (ASLR-based boot environments). UNVERIFIED as a named
  incident here.

## Adversarial evals

- A PIE kernel image whose relative relocations are skipped runs fine in
  a stub (link-time base == load base) and only crashes when the loader
  places it at a different address — the unconditional-pass trap from
  `meta-verification-harness-validity`.
- A loader that jumps to `e_entry` without adding bias for ET_DYN — the
  entry points into nothing; only the RX-range check catches it.
- A compressed payload with a valid magic that the loader treats as an
  ELF — first instruction faults.
- A corrupted `p_memsz < p_filesz` header (malicious kernel image) must
  be rejected, not memcpy an oversized region.

## Verification commands

```
gcc -Wall -Wextra -Werror -O2 examples/good/elf_load_address.c -o elfaddr && elfaddr
gcc -Wall -Wextra -Werror -O2 examples/good/elf_relocation.c -o elfrel && elfrel
gcc -Wall -Wextra -Werror -O2 examples/bad/elf_bss_missing.c -o elfbss && elfbss

# Cross-check with real tooling on this host:
gcc -c -fPIC -o tmp.o examples/good/elf_relocation.c && readelf -l tmp.o

# Target (documented, not runnable here):
# x86 Linux: jump with boot_params in %esi, magic 0x53726448 in %edx
# ARM64: DTB in x0, x1=0, x2=0
```

## Verified facts

- KNOWN: PT_LOAD-only loading; `p_vaddr ≡ p_offset (mod p_align)`;
  BSS zero-fill rule; ET_DYN bias + R_*_RELATIVE application; e_entry is
  a virtual address; compressed-payload stage chaining; x86/ARM64 entry
  handoff contracts. Sources: sysv-elf, gnu-ld-manual,
  aarch64-boot-protocol, kernel-source.
- EXECUTED on this host: `elfaddr` PASS; `elfrel` PASS; `elfbss` detects
  stale RAM and exits 1 (recorded below).
- UNVERIFIED: real kernel boot through a bootloader, QEMU kernel boot,
  decompressor execution on this host.

## Scoring

- precision: every flagged issue maps to a reference rule (1–6).
- recall: all three bad fixtures detected (BSS, misalignment, relocation
  skip, wrong entry).
- FP-rate: ET_EXEC/no-BSS/bias-0 cases produce zero flags.
- Decisive test: "does the loader honor p_vaddr≡p_offset and zero the BSS
  tail?" and "is e_entry (+bias) inside a mapped RX segment?"

### Executed output (2026-08-17, MSYS2 gcc 16.1.0)

```
$ gcc -Wall -Wextra -Werror -O2 examples/good/elf_load_address.c -o elfaddr && ./elfaddr
seg0: base=0x100000 filesz=0x2000 bss=0x0
seg1: base=0x103000 filesz=0x1000 bss=0x3000
seg2: base=0x107000 filesz=0x2000 bss=0x0
PASS: load addresses computed with alignment + BSS zero-fill
exit 0

$ gcc -Wall -Wextra -Werror -O2 examples/good/elf_relocation.c -o elfrel && ./elfrel
PASS: relative relocations relocated to load base 0x8000000
exit 0

$ gcc -Wall -Wextra -Werror -O2 examples/bad/elf_bss_missing.c -o elfbss && ./elfbss
BUG: BSS tail contains stale RAM (0xcd at vaddr+0x2000)
exit 1
```
