---
title: Low-level skills TrothByte
description: 124 verified, source-backed low-level engineering skills for AI coding agents — C, C++, Rust, assembly, kernel, embedded, Zig, GPU, reverse engineering, build systems.
---

# Low-level skills TrothByte

**124 verified, source-backed low-level engineering skills for AI coding agents.**

AI agents writing C, Rust, assembly, kernels, or firmware fail in predictable ways:
they trust "it compiles", guess ABIs, ignore memory ordering, and skip verification.
This repository fixes exactly those failures.

- **124 skills** · **32 domains** · **65 source-backed** (executed on real toolchains) · **177 primary sources** · **56 traced claims**
- Every normative claim is traced: `claim → source → section → skill`
- `research/` documents 55+ real, source-traced AI-agent failures in low-level code
- MIT license · CI-green · Agent Skills spec-compliant

## Install

```bash
git clone https://github.com/TrothByte/low-level-skills-trothbyte
python tools/validate.py    # every skill, registry, and claim passes the gates
```

Or as a Claude Code plugin marketplace:

```text
/plugin marketplace add TrothByte/low-level-skills-trothbyte
/plugin install low-level-skills@trothbyte-low-level-skills
```

Or via [skills.sh](https://skills.sh/):

```bash
npx skills add TrothByte/low-level-skills-trothbyte
```

## Domains

| Domain | Skills |
|---|---|
| [Assembly](https://github.com/TrothByte/low-level-skills-trothbyte/tree/main/skills/assembly) | x86-64, NASM/GAS/AT&T, Thumb-2, NEON, RISC-V |
| [C](https://github.com/TrothByte/low-level-skills-trothbyte/tree/main/skills/c) | UB, integer promotion, strings, errno, signal safety |
| [C++](https://github.com/TrothByte/low-level-skills-trothbyte/tree/main/skills/cpp) | RAII, lifecycle, move semantics |
| [Rust](https://github.com/TrothByte/low-level-skills-trothbyte/tree/main/skills/rust) | unsafe, FFI, API drift, supply chain, crypto |
| [Kernel](https://github.com/TrothByte/low-level-skills-trothbyte/tree/main/skills/kernel) | RCU, uaccess, scheduler/MM/VFS, drivers |
| [Embedded](https://github.com/TrothByte/low-level-skills-trothbyte/tree/main/skills/embedded) | registers, devicetree, RTOS, OTA, HIL |
| [Zig](https://github.com/TrothByte/low-level-skills-trothbyte/tree/main/skills/zig) | comptime, allocators, build system, FFI, SIMD |
| [GPU](https://github.com/TrothByte/low-level-skills-trothbyte/tree/main/skills/gpu) | PTX, memory model, kernel verification |
| [RE / Binary](https://github.com/TrothByte/low-level-skills-trothbyte/tree/main/skills/reverse-engineering) | decompilation, shellcode, CAN, Ghidra |
| [Build systems](https://github.com/TrothByte/low-level-skills-trothbyte/tree/main/skills/build-systems) | CMake, toolchain drift, linker errors |

## Documentation

- [Full skill catalog](SKILLS.md)
- [Architecture](architecture.md)
- [Research surveys](https://github.com/TrothByte/low-level-skills-trothbyte/tree/main/research)
- [Acknowledgments](ACKNOWLEDGMENTS.md)

## License

MIT — see the [repository](https://github.com/TrothByte/low-level-skills-trothbyte).
