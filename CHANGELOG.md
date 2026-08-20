# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- **Phase 1 expansion pipeline (×8 kernel/networking core skills)** — all
  source-backed with host-verified compile-and-run examples (MinGW gcc 16.1):
  - *kernel*: vfs-file-operations-and-fops (fops dispatch/return contract),
    page-cache-writeback-semantics (fsync/dirty/writeback model),
    workqueue-flush-and-cancellation (cancel_work_sync vs cancel_work,
    flush-from-work deadlock), kernel-timers-hrtimer-vs-legacy (timer_list vs
    hrtimer context and teardown), waitqueue-completion-synchronization
    (lost wakeup, complete vs complete_all), kthread-create-and-teardown
    (kthread_stop contract, unload ordering).
  - *networking*: sk-buff-socket-buffer-management (head/data/tail layout,
    skb_put/clone/copy/share), tcp-congestion-control-internals (RFC 5681
    slow start/congestion avoidance, fast retransmit, CUBIC).
- **12 new primary sources**: linux-vfs-docs, linux-writeback-docs,
  kernel-workqueue-docs, kernel-timers-docs, kernel-completion-docs,
  kernel-kthread-docs, linux-skbuff-docs, linux-networking-docs, linux-tcp-docs,
  rfc-793, rfc-5681, rfc-8312.
- Registry now: **193 skills**, 290 primary sources, 248 traced claims
  (CL-185..CL-248), ~378 cross-links (+56 edges).
- **v3.0 agent-failure-mode set (×22 skills)** across 1 new domain (`safety`, 35 total):
  - *CRITICAL*: misra-c-compliance (Top-k rules for LLM agents, MISRA C:2012/C++:2023,
    host checker), agent-deception-detection (raw-evidence gate vs fabricated output),
    destructive-refactoring-guard (diff/LOC/compile-before-delete gates),
    compiler-unstable-code-detection (differential testing across -O levels/compilers),
    arm-mte-programming (SYNC/ASYNC/ASYMM, 16B granules, IRG/LDG/STG, TBI, prctl).
  - *HIGH*: rust-for-linux-module-dev, io-uring-interface, bpf-core-relocation,
    secure-boot-chain, hard-real-time-determinism, binary-hardening-flags,
    checked-c-migration, post-quantum-crypto-mlkem (ML-KEM/ML-DSA, FIPS 203/204),
    vulkan-compute-shaders, reproducible-builds-firmware.
  - *MEDIUM*: usb-device-stack, pcie-config-space, core-dump-analysis,
    napi-network-driver, symbolic-execution-klee-angr.
  - *Gap-analysis extras*: floating-point-ieee-semantics, endianness-and-byte-order.
- **New domain**: `safety` (MISRA compliance + hard real-time determinism).
- Registry now: **185 skills**, 278 primary sources, 184 traced claims (CL-142..CL-184),
  ~322 cross-links (+89 edges).
- Web-research grounding: MISRA LLM studies (RS-8123173, arXiv 2506.23535), agent
  deception research (PNAS 2317967121, arXiv 2509.03518/2509.18970), differential
  compiler testing (UBfuzz ASPLOS'24, DiffSpec, DESIL), Checked C (3C, arXiv 2203.13445).

### Changed
- README statistics updated (185 skills · 35 domains · 93 source-backed · 278 sources ·
  184 claims); landing page, `llms.txt`, `docs/architecture.md`, `docs/roadmap.md`,
  `docs/agents-failures-cheatsheet.md` regenerated/updated; `safety` added to area map.

### Added (v2.0)
- **v2.0 skill format migration**: 9 required sections enforced as errors, body ≤250
  lines, description ≤50 words, token gate ≤2000 activation tokens.
- **39 new skills** across 2 new domains:
  - *v2.0 gap set (×33)*: memory-model-arm-x86-riscv, page-table-management,
    dma-cache-coherency, capability-based-security, toctou-kernel,
    formal-verification-kani-verus, interrupt-controller-gic-apic, iommu-smmu-isolation,
    side-channel-mitigation, sel4-sddf-driver-framework, kernel-exploitation-primitives,
    invariant-identification, data-race-kernel-detection, deadlock-kernel-prevention,
    fuzzing-harness-kernel, agent-scope-management, framekernel-architecture,
    bootloader-uefi-acpi-dtb, kernel-loader-elf, property-based-testing-kernel,
    agent-tool-whitelist, kernel-ub-patterns, hardware-register-bringup, meta-eval-runner,
    meta-claim-extraction, meta-token-optimization, meta-security-audit,
    meta-formal-verification, gpu-kernel-reward-hacking-detection,
    kernel-patch-review-commit-log-independence, accelerator-pipeline-synchronization,
    llm-verifier-warning-disposition, ebpf-verifier-opaque-feedback-iteration.
  - *Design (×6, designer mode)*: design-token-system-discipline,
    design-typography-hierarchy, design-color-contrast-wcag-a11y, design-layout-spacing-grid,
    design-visual-hierarchy-composition, design-anti-ai-look-originality-review.
- **New domains**: `accelerator`, `design` (34 total).
- **New validators**: `claim_extractor.py`, `prose_lint.py`; cycle detection + tools.yaml
  gate in `registry_check.py`; blended token measurement + `--check` mode in
  `token_measure.py`.
- Registry now: **163 skills**, 228 primary sources, 141 traced claims, 233 cross-links.

### Changed (v2.0)
- README statistics updated (163 skills · 34 domains · 85 source-backed · 228 sources ·
  141 claims); landing page, `llms.txt`, `docs/architecture.md`, `docs/roadmap.md`,
  `docs/agents-failures-cheatsheet.md` regenerated/updated.
- CI: added token budget gate and domain-README staleness check.

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
