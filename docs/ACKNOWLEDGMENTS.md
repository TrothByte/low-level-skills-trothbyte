# Acknowledgments — Low-level skills TrothByte

This repository is a synthesis. The skills, failure-mode catalogs, and verification
disciplines here are built on the work of many open-source projects, standards bodies,
and individuals. We are grateful to every one of them.

## Skill repositories

The following skill repositories were studied as research input. Where licenses permit,
their approaches informed our skill design; content from UNKNOWN-license repositories was
used for ideas and methodology only (see `LICENSE` and `registry/claims.yaml` for the
attribution policy).

| Repository | License | How it shaped this work |
|---|---|---|
| [trailofbits/skills](https://github.com/trailofbits/skills) | CC-BY-SA-4.0 | Bug-class catalogs (69 Rust / 47–64 C classes), "Rationalizations to Reject", verification-gate design, calibration data (PR #238, issues #143/#181/#205/#216) |
| [mohitmishra786/low-level-dev-skills](https://github.com/mohitmishra786/low-level-dev-skills) | MIT | The broadest coverage map: compilers, linkers, ELF/DWARF, kernel, SIMD, ABI, bare-metal |
| [wshobson/agents](https://github.com/wshobson/agents) | MIT | Survey-style memory-safety and asm pattern skills, signed/unsigned branch idioms |
| [Jeffallan/claude-skills](https://github.com/Jeffallan/claude-skills) | MIT | Embedded and C++ RAII/ownership skills |
| [SnailSploit/Claude-Red](https://github.com/SnailSploit/Claude-Red) | MIT | Deep x64 assembly/shellcode content; Windows-on-ARM64 syscall conventions |
| [NVIDIA/skills](https://github.com/NVIDIA/skills) | CC-BY-4.0 / Apache-2.0 | GPU programming skills (cuTile/Triton), DOCA DMA |
| [Z3Prover/z3](https://github.com/Z3Prover/z3) | MIT | The sanitizer-in-CI-loop pattern with a findings database |
| [VoltAgent/awesome-agent-skills](https://github.com/VoltAgent/awesome-agent-skills) | MIT | Index of skill repositories; progressive-disclosure research |
| [zhaoxuya520/reverse-skill](https://github.com/zhaoxuya520/reverse-skill) | UNKNOWN | Reverse-engineering skill breadth; Go/Rust RE methodology |
| [zhinkgit/embeddedskills](https://github.com/zhinkgit/embeddedskills) | UNKNOWN | Embedded verification discipline (GDB, J-Link as gates) |
| [CSS-Electronics/can-bus-reverse-engineering-skills](https://github.com/CSS-Electronics/can-bus-reverse-engineering-skills) | UNKNOWN | The deterministic protocol RE pipeline and verify-as-gate |
| [P4nda0s/reverse-skills](https://github.com/P4nda0s/reverse-skills) | UNKNOWN | Reverse-engineering skill structure |
| [Masriyan/Claude-Code-CyberSecurity-Skill](https://github.com/Masriyan/Claude-Code-CyberSecurity-Skill) | UNKNOWN | Cybersecurity skill patterns |
| [SimoneAvogadro/android-reverse-engineering-skill](https://github.com/SimoneAvogadro/android-reverse-engineering-skill) | UNKNOWN | Platform-specific RE skills |
| [rudedogg/zig-skills](https://github.com/rudedogg/zig-skills) | UNKNOWN | Zig version-pinning problem; confidence-level review checklists |
| [zigcc/skills](https://github.com/zigcc/skills) | UNKNOWN | Zig skill structure |
| [nzrsky/zig-skills](https://github.com/nzrsky/zig-skills) | UNKNOWN | Zig skill structure |
| [whit3rabbit/claude-zig-skill](https://github.com/whit3rabbit/claude-zig-skill) | UNKNOWN | Zig recipe collection (BBQ Cookbook) |

## Standards and primary sources

The normative claims in this repository cite the following, among others:

- **ISO/IEC** WG14 (C11/C23: N1570, N3096) and WG21 (C++20/C++23: N4861, N4971)
- **cppreference.com** contributors — the C and C++ language references
- **SEI CERT** (Carnegie Mellon) — the C Coding Standard
- **Rust team** — The Rust Reference, The Rustonomicon, Rust API Guidelines
- **x86-64 psABI project** — System V AMD64 ABI
- **Arm** — AAPCS64, ABI suite, ARM ARM, CMSIS, NEON/SVE ACLE
- **RISC-V International** — ISA specs and psABI
- **Intel and AMD** — SDM and AMD64 APM, Optimization Reference Manuals
- **The LLVM project** — LLVM LangRef, Clang docs, sanitizer documentation
- **GNU** — GCC manual, binutils (as/ld/objdump/readelf), GDB manual
- **The QEMU project** — emulation documentation
- **The Linux kernel community** — memory-barriers.txt, RCU docs, eBPF docs, coding style
- **NVIDIA** — CUDA C++ Programming Guide, PTX ISA
- **The DWARF committee** — DWARF Debugging Information Format v5
- **MITRE** — CWE, and the CVE list authors (the historical CVE fixtures used in evals)

## Talks, articles, and research

- **Chandler Carruth** — *Garbage In, Garbage Out: Arguing about Undefined Behavior* (CppCon 2016)
- **Matt Godbolt** — *What Has My Compiler Done for Me Lately?* (CppCon)
- **Sean Parent** — *C++ Seasoning* (GoingNative 2013)
- **Aleksey Kladov (matklad)** — *Preconditions* and API design articles
- **Herb Sutter** — *GotW* and RAII articles
- **Agner Fog** — optimization manuals and instruction tables
- **Pearce et al.** — *Asleep at the Keyboard?* (IEEE S&P 2022)
- **Perry et al.** — *Do Users Write More Insecure Code with AI Assistants?* (CCS 2023)
- **Bhatt et al. (Meta)** — *CyberSecEval* (2023)
- **Ralf Jung et al.** — Stacked Borrows and Rust aliasing semantics

## Tools that verified this work

GCC 16.1 (MSYS2), rustc 1.97.1, GDB 17.2, GNU as/ld/objdump/nm, and Python 3.11 were used to
execute-verify the examples in this repository. Without reproducible toolchains, none of the
"source-backed" claims would be credible.

---

If your work is represented here and you would like different attribution, or if any
attribution is inaccurate, please open an issue and we will correct it promptly.
