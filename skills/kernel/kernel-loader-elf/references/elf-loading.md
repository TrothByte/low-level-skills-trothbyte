# Kernel Loader: Boot-Time ELF Loading — Reference Rules

Knowledge layer for `kernel-loader-elf`. Format: RULE → WHY AI GETS IT
WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE → VERIFICATION →
SOURCE. Uncertainty marked KNOWN / INFERRED / UNVERIFIED. The
load-address and relocation fixtures were executed on this host (gcc);
kernel-boot runs are UNVERIFIED. Relative paths assume the skill
directory as CWD.

## 1. Only PT_LOAD segments are loaded; alignment couples p_vaddr and p_offset

- **RULE**: A boot loader maps `PT_LOAD` program headers only. The runtime
  address is `p_vaddr (+ bias)`, the file bytes come from `p_offset`, and
  `p_vaddr ≡ p_offset (mod p_align)` must hold (the linker guarantees it;
  the loader must honor it). Loading `p_filesz` bytes at an unaligned
  `p_vaddr` produces a mapping the hardware MMU / kernel rejects.
- **WHY AI GETS IT WRONG**: agents copy the whole file or read
  `p_offset` and `p_vaddr` as independent numbers, dropping the
  congruence that makes a direct `mmap`-style mapping legal.
- **CORRECT REASONING**: the segment must be mapped with its alignment;
  the file offset must be placed at the congruent file position. Compute
  the page-aligned base, then add the `p_vaddr % p_align` remainder.
- **EXAMPLE** (bad): mapping a 4KiB-aligned `p_vaddr=0x1000`,
  `p_offset=0x0` segment at `base+0x0` instead of `base+0x0` with the
  file at page 0 (this example is actually correct; see the fixture for
  the misaligned variant: `examples/bad/elf_bss_missing.c`).
- **COUNTEREXAMPLE** (good): `examples/good/elf_load_address.c` computes
  `base = round_down(p_vaddr, p_align)` and asserts congruence.
- **VERIFICATION**: `elfaddr` run (executed) + `readelf -l` cross-check
  (documented). The congruence rule is KNOWN.
- **SOURCE**: sysv-elf (program header, segment alignment);
  gnu-ld-manual (segment layout and alignment).

## 2. BSS tail (p_memsz − p_filesz) must be zero-filled

- **RULE**: `p_filesz` bytes come from the file; the remainder of
  `p_memsz` is BSS and must be zeroed by the loader. Skipping the
  zero-fill means uninitialized kernel globals contain stale RAM.
- **WHY AI GETS IT WRONG**: agents map the file and forget that `p_memsz`
  can exceed `p_filesz`, or zero the wrong region.
- **CORRECT REASONING**: after copying the file bytes, memset the range
  `[p_vaddr+p_filesz, p_vaddr+p_memsz)` to zero for every PT_LOAD with
  `p_memsz > p_filesz`.
- **EXAMPLE** (bad): `examples/bad/elf_bss_missing.c` — maps file bytes
  and never zeroes the tail; the fixture detects the garbage.
- **COUNTEREXAMPLE** (good): `examples/good/elf_load_address.c` zero-fills
  the BSS tail and asserts it.
- **VERIFICATION**: `elfbss` prints the garbage residue it would otherwise
  carry (executed). The rule is KNOWN.
- **SOURCE**: sysv-elf (PT_LOAD, p_filesz/p_memsz); gnu-ld-manual (BSS
  section semantics).

## 3. ET_DYN (PIE) requires a load bias applied to every relative relocation

- **RULE**: `ET_EXEC` images run at fixed `p_vaddr`; `ET_DYN` images are
  position-independent and must be relocated by a load bias. Relative
  relocations (`R_X86_64_RELATIVE`, `R_AARCH64_RELATIVE`, etc.) are
  computed as `B + A` (load base + addend; the base symbol value S = 0).
  Skipping them leaves the GOT pointing at the image's link-time base.
- **WHY AI GETS IT WRONG**: agents handle ET_EXEC and ET_DYN identically,
  or relocate ET_EXEC (harmless but wrong) and skip ET_DYN (fatal when
  the kernel is loaded at a different address). A common error is
  computing `bias + A` instead of `B + A` (`bias` is the *difference*
  between load and link base; `B` is the absolute load base itself).
- **CORRECT REASONING**: on `e_type == ET_DYN`, choose `B` (load base),
  add it to every `PT_LOAD` `p_vaddr`, and for each relative relocation
  write `B + A` (from `.rela.dyn` / `PT_DYNAMIC`).
