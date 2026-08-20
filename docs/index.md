---
title: Low-level skills TrothByte
description: 193 verified, source-backed low-level engineering skills for AI coding agents — C, C++, Rust, assembly, kernel, networking, embedded, Zig, GPU, reverse engineering, build systems.
---

# Low-level skills TrothByte

**193 verified, source-backed low-level engineering skills for AI coding agents.**

AI agents writing C, Rust, assembly, kernels, or firmware fail in predictable ways:
they trust "it compiles", guess ABIs, ignore memory ordering, and skip verification.
This repository fixes exactly those failures.

- **193 skills** · **35 domains** · **101 source-backed** (executed on real toolchains) · **290 primary sources** · **248 traced claims**
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
| [Kernel](https://github.com/TrothByte/low-level-skills-trothbyte/tree/main/skills/kernel) | RCU, uaccess, VFS/fops, page cache & writeback, workqueues, timers, kthreads, scheduler/MM, drivers, io_uring, Rust modules |
| [Networking](https://github.com/TrothByte/low-level-skills-trothbyte/tree/main/skills/networking) | sk_buff management, TCP congestion control, eBPF verifier, NAPI, BPF CO-RE |
| [Embedded](https://github.com/TrothByte/low-level-skills-trothbyte/tree/main/skills/embedded) | registers, devicetree, RTOS, OTA, HIL, USB, PCIe, MTE, reproducible builds |
| [Safety](https://github.com/TrothByte/low-level-skills-trothbyte/tree/main/skills/safety) | MISRA compliance, hard real-time determinism |
| [Zig](https://github.com/TrothByte/low-level-skills-trothbyte/tree/main/skills/zig) | comptime, allocators, build system, FFI, SIMD |
| [GPU](https://github.com/TrothByte/low-level-skills-trothbyte/tree/main/skills/gpu) | PTX, Vulkan compute, memory model, kernel verification |
| [RE / Binary](https://github.com/TrothByte/low-level-skills-trothbyte/tree/main/skills/reverse-engineering) | decompilation, shellcode, CAN, Ghidra |
| [Build systems](https://github.com/TrothByte/low-level-skills-trothbyte/tree/main/skills/build-systems) | CMake, toolchain drift, linker errors |
| [Agent behavior](https://github.com/TrothByte/low-level-skills-trothbyte/tree/main/skills/_meta) | deception detection, refactoring guard, evidence, verification |

## Documentation

- [Full skill catalog](SKILLS.md)
- [Architecture](architecture.md)
- [Research surveys](https://github.com/TrothByte/low-level-skills-trothbyte/tree/main/research)
- [Acknowledgments](ACKNOWLEDGMENTS.md)

## License

MIT — see the [repository](https://github.com/TrothByte/low-level-skills-trothbyte).
