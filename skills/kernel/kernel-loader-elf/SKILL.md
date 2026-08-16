---
name: kernel-loader-elf
description: Use when a bootloader must load an ELF kernel image at boot time: parsing program headers, mapping virtual addresses and alignment, applying relocations for position-independent kernels, decompressing a compressed payload, and performing the entry-point handoff. Teaches the boot-time subset of ELF loading that differs from userspace loading.
---
# Kernel Loader: Boot-Time ELF Loading

## When to use

- Writing or reviewing a bootloader stage that loads an ELF kernel
  (multiboot-style, `kexec`, bare-metal homegrown OS, or a UEFI-loaded
  kernel image) and must map its segments into memory.
- Computing load addresses from `PT_LOAD` program headers with correct
  alignment and BSS zero-fill.
- Applying dynamic relocations (`R_*_RELATIVE`, GOT/PLT fixups) to a
  position-independent kernel image before jumping to it.
- Decompressing a compressed kernel (gzip/xz/zstd payload) and handing
  off to the decompressed entry point.
- Reviewing generated ELF-loading code that "looks correct" but reads
  `p_offset`/`p_vaddr`/`p_filesz`/`p_memsz` from the wrong headers.

## When not to use

- Userspace dynamic linking (GOT/PLT at `execve` time, `ld.so`) — that is
  `elf-dynamic-linking-got-plt`.
- ELF object file layout, section headers, or symbol tables for debugging —
  use `elf-layout-and-relocations` / `elf-linker-loader-debugger`.
- Writing the ELF from scratch or link scripts — use `gnu-ld-manual`
  / `embedded-linker-script`.
- General UEFI/ACPI/DTB handoff after the kernel is in memory — use
  `bootloader-uefi-acpi-dtb`.

## What the agent often gets wrong

- Loading `p_offset` bytes straight from the file into `p_vaddr` without
  honoring page/segment alignment — on x86 with 4KiB pages, `p_vaddr`
  lower bits must match `p_offset` modulo page size (the `p_vaddr ≡
  p_offset (mod align)` rule), or the mapping is misaligned.
- Forgetting BSS zero-fill: only `p_filesz` bytes come from the file;
  the `p_memsz - p_filesz` tail must be zeroed, or uninitialized data
  leaks into kernel globals.
- Applying relocations to a non-PIE image, or failing to add the load bias
  to `R_*_RELATIVE` fixups of a PIE image — entry runs with a torn GOT.
- Assuming the file's ELF header tells you the *runtime* load address:
  `e_entry` is a virtual address, not a file offset; jumping to it before
  mapping segments is the classic "jump into nothing" bug.
- Treating compressed and uncompressed kernels the same: a compressed
  kernel needs a decompressor stage (with its own entry) that expands the
  payload in place before the real entry runs.
- Ignoring the target architecture's handoff contract (x86: `boot_params`
  in `%esi`, magic in `%edx` for the Linux protocol; ARM64: DTB in x0).

## How to reason correctly

1. **Parse the ELF header** (`e_phoff`, `e_phnum`, `e_phentsize`,
   `e_entry`, `e_type`). `ET_EXEC` = fixed addresses; `ET_DYN` = PIE,
   needs a load bias. Reject `ET_REL` (unlinked) at boot time.