- **EXAMPLE** (bad): skipping relocations on an ET_DYN kernel loaded at a
  non-link-time address — GOT entries still point at the old base.
- **COUNTEREXAMPLE** (good): `examples/good/elf_relocation.c` — applies
  relative fixups and verifies the resulting pointers.
- **VERIFICATION**: `elfrel` prints relocated values (executed). The rule
  is KNOWN.
- **SOURCE**: sysv-elf (relocation types, dynamic section).

## 4. e_entry is a virtual address, not a file offset

- **RULE**: `e_entry` is the virtual address of the entry point in the
  *loaded* image (`e_entry + bias` for ET_DYN). Jumping to it before
  mapping segments, or treating it as a file offset, branches into
  nothing.
- **WHY AI GETS IT WRONG**: agents jump to `e_entry` directly (forgetting
  bias) or compute an entry "offset" from the file and call it.
- **CORRECT REASONING**: resolve the entry to a runtime address and check
  it falls inside a mapped `PT_LOAD` with execute permission before
  jumping.
- **EXAMPLE** (bad): `jmp e_entry` on an ET_DYN kernel loaded at bias
  ≠ 0.
- **COUNTEREXAMPLE** (good): entry = `e_entry + bias`, validated against
  the RX segment range.
- **VERIFICATION**: range check in the good fixture (executed); target
  boot UNVERIFIED.
- **SOURCE**: sysv-elf (ELF header, e_entry).

## 5. Compressed kernels need a decompressor stage before the real entry

- **RULE**: A compressed kernel (bzImage, vmlinuz, xz/zstd/gzip payload)
  is loaded, then a decompressor runs in place and jumps to the
  decompressed image's entry. The loader must identify the payload
  format by magic and hand control to the decompressor (which may itself
  be a small ELF with its own PT_LOAD segments), then to the final entry.
- **WHY AI GETS IT WRONG**: agents assume the loaded bytes *are* the
  kernel and jump into a compressed blob, which faults at the first
  instruction.
- **CORRECT REASONING**: decompression is a separate stage: detect magic
  (`\x1f\x8b` gzip, `\xfd7zXZ` xz, `\x28\xb5\x2f\xfd` zstd), invoke the
  in-place decompressor, then load/run the resulting image.
- **EXAMPLE** (bad): jumping to the entry of a compressed payload.
- **COUNTEREXAMPLE** (good): the loader chains decompressor → final
  entry.
- **VERIFICATION**: target-only; documented. UNVERIFIED as a run.
- **SOURCE**: kernel-source (boot protocol, compressed image entry);
  qemu-docs (target harness).

## 6. Entry handoff follows the architecture boot protocol

- **RULE**: the loader passes control to the kernel with the
  architecture's exact argument contract. x86 Linux: `boot_params` in
  `%esi`, magic `0x53726448` in `%edx` (or multiboot: magic `0x2BADB002`
  in `%eax`); ARM64: DTB physical address in `x0`, `x1=0`, `x2=0`.
- **WHY AI GETS IT WRONG**: agents invent a convenient calling convention
  ("pass the DTB in x2") that the kernel ignores, or skip the magic
  check the kernel uses to detect a valid boot loader.
- **CORRECT REASONING**: read the target's boot protocol and set each
  register exactly; the kernel validates the magic and fails early if it
  is wrong.
- **EXAMPLE** (bad): ARM64 loader passing DTB in x1.
- **COUNTEREXAMPLE** (good): x0=DTB, x1=0, x2=0.
- **VERIFICATION**: UNVERIFIED as a run; the register contract is KNOWN
  from aarch64-boot-protocol and kernel-source.
- **SOURCE**: aarch64-boot-protocol (x0/x1/x2); kernel-source (x86 boot
  protocol).

## Quick reference table

| Topic | Rule in one line |
|---|---|
| which headers | PT_LOAD only; ignore everything else for mapping |
| alignment | p_vaddr ≡ p_offset (mod p_align) |
| BSS | zero [p_vaddr+p_filesz, p_vaddr+p_memsz) |
| ET_DYN | add bias to p_vaddr and every relative relocation |
| entry | e_entry (+ bias); must land in an RX segment |
| compressed | decompressor stage first; magic identifies format |
| handoff | x86 %esi/%edx magic; ARM64 x0=DTB, x1=0, x2=0 |
