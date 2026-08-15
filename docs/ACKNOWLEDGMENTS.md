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
| [trailofbits/skills](https://github.com/trailofbits/skills) | CC-BY-SA-4.0 | Bug-class catalogs (69 Rust / 47–64 C classes), "Rationalizations to Reject", verification-gate design, ablation-Δ eval methodology, calibration data (PR #238, issues #143/#181/#205/#216) |
| [wshobson/agents](https://github.com/wshobson/agents) | MIT | Survey-style memory-safety and asm pattern skills, PluginEval eval methodology (Monte-Carlo activation, statistical CI) |
| [Jeffallan/claude-skills](https://github.com/Jeffallan/claude-skills) | MIT | Embedded and C++ RAII/ownership skills |
| [SnailSploit/Claude-Red](https://github.com/SnailSploit/Claude-Red) | MIT | Deep x64 assembly/shellcode content; Windows-on-ARM64 syscall conventions (ideas only — content did not pass the verification bar) |
| [mohitmishra786/low-level-dev-skills](https://github.com/mohitmishra786/low-level-dev-skills) | MIT | The broadest coverage map: compilers, linkers, ELF/DWARF, kernel internals (scheduler/MM/VFS), SIMD, ABI, virtualization, HPC |
| [NVIDIA/skills](https://github.com/NVIDIA/skills) | CC-BY-4.0 / Apache-2.0 | GPU programming skills (cuTile/Triton), DOCA/DPU; knowledge-map routing, hardware-safety change discipline, four-source version audit |
| [Z3Prover/z3](https://github.com/Z3Prover/z3) | MIT | SMT-solver reasoning ground for the `smt-z3-sound-usage` skill |
| [VoltAgent/awesome-agent-skills](https://github.com/VoltAgent/awesome-agent-skills) | MIT | Index of skill repositories; progressive-disclosure research |
| [zhaoxuya520/reverse-skill](https://github.com/zhaoxuya520/reverse-skill) | UNKNOWN | Reverse-engineering skill breadth; Go/Rust RE methodology |
| [zhinkgit/embeddedskills](https://github.com/zhinkgit/embeddedskills) | UNKNOWN | Embedded verification discipline (GDB, J-Link as gates) |
| [CSS-Electronics/can-bus-reverse-engineering-skills](https://github.com/CSS-Electronics/can-bus-reverse-engineering-skills) | MIT | The deterministic CAN protocol RE pipeline, verify-as-gate, SWEEP/HOLDS methodology, parked-vs-moving gate |
| [P4nda0s/reverse-skills](https://github.com/P4nda0s/reverse-skills) | UNVERIFIED (README claims MIT) | Reverse-engineering skill structure; negative-guidance patterns |
| [Masriyan/Claude-Code-CyberSecurity-Skill](https://github.com/Masriyan/Claude-Code-CyberSecurity-Skill) | MIT | Chaining tables, output-template-first, authorization gates |
| [SimoneAvogadro/android-reverse-engineering-skill](https://github.com/SimoneAvogadro/android-reverse-engineering-skill) | Apache-2.0 | Android RE breadth; fingerprint-first triage; R8/Kotlin-metadata name recovery |
| [rudedogg/zig-skills](https://github.com/rudedogg/zig-skills) | UNKNOWN | Zig version-pinning problem; WRONG/CORRECT migration pairs (ideas only) |
| [zigcc/skills](https://github.com/zigcc/skills) | UNKNOWN | Zig version policy (`VERSIONING.md` update-in-place vs new-skill) as idea |
| [nzrsky/zig-skills](https://github.com/nzrsky/zig-skills) | MIT | Zig skill structure (derivative of rudedogg) |
| [whit3rabbit/claude-zig-skill](https://github.com/whit3rabbit/claude-zig-skill) | MIT | Zig recipe collection (BBQ Cookbook); multi-version references |
| [0xazanul/fuzz-skill](https://github.com/0xazanul/fuzz-skill) | UNKNOWN | "Proof Standard" for fuzzing claims — the basis of `fuzzing-harness-evidence-gate` (idea only) |
| [LNC0831/oh-my-fpga](https://github.com/LNC0831/oh-my-fpga) | MIT | FPGA/HDL discipline: CDC classification, timing closure, "never make the number green by hiding a violation" |
| [alexfdez1010/risc-v-skill](https://github.com/alexfdez1010/risc-v-skill) | MIT | RISC-V/RVV intrinsics: VL/AVL strip-mining, LMUL/SEW, tail/mask policies |
| [qwattash/cheri-skills](https://github.com/qwattash/cheri-skills) | BSD-3-Clause | CHERI capability semantics (bounds/tags, purecap/hybrid), cheribuild/QEMU harness (author: Alfredo Mazzinghi) |
| [MarsDoge/uefi-firmware-skill](https://github.com/MarsDoge/uefi-firmware-skill) | MIT | UEFI/PI spec-boundary discipline, edk2, QEMU serial-log debugging |
| [cellebrite-labs/ghidra-rpc](https://github.com/cellebrite-labs/ghidra-rpc) | UNKNOWN | Agent↔Ghidra automation loop (idea only) |
| [manyuegong33/r0crawl_skills](https://github.com/manyuegong33/r0crawl_skills) | UNKNOWN | RE router / beginner-contract patterns (idea only) |
| [mukul975/Anthropic-Cybersecurity-Skills](https://github.com/mukul975/Anthropic-Cybersecurity-Skills) | Apache-2.0 | Breadth reference for security skill taxonomy |
| [hackersifu](https://github.com/hackersifu) | MIT | Malware-RE workflows (IOC/unpacking) |
| [rekit](https://github.com/rekit) | Apache-2.0 | RE workflow automation |
| [n132/Libc-GOT-Hijacking](https://github.com/n132/Libc-GOT-Hijacking) | UNKNOWN | Exploit-technique portfolio framing (idea only) |

## Standards and primary sources

The normative claims in this repository cite the following, among others:

- **ISO/IEC** WG14 (C11/C23: N1570, N3096) and WG21 (C++20/C++23: N4861, N4971)
- **cppreference.com** contributors — the C and C++ language references
- **SEI CERT** (Carnegie Mellon) — the C Coding Standard
- **Rust team** — The Rust Reference, The Rustonomicon, Rust API Guidelines, Miri
- **x86-64 psABI project** — System V AMD64 ABI; **RISC-V psABI**
- **Arm** — AAPCS64, ABI suite, ARM ARM, CMSIS, NEON/SVE ACLE, Thumb-2
- **RISC-V International** — ISA specs, V-extension 1.0, RVV C intrinsics
- **CHERI project (Cambridge / CTSRD)** — CHERI ISA and CheriBSD
- **Intel and AMD** — SDM and AMD64 APM, Optimization Reference Manuals
- **The LLVM project** — LLVM LangRef, Clang docs, sanitizer documentation, lld
- **GNU** — GCC manual, binutils (as/ld/objdump/readelf), GDB manual, GNU Make
- **The QEMU project** — emulation documentation
- **The Linux kernel community** — memory-barriers.txt, RCU docs, eBPF docs, scheduler/MM/VFS
  docs, tracing docs (ftrace/kprobes/kdump), driver API, kbuild, namespaces(7), cgroup v2,
  OCI runtime spec, syscall tables
- **NVIDIA** — CUDA C++ Programming Guide, PTX ISA, NCCL; **AMD** — ROCm/HIP
- **InfiniBand Trade Association / rdma-core** — verbs documentation; **NVIDIA DOCA**
- **The DWARF committee** — DWARF Debugging Information Format v5
- **MPI Forum** — MPI-4.1; **OpenMP ARB** — OpenMP 5.x
- **UEFI Forum** — UEFI & PI specifications; **TianoCore** — EDK II
- **Zig Software Foundation** — Zig Language Reference, std source, release notes, build guide
- **NASM project** — NASM manual; **CMake**, **Ninja**, **pkg-config** documentation
- **Devicetree.org** — Devicetree Specification; **Vector** — CAN DBC format; **ISO** — 11898-1
- **MITRE** — CWE (incl. CWE-1254) and the CVE list authors
- **Frama-C/CEA** — ACSL; **Diffblue** — CBMC; **Amazon** — Kani; **Microsoft Research** — Z3, SMT-LIB
- **OpenOCD**, **MCUboot**, **Espressif ESP-IDF**, **Zephyr** — embedded toolchain documentation
- **Reparaz et al.** — dudect; **Thomas Pornin (BearSSL)** — ctgrind; **Trail of Bits** — constant-time analysis

## Talks, articles, and research

The three failure surveys in `research/` are grounded in these works, among others:

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
- **Mitchell Hashimoto** — *Ghostty memory leak fix* (VM/page-pool analysis, 2026)
- **Daniel Lemire** — NEON-SIMD per-lane overflow-guard analysis (2026)
- **Greg Kroah-Hartman** (The Register, 2026) and the LKML review experiments — AI-patch calibration (~⅔ correct)
- **Meta** — *Large Language Model Compiler* (arXiv 2407.02524)
- **LLM4Decompile** (arXiv 2403.05286); **FidelityGPT/DeGPT** (arXiv 2510.19615); **SLaDe** (2305.12520);
  **SCDBench** (2605.29059); **REx86** (2510.20975); **capability-cliff** (2607.06125);
  **Deconstructing Obfuscation** (2505.19887); **AutoDecompiler** (2606.16162)
- **CONCUR** (arXiv 2603.03683); **RustEvo²** (2503.16922); **crate hallucinations** (2606.08444);
  **We Have a Package for You** (2406.10279); **MARIN/APIHulBench** (2505.05057); **RustBrain** (2503.02335);
  **crypto-Rust generation** (2604.27001)
- **ISO-Bench** (arXiv 2602.19594); **The Correctness Illusion** (2606.20128); **CommBench** (2608.04450);
  **AgentKernelArena** (2605.16819); **FastKernels** (2605.23215)
- **NeuComBack** (arXiv 2511.01183); **SuperCoder** (2505.11480); **CompilerEval** (2511.04132);
  **PeepholeBench** (2607.02684); **LLVM-Bench** (2607.00700)
- **LiveFMBench** (arXiv 2605.01394); **loop-invariant repair** (2511.06552);
  **ProVerif/OFMC confidence** (2607.20712); **The Illusion of Safety** (2607.00107);
  **SRE-Bench** (2608.11469); **Binaries Talk Back** (2607.12507)
- **EmbedAgent** (arXiv 2506.11003); **Embedded Arena** (2606.16190); **FreeRTOS firmware study** (2509.09970)

## Tools that verified this work

GCC 16.1 (MSYS2), rustc 1.97.1, GDB 17.2, GNU as/ld/objdump/nm, CMake/Ninja, and Python 3.11
were used to execute-verify the source-backed examples in this repository. Without
reproducible toolchains, none of the "source-backed" claims would be credible.

---

If your work is represented here and you would like different attribution, or if any
attribution is inaccurate, please open an issue and we will correct it promptly.