2. **For each `PT_LOAD`**: compute the runtime address
   `p_vaddr + bias`, honor alignment (align down `p_vaddr` to the
   segment's `p_align`, keep `p_offset` congruent), copy `p_filesz`
   bytes, zero `p_memsz - p_filesz`, protect per `p_flags` (R/W/X).
3. **Relocations**: for ET_DYN apply `B + A` (load base + addend) for
   `R_X86_64_RELATIVE`-class fixups (and the corresponding relocation
   entries from the `PT_DYNAMIC`/`.rela.dyn` data). Verify the entry
   symbol resolves to a mapped address.
4. **Compressed payload**: identify the payload format (magic), run the
   decompressor (self-contained or built-in), then load *that* image —
   a decompressor may itself be a small ELF with its own program headers.
5. **Entry handoff**: jump to `e_entry + bias` with the architecture's
   expected arguments (x86 Linux: `boot_params` pointer in `%esi`, magic
   `0x53726448` in `%edx`; ARM64: DTB in `x0`, `x1=0`, `x2=0`; multiboot:
   magic in `%eax`). Never jump before verifying the entry is inside a
   mapped RX segment.
6. **Verify on host**: for any generated loader, run the good fixture's
   ELF-to-load-address logic against real object files (`readelf -l`)
   and assert the computed mappings match `readelf`'s segment view.

## What to verify

- Every `PT_LOAD` maps to the correct runtime address with `p_vaddr ≡
  p_offset (mod p_align)`.
- BSS tail zeroed for every `PT_LOAD` where `p_memsz > p_filesz`.
- ET_DYN relocation base added to all relative relocations; no fixup
  applied to a non-PIE image.
- `e_entry + bias` falls inside a mapped, executable `PT_LOAD`.
- Decompressor stage (if any) is invoked before the real kernel entry,
  and the compressed image's magic identifies its algorithm.
- The handoff registers/arguments match the target boot protocol.

## How to verify

```
# Host-verifiable: load-address math and relocation application
gcc -Wall -Wextra -Werror -O2 examples/good/elf_load_address.c -o elfaddr
elfaddr                          # prints computed mappings for a synthetic ELF
gcc -Wall -Wextra -Werror -O2 examples/good/elf_relocation.c -o elfrel
elfrel                           # applies relative relocations, checks sum
gcc -Wall -Wextra -Werror -O2 examples/bad/elf_bss_missing.c -o elfbss
elfbss                           # BAD: BSS not zeroed — detects garbage

# Cross-check with real tooling (documented; readelf present on this host):
gcc -c -fPIC -o tmp.o examples/good/elf_relocation.c && readelf -l tmp.o
#   compare the fixture's computed addresses with readelf's program headers

# Target (documented; kernel boot not reproducible on this host):
# x86 Linux boot protocol: jump with boot_params in %esi, magic in %edx
# ARM64: DTB in x0, x1=0, x2=0 (see bootloader-uefi-acpi-dtb)
```

The load-address and relocation logic is host-verifiable (gcc runs,
output in `evals/README.md`); the kernel entry handoff needs a real
bootloader environment, marked UNVERIFIED.

## Where the knowledge comes from

- `sysv-elf` — program headers, `PT_LOAD`, `e_entry`, relocation types.
- `gnu-ld-manual` — alignment and load-address semantics for segments.
- `aarch64-boot-protocol` — ARM64 entry handoff (DTB in x0).
- `kernel-source` — Linux boot protocol specifics (x86 `boot_params`,
  compressed kernel entry).
- `qemu-docs` — verification environment for target runs.

## Related skills

- `elf-linker-loader-debugger` (require; adjacent ELF mechanics).
- `elf-layout-and-relocations` (require; section layout, relocation
  semantics).
- `elf-dynamic-linking-got-plt` (recommend; userspace GOT/PLT contrast).
- `bootloader-uefi-acpi-dtb` (recommend; post-load handoff data).
- `bootloader-stages` (recommend; where the loader runs in the boot flow).

## Evaluation

- Synthetic: `bad/elf_bss_missing.c` must be flagged; `good/elf_load_address.c`
  and `good/elf_relocation.c` must pass and agree with `readelf`.
- False-positive: ET_EXEC kernel with no relocations is correct; a
  segment with `p_memsz == p_filesz` legitimately has no BSS tail — do
  not flag.
- Historical: misaligned segment mapping and missing-BSS bugs are
  documented bootloader failure classes (multiboot/kexec communities);
  UNVERIFIED as named incidents on this host.
- Adversarial: a PIE image whose relative relocations are skipped runs
  fine in a stub and crashes only on hardware with ASLR; a loader that
  jumps to `e_entry` without a bias for ET_DYN must be caught.
- Verified facts and commands: `evals/README.md`.
