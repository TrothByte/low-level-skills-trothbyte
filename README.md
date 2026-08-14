# Low-level skills TrothByte

_Production-grade low-level engineering knowledge for AI agents — verified, source-backed, and cheap to load._

> 60 skills &nbsp;·&nbsp; 23 domains &nbsp;·&nbsp; 47 source-backed &nbsp;·&nbsp; 62 primary sources &nbsp;·&nbsp; 17 traced claims

---

## Why TrothByte

AI agents writing C, C++, Rust, assembly, kernels, or firmware routinely fail in the same
ways: they trust "it compiles", guess ABIs, ignore memory ordering, and skip verification.
TrothByte exists to fix exactly those failures.

Every skill answers the questions that matter before a single line is written:

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
3. Open that skill's `SKILL.md`, load its `references/` only as needed.
4. Verify like the skill says: warnings-as-errors, sanitizers, asm inspection, runtime asserts.

## Skills catalog

The complete index of every skill — what it does, its stability, and where it lives —
is in **[`docs/SKILLS.md`](docs/SKILLS.md)**. Use the area map below for orientation.

| Area | Domains |
|---|---|
| **Languages & semantics** | [`c`](skills/c/README.md) · [`cpp`](skills/cpp/README.md) · [`rust`](skills/rust/README.md) · [`concurrency`](skills/concurrency/README.md) |
| **Compilers & IR** | [`compiler`](skills/compiler/README.md) · [`llvm`](skills/llvm/README.md) |
| **Machine level** | [`assembly`](skills/assembly/README.md) · [`abi`](skills/abi/README.md) · [`ffi`](skills/ffi/README.md) · [`elf`](skills/elf/README.md) · [`dwarf`](skills/dwarf/README.md) |
| **Systems engineering** | [`kernel`](skills/kernel/README.md) · [`networking`](skills/networking/README.md) · [`embedded`](skills/embedded/README.md) · [`bootloader`](skills/bootloader/README.md) · [`qemu`](skills/qemu/README.md) |
| **Analysis & performance** | [`binary-analysis`](skills/binary-analysis/README.md) · [`reverse-engineering`](skills/reverse-engineering/README.md) · [`performance`](skills/performance/README.md) · [`simd`](skills/simd/README.md) · [`gpu`](skills/gpu/README.md) |
| **Tooling & agent behavior** | [`sanitizers`](skills/sanitizers/README.md) · [`_meta`](skills/_meta/README.md) |

Start with the cross-layer flagships:

| Skill | Solves |
|---|---|
| [`safe-low-level-from-scratch`](skills/_meta/safe-low-level-from-scratch/SKILL.md) | writing new code that is safe by design, not by fix |
| [`compiler-ub-assumptions`](skills/compiler/compiler-ub-assumptions/SKILL.md) | "works at -O0, breaks at -O2" |
| [`memory-ordering-reasoning`](skills/concurrency/memory-ordering-reasoning/SKILL.md) | races that compile and pass naive tests |
| [`abi-layout-reasoning`](skills/abi/abi-layout-reasoning/SKILL.md) | struct layout and calling conventions, verified with the compiler |
| [`ffi-boundary-cross-language`](skills/ffi/ffi-boundary-cross-language/SKILL.md) | where one language's safety guarantees end |

## Repository map

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

## Quality & provenance

- **Verification is executed, not asserted.** Examples in this repository were compiled and
  run with GCC 16.1, rustc 1.97.1, GDB 17.2, GNU as/ld/objdump, and Python 3.11. Where a
  toolchain is unavailable (NVIDIA CUDA, Linux eBPF, LLVM, QEMU, sanitizer runtimes), the
  skill is honestly marked `researched` with the exact target commands documented.
- **Every normative claim is traceable:** `claim → source → section → skill` in
  `registry/claims.yaml`, backed by 62 primary sources in `registry/sources.yaml`.
- **Quality gates** run on every change:
  `tools/lint/skill_lint.py` · `tools/lint/registry_check.py` · `tools/source/source_check.py` ·
  `tools/tokens/token_measure.py`.
- **Token budget is measured.** A typical `SKILL.md` costs ~1–2K tokens; deep knowledge
  stays in `references/` and loads only when needed.

## Stability levels

| Level | Meaning |
|---|---|
| `source-backed` | claims verified by execution on a real toolchain |
| `researched` | grounded in primary sources; verification requires a toolchain not present in this environment |
| `evaluated` / `stable` | reached after evals and full review (next phases) |

## License

MIT — see [`LICENSE.md`](LICENSE.md). Attribution policy for the repositories, standards, talks,
and research this work builds on: [`docs/ACKNOWLEDGMENTS.md`](docs/ACKNOWLEDGMENTS.md).
