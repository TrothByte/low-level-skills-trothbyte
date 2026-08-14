<div align="center">

# Low-level skills TrothByte

**Production-grade low-level engineering knowledge for AI coding agents — verified, source-backed, and cheap to load.**

[![skills](https://img.shields.io/badge/skills-60-blueviolet?style=flat-square)](docs/SKILLS.md)
[![domains](https://img.shields.io/badge/domains-23-9cf?style=flat-square)](#skills-catalog)
[![source-backed](https://img.shields.io/badge/source--backed-47-success?style=flat-square)](registry/skills.yaml)
[![primary sources](https://img.shields.io/badge/primary--sources-62-informational?style=flat-square)](registry/sources.yaml)
[![traced claims](https://img.shields.io/badge/traced--claims-17-important?style=flat-square)](registry/claims.yaml)
[![license](https://img.shields.io/badge/license-MIT-brightgreen?style=flat-square)](LICENSE.md)

</div>

> [!NOTE]
> **60 skills · 23 domains · 47 source-backed · 62 primary sources · 17 traced claims.**
> AI agents writing C, C++, Rust, assembly, kernels, or firmware fail in predictable ways —
> they trust "it compiles", guess ABIs, ignore memory ordering, and skip verification.
> TrothByte exists to fix exactly those failures.

---

## 🎯 What is TrothByte

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

## 🚀 Quick start for an agent

1. Read [`AGENTS.md`](AGENTS.md) — engineering rules and resume protocol.
2. Route to the **minimal** skill set with [`meta-routing`](skills/_meta/meta-routing/SKILL.md).
3. Open that skill's `SKILL.md`; load its `references/` only as needed.
4. Verify like the skill says: warnings-as-errors, sanitizers, asm inspection, runtime asserts.

## 🗂️ Skills catalog

The complete index — what each skill does, its stability, and where it lives — is in
**[`docs/SKILLS.md`](docs/SKILLS.md)**. The area map below is your orientation.

| Area | Domains |
|---|---|
| 🔤 Languages & semantics | [`c`](skills/c/README.md) · [`cpp`](skills/cpp/README.md) · [`rust`](skills/rust/README.md) · [`concurrency`](skills/concurrency/README.md) |
| ⚙️ Compilers & IR | [`compiler`](skills/compiler/README.md) · [`llvm`](skills/llvm/README.md) |
| 🔩 Machine level | [`assembly`](skills/assembly/README.md) · [`abi`](skills/abi/README.md) · [`ffi`](skills/ffi/README.md) · [`elf`](skills/elf/README.md) · [`dwarf`](skills/dwarf/README.md) |
| 🧠 Systems engineering | [`kernel`](skills/kernel/README.md) · [`networking`](skills/networking/README.md) · [`embedded`](skills/embedded/README.md) · [`bootloader`](skills/bootloader/README.md) · [`qemu`](skills/qemu/README.md) |
| 🔬 Analysis & performance | [`binary-analysis`](skills/binary-analysis/README.md) · [`reverse-engineering`](skills/reverse-engineering/README.md) · [`performance`](skills/performance/README.md) · [`simd`](skills/simd/README.md) · [`gpu`](skills/gpu/README.md) |
| 🧰 Tooling & agent behavior | [`sanitizers`](skills/sanitizers/README.md) · [`_meta`](skills/_meta/README.md) |

### 🏆 Flagships to start with

| Skill | Solves |
|---|---|
| [`safe-low-level-from-scratch`](skills/_meta/safe-low-level-from-scratch/SKILL.md) | writing new code that is safe by design, not by fix |
| [`compiler-ub-assumptions`](skills/compiler/compiler-ub-assumptions/SKILL.md) | "works at -O0, breaks at -O2" |
| [`memory-ordering-reasoning`](skills/concurrency/memory-ordering-reasoning/SKILL.md) | races that compile and pass naive tests |
| [`abi-layout-reasoning`](skills/abi/abi-layout-reasoning/SKILL.md) | struct layout and calling conventions, verified with the compiler |
| [`ffi-boundary-cross-language`](skills/ffi/ffi-boundary-cross-language/SKILL.md) | where one language's safety guarantees end |

## 📂 Repository map

| Path | Purpose |
|---|---|
| `skills/` | 60 skills across 23 domains (`SKILL.md` + `references/` + `examples/` + `evals/`) |
| `registry/` | machine-readable state: skills, sources, claims, cross-links, tools, evals |
| `roadmap/` | coverage matrix, uniqueness analysis, priorities, live progress |
| `research/` | the original research documents this repository was built from |
| `docs/` | skill catalog, acknowledgments, architecture |
| `tools/` | shared scripts: validators, token measurement, generators |
| `AGENTS.md` | engineering rules and resume protocol |
| `WORKLOG.md` | development journal |

## 🛡️ Quality & provenance

> [!IMPORTANT]
> **Verification is executed, not asserted.** Examples were compiled and run with
> GCC 16.1, rustc 1.97.1, GDB 17.2, GNU as/ld/objdump, and Python 3.11. Where a toolchain
> is unavailable (NVIDIA CUDA, Linux eBPF, LLVM, QEMU, sanitizer runtimes), the skill is
> honestly marked `researched` with the exact target commands documented.

- **Every normative claim is traceable:** `claim → source → section → skill` in
  [`registry/claims.yaml`](registry/claims.yaml), backed by 62 primary sources in
  [`registry/sources.yaml`](registry/sources.yaml).
- **Quality gates** run on every change:
  [`skill_lint.py`](tools/lint/skill_lint.py) · [`registry_check.py`](tools/lint/registry_check.py) ·
  [`source_check.py`](tools/source/source_check.py) · [`token_measure.py`](tools/tokens/token_measure.py).
- **Token budget is measured.** A typical `SKILL.md` costs ~1–2K tokens; deep knowledge stays
  in `references/` and loads only when needed.

## 🧭 Stability levels

| Level | Meaning |
|---|---|
| `source-backed` | claims verified by execution on a real toolchain |
| `researched` | grounded in primary sources; verification needs a toolchain not present here |
| `evaluated` / `stable` | reached after evals and full review (next phases) |

## 🤝 Contributing

TrothByte is built as a product, not a pile of files. Before contributing, read
[`AGENTS.md`](AGENTS.md) (engineering rules) and [`CONTRIBUTING.md`](CONTRIBUTING.md).

## 📄 License

MIT — see [`LICENSE.md`](LICENSE.md). Attribution policy for the repositories, standards, talks,
and research this work builds on: [`docs/ACKNOWLEDGMENTS.md`](docs/ACKNOWLEDGMENTS.md).
