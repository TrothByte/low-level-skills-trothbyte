# elf — Skills

ELF is the binary format of Linux and friends.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `elf-dynamic-linking-got-plt` | Use when explaining or debugging how a dynamically linked ELF program resolves external calls and data — GOT/PLT layout, lazy vs eager binding (LD_BIND_NOW), R_X86_64_GOTPCREL/PLT32 relocations, readelf -d and objdump -R output, symbol interposition, and -fPIC/-fPIE implications. | common | source-backed | `skills/elf/elf-dynamic-linking-got-plt` |
| `elf-layout-and-relocations` | Use when reading or debugging ELF object files and executables — ELF header fields, section vs program headers, .text/.data/.bss/.rodata/.dynsym/.got/.plt roles, symbol binding and visibility, R_X86_64_* relocation types, static vs dynamic linking, and the "recompile with -fPIC" error on x86-64 shared objects. | common | source-backed | `skills/elf/elf-layout-and-relocations` |
| `elf-linker-loader-debugger` | Use when diagnosing or building ELF binaries — symbol resolution, static vs dynamic linking, relocation failures, undefined symbols, missing -fPIC, PLT/GOT lazy binding, .init_array ordering, _start-to-main flow, dynamic loader errors, or mapping addresses to source lines in a debugger. Explains the compiler-to-linker-to-loader-to-debugger pipeline as one process. | cross-layer | source-backed | `skills/elf/elf-linker-loader-debugger` |

## How to use

- Read the `SKILL.md` of a skill for its triggers, reasoning rules, and verification commands.
  (`references/` hold deep knowledge; `examples/good` and `examples/bad` are compiled/run
  fixtures; `evals/README.md` defines eval cases.)
- Load only the skill you need (see `skills/_meta/meta-routing`; references load on demand.

## Related

[Back to repository root](../../README.md)
