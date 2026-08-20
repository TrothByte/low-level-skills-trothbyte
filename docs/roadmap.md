# Roadmap

Public roadmap for **Low-level skills TrothByte**. Status and history live in
`roadmap/progress.yaml` (machine-readable) and `WORKLOG.md` (journal).

## Current state (2026-08-20)

- **185 skills В· 35 domains В· 93 source-backed В· 92 researched** (honestly marked).
- Registry: 278 primary sources, 184 traced claims, ~322 cross-links.
- Quality gates: `python tools/validate.py` (skill_lint + registry_check + source_check +
  claim_extractor + token gate), CI on every push/PR, generated-catalog and landing-data
  staleness checks.
- Distribution: GitHub Pages landing, Claude Code plugin marketplace, skills.sh badge,
  llms.txt, CITATION.cff, ready-to-publish npm/PyPI packages.
- v2.0: 33 new skills (gap set) + 6 design skills (designer-mode domain) + research-driven
  new-weakness skills; new domains `accelerator` and `design`.
- **v3.0: 22 new skills (agent-failure-mode set)** — MISRA compliance, agent deception
  detection, destructive-refactoring guard, compiler unstable-code detection (differential
  testing), ARM MTE, Rust-for-Linux modules, io_uring, BPF CO-RE, secure boot chain,
  hard real-time determinism, binary hardening flags, Checked C migration, ML-KEM/ML-DSA,
  Vulkan compute, reproducible firmware builds, USB device stack, PCIe config space,
  core-dump analysis, NAPI, symbolic execution (KLEE/angr), IEEE 754 semantics,
  endianness; new domain `safety`.

## Next phases

1. **Evals at scale (PHASE 15-21)** — run the per-skill synthetic / false-positive /
   historical-CVE / adversarial evals recorded in each `evals/README.md`; publish results.
2. **Routing & collision evals (PHASE 19-20)** — verify `meta-routing` selects the right
   minimal skill set and no two skills collide.
3. **Token optimization (PHASE 14)** — keep SKILL.md ~1-2K tokens, deepen `references/`.
4. **Elevate researched → source-backed** — on a Linux/GPU host with zig, nasm,
   clang-cross, qemu, nvcc, mpicc, valgrind, verilator, jadx/frida, frama-c/cbmc/kani,
   z3, and kernel toolchains installed. Each skill documents its exact command.
5. **License audit (PHASE 22)** — full pass over the 228 registered sources.
6. **Calibration (PHASE 21)** — fold evals back into skill guidance.

## Good first issues

See the [`good first issue`](https://github.com/TrothByte/low-level-skills-trothbyte/issues?q=is%3Aissue+is%3Aopen+label%3A%22good+first+issue%22)
label: landing-page filters, CI example compilation, i18n, and more.

## Contributing

Read `AGENTS.md` (engineering rules) and `CONTRIBUTING.md` first. New skills must be
source-traced, differentiated from existing ones, and gated by the validators.
