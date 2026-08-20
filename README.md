<div align="center">

# Low-level skills TrothByte

**Production-grade low-level engineering knowledge for AI coding agents — verified, source-backed, and cheap to load.**

<sub>Follows the [Agent Skills](https://agentskills.io) open standard — works with Claude Code, Cursor, GitHub Copilot, VS Code, Gemini CLI, OpenCode, Codex, and other Agent Skills clients.</sub>

[![skills](https://img.shields.io/badge/skills-185-0F766E?style=flat-square)](docs/SKILLS.md)
[![domains](https://img.shields.io/badge/domains-35-475569?style=flat-square)](#skills-catalog)
[![source-backed](https://img.shields.io/badge/source--backed-93-15803D?style=flat-square)](registry/skills.yaml)
[![primary sources](https://img.shields.io/badge/primary--sources-278-1D4ED8?style=flat-square)](registry/sources.yaml)
[![traced claims](https://img.shields.io/badge/traced--claims-184-9D174D?style=flat-square)](registry/claims.yaml)
[![license](https://img.shields.io/badge/license-MIT-brightgreen?style=flat-square)](LICENSE.md)
[![CI](https://img.shields.io/github/actions/workflow/status/TrothByte/low-level-skills-trothbyte/ci.yml?style=flat-square&label=CI)](https://github.com/TrothByte/low-level-skills-trothbyte/actions)
[![skills.sh](https://skills.sh/b/TrothByte/low-level-skills-trothbyte)](https://skills.sh/TrothByte/low-level-skills-trothbyte)

</div>

> [!NOTE]
> **185 skills · 35 domains · 93 source-backed · 278 primary sources · 184 traced claims.**
> AI agents writing C, C++, Rust, assembly, kernels, or firmware fail in predictable ways —
> they trust "it compiles", guess ABIs, ignore memory ordering, and skip verification.
> TrothByte exists to fix exactly those failures.

---

## What is TrothByte

Every skill answers the questions that matter **before** a single line is written:

1. **When to use** and **when not to** — so an agent loads the right tool and nothing else.
2. **What the agent often gets wrong** — the systematic mistakes, named and catalogued.
3. **How to reason correctly** — the positive reasoning process, not just "don't do X".
4. **What to verify** and **how** — executable gates, not vibes.
5. **Where the knowledge comes from** — every normative claim traces to a primary source.

```text
SKILL.md          small, operational, loads first
  └─ references/  deep knowledge, loads on demand (progressive disclosure)
     └─ examples/ good|bad, compiled and run against real toolchains
        └─ evals/ synthetic · false-positive · historical CVE · adversarial
```

## Quick start for an agent

1. Read [`AGENTS.md`](AGENTS.md) — engineering rules and resume protocol.
2. Route to the **minimal** skill set with [`meta-routing`](skills/_meta/meta-routing/SKILL.md).
3. Open that skill's `SKILL.md`; load its `references/` only as needed.
4. Verify like the skill says: warnings-as-errors, sanitizers, asm inspection, runtime asserts.

## Install as an agent-skill pack

```bash
# Clone the whole pack (works with any agent skills runtime)
git clone https://github.com/TrothByte/low-level-skills-trothbyte

# Or install via skills.sh
npx skills add TrothByte/low-level-skills-trothbyte

# Or as a Claude Code plugin marketplace
/plugin marketplace add TrothByte/low-level-skills-trothbyte
/plugin install low-level-skills@trothbyte-low-level-skills
```

Docs site: **[trothbyte.github.io/low-level-skills-trothbyte](https://trothbyte.github.io/low-level-skills-trothbyte)**

## Skills catalog

The complete index — what each skill does, its stability, and where it lives — is in
**[`docs/SKILLS.md`](docs/SKILLS.md)**. The area map below is your orientation.

| Area | Domains |
|---|---|
| Languages & semantics | [`c`](skills/c/README.md) · [`cpp`](skills/cpp/README.md) · [`rust`](skills/rust/README.md) · [`concurrency`](skills/concurrency/README.md) · [`zig`](skills/zig/README.md) |
| Compilers & IR | [`compiler`](skills/compiler/README.md) · [`llvm`](skills/llvm/README.md) |
| Machine level | [`assembly`](skills/assembly/README.md) · [`abi`](skills/abi/README.md) · [`ffi`](skills/ffi/README.md) · [`elf`](skills/elf/README.md) · [`dwarf`](skills/dwarf/README.md) |
| Systems engineering | [`kernel`](skills/kernel/README.md) · [`networking`](skills/networking/README.md) · [`embedded`](skills/embedded/README.md) · [`bootloader`](skills/bootloader/README.md) · [`qemu`](skills/qemu/README.md) · [`virtualization`](skills/virtualization/README.md) · [`riscv`](skills/riscv/README.md) |
| Binary analysis & RE | [`binary-analysis`](skills/binary-analysis/README.md) · [`reverse-engineering`](skills/reverse-engineering/README.md) · [`mobile`](skills/mobile/README.md) |
| Performance & HPC | [`performance`](skills/performance/README.md) · [`simd`](skills/simd/README.md) · [`gpu`](skills/gpu/README.md) · [`hpc`](skills/hpc/README.md) |
| Security, safety & hardware | [`security`](skills/security/README.md) · [`safety`](skills/safety/README.md) · [`hdl`](skills/hdl/README.md) |
| Accelerators | [`accelerator`](skills/accelerator/README.md) |
| Design (designer mode) | [`design`](skills/design/README.md) |
| Tooling & agent behavior | [`sanitizers`](skills/sanitizers/README.md) · [`build-systems`](skills/build-systems/README.md) · [`debugging`](skills/debugging/README.md) · [`_meta`](skills/_meta/README.md) |

### Flagships to start with

| Skill | Solves |
|---|---|
| [`safe-low-level-from-scratch`](skills/_meta/safe-low-level-from-scratch/SKILL.md) | writing new code that is safe by design, not by fix |
| [`compiler-ub-assumptions`](skills/compiler/compiler-ub-assumptions/SKILL.md) | "works at -O0, breaks at -O2" |
| [`memory-ordering-reasoning`](skills/concurrency/memory-ordering-reasoning/SKILL.md) | races that compile and pass naive tests |
| [`abi-layout-reasoning`](skills/abi/abi-layout-reasoning/SKILL.md) | struct layout and calling conventions, verified with the compiler |
| [`ffi-boundary-cross-language`](skills/ffi/ffi-boundary-cross-language/SKILL.md) | where one language's safety guarantees end |
| [`agent-deception-detection`](skills/_meta/agent-deception-detection/SKILL.md) | agents fabricating evidence, logs, or git history |
| [`compiler-unstable-code-detection`](skills/compiler/compiler-unstable-code-detection/SKILL.md) | code that behaves differently across compilers/-O levels |
| [`misra-c-compliance`](skills/safety/misra-c-compliance/SKILL.md) | safety-critical C/C++ that must pass MISRA rule sets |

## v3.0: agent-failure-mode skills

22 new skills target the failure classes that dominate real agent incidents —
research-grounded, host-verified where possible:

- **CRITICAL**: [`misra-c-compliance`](skills/safety/misra-c-compliance/SKILL.md),
  [`agent-deception-detection`](skills/_meta/agent-deception-detection/SKILL.md),
  [`destructive-refactoring-guard`](skills/_meta/destructive-refactoring-guard/SKILL.md),
  [`compiler-unstable-code-detection`](skills/compiler/compiler-unstable-code-detection/SKILL.md),
  [`arm-mte-programming`](skills/embedded/arm-mte-programming/SKILL.md)
- **HIGH**: [`rust-for-linux-module-dev`](skills/kernel/rust-for-linux-module-dev/SKILL.md),
  [`io-uring-interface`](skills/kernel/io-uring-interface/SKILL.md),
  [`bpf-core-relocation`](skills/networking/bpf-core-relocation/SKILL.md),
  [`secure-boot-chain`](skills/security/secure-boot-chain/SKILL.md),
  [`hard-real-time-determinism`](skills/safety/hard-real-time-determinism/SKILL.md),
  [`binary-hardening-flags`](skills/security/binary-hardening-flags/SKILL.md),
  [`checked-c-migration`](skills/c/checked-c-migration/SKILL.md),
  [`post-quantum-crypto-mlkem`](skills/security/post-quantum-crypto-mlkem/SKILL.md),
  [`vulkan-compute-shaders`](skills/gpu/vulkan-compute-shaders/SKILL.md),
  [`reproducible-builds-firmware`](skills/embedded/reproducible-builds-firmware/SKILL.md)
- **MEDIUM**: [`usb-device-stack`](skills/embedded/usb-device-stack/SKILL.md),
  [`pcie-config-space`](skills/embedded/pcie-config-space/SKILL.md),
  [`core-dump-analysis`](skills/debugging/core-dump-analysis/SKILL.md),
  [`napi-network-driver`](skills/networking/napi-network-driver/SKILL.md),
  [`symbolic-execution-klee-angr`](skills/security/symbolic-execution-klee-angr/SKILL.md)
- **Gap-analysis extras**: [`floating-point-ieee-semantics`](skills/c/floating-point-ieee-semantics/SKILL.md),
  [`endianness-and-byte-order`](skills/c/endianness-and-byte-order/SKILL.md)

## Repository map

| Path | Purpose |
|---|---|
| `skills/` | 185 skills across 35 domains (`SKILL.md` + `references/` + `examples/` + `evals/`) |
| `registry/` | machine-readable state: skills, sources, claims, cross-links, tools, evals |
| `roadmap/` | coverage matrix, uniqueness analysis, priorities, live progress |
| `research/` | the original research documents this repository was built from |
| `docs/` | skill catalog, [evidence](docs/evidence.md), [roadmap](docs/roadmap.md), acknowledgments, architecture |
| `tools/` | shared scripts: validators, token measurement, generators |
| `publish/` | ready-to-publish npm & PyPI packages (`trothbyte-skills install`) |
| `.github/workflows/` | CI: runs `tools/validate.py` on every push and PR |
| `AGENTS.md` | engineering rules and resume protocol |
| `CONTRIBUTING.md` · `SECURITY.md` · `CHANGELOG.md` | contribution, security, and release metadata |
| `WORKLOG.md` | development journal |

## Quality & provenance

> [!IMPORTANT]
> **Verification is executed, not asserted.** Examples were compiled and run with
> GCC 16.1, rustc 1.97.1, GDB 17.2, GNU as/ld/objdump, and Python 3.11. Where a toolchain
> is unavailable (NVIDIA CUDA, Linux eBPF, LLVM, QEMU, sanitizer runtimes), the skill is
> honestly marked `researched` with the exact target commands documented.

- **Every normative claim is traceable:** `claim → source → section → skill` in
  [`registry/claims.yaml`](registry/claims.yaml), backed by 278 primary sources in
  [`registry/sources.yaml`](registry/sources.yaml).
- **Quality gates** run on every change — locally via [`tools/validate.py`](tools/validate.py)
  (skill_lint · registry_check · source_check · claim_extractor · token gate) and in CI
  ([`.github/workflows/ci.yml`](.github/workflows/ci.yml)) on every push and PR.
- **Token budget is enforced.** A typical `SKILL.md` costs ~1–2K tokens (hard gate: ≤2000
  activation cost, measured with [`token_measure.py`](tools/tokens/token_measure.py));
  deep knowledge stays in `references/` and loads only when needed.

## Stability levels

| Level | Meaning |
|---|---|
| `source-backed` | claims verified by execution on a real toolchain |
| `researched` | grounded in primary sources; verification needs a toolchain not present here |
| `evaluated` / `stable` | reached after evals and full review (next phases) |

## Contributing

TrothByte is built as a product, not a pile of files. Before contributing, read
[`AGENTS.md`](AGENTS.md) (engineering rules) and [`CONTRIBUTING.md`](CONTRIBUTING.md).

## License

MIT — see [`LICENSE.md`](LICENSE.md). Attribution policy for the repositories, standards, talks,
and research this work builds on: [`docs/ACKNOWLEDGMENTS.md`](docs/ACKNOWLEDGMENTS.md).

## Cite

Academic or benchmark use? Cite via [`CITATION.cff`](CITATION.cff) (GitHub renders a
"Cite this repository" button automatically; Zenodo picks it up on releases).
Verification evidence: [`docs/evidence.md`](docs/evidence.md).
