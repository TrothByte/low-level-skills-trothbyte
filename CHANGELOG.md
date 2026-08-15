# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- GitHub Actions CI running the full validator suite (`tools/validate.py`).
- `.editorconfig`, `.gitattributes`, `requirements-dev.txt`, `CHANGELOG.md`.

### Removed
- `.pre-commit-config.yaml`.

## [0.2.0] — 2026-08-15

### Added
- **64 new skills (A–W)** across 17 new or expanded domains, derived from three failure
  surveys (agent failures, external-repo audit, assembly hallucinations):
  - *Assembly (×5)*: instruction-verification gate, NASM/GAS/AT&T dialects, Thumb-2
    encoding, AArch64 NEON safety, RISC-V calling conventions.
  - *Binary analysis / RE (×5)*: decompilation fidelity, shellcode analysis, CAN signal
    extraction, Ghidra automation, VM allocator diagnosis.
  - *Concurrency (×1)*: actual-parallelism detection (fake-parallelism gate).
  - *Rust (×4)*: API evolution/drift, dependency supply chain, crypto primitives safety,
    unsafe safety-contract verification.
  - *Embedded (×6)*: hardware-register verification, devicetree/Kconfig, board bring-up,
    flash/debug cycle, OTA safety, HIL CI.
  - *Kernel (×6)*: scheduler/MM/VFS internals, ftrace/kprobes/kdump debugging, container
    internals, char-device lifecycle, out-of-tree modules, API-drift migration.
  - *New domains*: **Zig (×11)**, build-systems (×4), debugging (×2), HPC (×3), HDL/FPGA
    (×3), RISC-V incl. CHERI (×2), Android RE (×2), virtualization (×1), security/formal
    (×3), plus UEFI firmware, fuzzing evidence gate, RDMA/NIC offload, GPU kernel
    verification & communication, and a meta harness-validity skill.
- Registry expanded: **124 skills**, 177 primary sources, 56 traced claims, ~70 new
  cross-links; 9 new domain READMEs; `docs/SKILLS.md` regenerated.

### Changed
- README statistics updated (124 skills · 32 domains · 65 source-backed · 177 sources ·
  56 claims).
- `.gitignore` extended with build-artifact patterns.

## [0.1.0] — 2026-08-14

### Added
- Initial release: 60 skills across 23 domains (47 source-backed), the registry
  (skills/sources/claims/cross-links/evals), the validator tooling
  (`skill_lint`, `registry_check`, `source_check`, `token_measure`), domain READMEs,
  docs (architecture, SKILLS catalog, acknowledgments), and the research/roadmap state.
