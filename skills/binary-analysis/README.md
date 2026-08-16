# binary-analysis — Skills

Reading binaries means recovering what the compiler encoded.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `binary-analysis-type-recovery` | Use when recovering C types (char, short, int, long, float, double, structs, arrays, pointers, function/vtable signatures) from x86-64 disassembly via instruction-width and addressing patterns (movzx/movsx, movss/movsd, movl/movq, disp(%reg), scale indexing), validated with DWARF when present. | common | source-backed | `skills/binary-analysis/binary-analysis-type-recovery` |
| `binary-disassembly-decompilation-fidelity` | Use when decompiling or judging decompiled code from x86-64 binaries, or when a reconstructed C function "looks right". Apply re-executability and byte-round-trip gates because decompilation is plausible-but-wrong (DeGPT 37% CFR, SCDBench 7%, Meta LLM Compiler 14% exact, cliff at ~200 instructions). | unique | source-backed | `skills/binary-analysis/binary-disassembly-decompilation-fidelity` |
| `binary-memory-leak-vm-allocator-diagnosis` | Use when diagnosing high RAM/RSS/commit growth that heap profilers (ASan/leak checkers) do not explain — VM-level leaks: page pools that reuse mmap/mmap'd regions without munmap, unbounded pools, allocator reuse-without-release. Ghostty PageList.zig 37-130 GB; agent is the trigger, not the cause. | unique | researched | `skills/binary-analysis/binary-memory-leak-vm-allocator-diagnosis` |

## How to use

- Read the `SKILL.md` of a skill for its triggers, reasoning rules, and verification commands.
  (`references/` hold deep knowledge; `examples/good` and `examples/bad` are compiled/run
  fixtures; `evals/README.md` defines eval cases.)
- Load only the skill you need (see `skills/_meta/meta-routing`; references load on demand.

## Related

[Back to repository root](../../README.md)
