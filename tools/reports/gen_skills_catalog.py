#!/usr/bin/env python3
"""gen_skills_catalog.py — generate docs/SKILLS.md: a catalog of every skill.

Groups the 60 skills by area and domain, with description, type, stability, tier,
and path for each. Source: registry/skills.yaml + each skill's SKILL.md frontmatter.

Usage: python tools/reports/gen_skills_catalog.py [repo-root]
"""
import os
import re
import sys

import yaml

AREAS = [
    ("Languages & semantics", ["c", "cpp", "rust", "concurrency", "zig"]),
    ("Compilers & IR", ["compiler", "llvm"]),
    ("Machine level", ["assembly", "abi", "ffi", "elf", "dwarf"]),
    ("Systems engineering", ["kernel", "networking", "embedded", "bootloader", "qemu", 
                              "virtualization", "riscv"]),
    ("Binary analysis & RE", ["binary-analysis", "reverse-engineering", "mobile"]),
    ("Performance & HPC", ["performance", "simd", "gpu", "hpc"]),
    ("Tooling & agent behavior", ["sanitizers", "_meta", "build-systems", "debugging"]),
    ("Security & hardware", ["security", "hdl"]),
    ("Accelerators", ["accelerator"]),
    ("Design", ["design"]),
]

DOMAIN_INTROS = {
    "c": "C's sharp edges: undefined behavior, integer promotion, strings and buffers, errno, signal handlers.",
    "cpp": "Object lifetimes, move semantics, and positive RAII/descriptor-type API design.",
    "rust": "Unsafe semantics, FFI boundaries, and panic safety — the three places Rust can still be wrong.",
    "concurrency": "Memory ordering, the atomics API, and lock/condvar discipline.",
    "compiler": "How compilers interpret undefined behavior — the root of '-O0 works, -O2 breaks'.",
    "llvm": "Reading LLVM IR and writing passes.",
    "assembly": "x86-64 registers, calling conventions, inline asm constraints, signed/unsigned branches, optimizer artifacts.",
    "abi": "Struct layout and argument passing for SysV AMD64, AAPCS64, RISC-V psABI — verified with the compiler.",
    "ffi": "Cross-language boundaries: layout, ownership, errors, and the no-unwind rule.",
    "elf": "The ELF pipeline: layout, relocations, GOT/PLT dynamic linking.",
    "dwarf": "Debug info and debugging optimized builds.",
    "kernel": "uaccess safety, RCU and memory barriers, atomic-context rules.",
    "networking": "The eBPF verifier model.",
    "embedded": "Volatile/MMIO ordering, interrupts, linker scripts, MPU/TrustZone, RTOS ISR discipline.",
    "bootloader": "Boot stages and relocation.",
    "qemu": "System emulation for kernel, firmware, and bare-metal verification.",
    "performance": "Measure-before-optimize discipline and cache/NUMA-aware code.",
    "simd": "Auto-vectorization, blockers, intrinsics.",
    "gpu": "GPU coherence scopes and PTX assembly.",
    "sanitizers": "The agent CI loop and reading sanitizer reports.",
    "binary-analysis": "Type recovery from disassembly.",
    "reverse-engineering": "Go/Rust binaries and automated protocol RE.",
    "_meta": "Agent behavior: routing, evidence, verification, assumptions, rationalizations, completion — plus the flagship cross-layer skills.",
    "build-systems": "Build systems turn source into binaries: CMake diagnostics, toolchain drift, linker errors.",
    "debugging": "Crash triage and instrumentation-over-reasoning — log before you guess.",
    "virtualization": "Hypervisor internals (VMX/SVM), KVM ioctls, paravirtualization.",
    "riscv": "RISC-V ISA, vector extensions (RVV), and CHERI capability-based safety.",
    "hpc": "MPI parallel programming, OpenMP offload, RDMA verbs for high-performance compute.",
    "mobile": "Android reverse engineering: DEX/Smali format extraction and JNI/native layer analysis.",
    "security": "Side-channel constant-time verification, formal spec loop invariants, SMT/Z3 sound usage.",
    "hdl": "Clock domain crossing audit, timing closure, and constraint authoring for FPGA design.",
    "accelerator": "AI accelerator pipeline programs: cross-unit (DMA/vector/matrix/scalar) synchronization and barrier coverage on shared on-chip buffers.",
    "design": "Designer-mode skills for AI agents: design tokens (DTCG), typography hierarchy, WCAG 2.2 color/contrast accessibility, layout grids and reflow, visual hierarchy, and anti-AI-look originality review.",
}


def frontmatter(path):
    try:
        text = open(path, encoding="utf-8").read()
    except OSError:
        return None, None
    m = re.match(r"^---\n(.*?)\n---\n", text, re.S)
    if not m:
        return None, None
    name = re.search(r"^name:\s*(.+)$", m.group(1), re.M)
    desc = re.search(r"^description:\s*(.+)$", m.group(1), re.M)
    return (name.group(1).strip() if name else None,
            desc.group(1).strip() if desc else None)


def main():
    root = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", ".."))
    registry = yaml.safe_load(open(os.path.join(root, "registry", "skills.yaml"), encoding="utf-8"))
    skills_dir = os.path.join(root, "skills")

    by_domain = {}
    for sk in registry["skills"]:
        parts = sk["path"].split("/")
        by_domain.setdefault(parts[1], []).append(sk)

    total = len(registry["skills"])
    sb = sum(1 for s in registry["skills"] if s.get("stability") == "source-backed")
    lines = [
        "# Low-level skills TrothByte — Skill Catalog",
        "",
        f"All **{total} skills** in one place. {sb} are source-backed (verified with real toolchains); "
        "the rest are researched and honestly marked. For orientation by domain, see the domain "
        "READMEs under `skills/`; for triggers and rules, open each skill's `SKILL.md`.",
        "",
        "**Stability:** `source-backed` = claims verified by execution (compilers, sanitizers, asm, "
        "debuggers); `researched` = content grounded in primary sources but requiring a toolchain not "
        "available in this repository's dev environment.",
        "",
    ]

    for area, domains in AREAS:
        lines.append(f"## {area}")
        lines.append("")
        for d in domains:
            skills = sorted(by_domain.get(d, []), key=lambda s: s["id"])
            if not skills:
                continue
            lines.append(f"### {d}")
            lines.append("")
            lines.append(DOMAIN_INTROS.get(d, ""))
            lines.append("")
            lines.append("| Skill | What it does | Type | Stability | Path |")
            lines.append("|---|---|---|---|---|")
            for sk in skills:
                skmd = os.path.join(skills_dir, d, os.path.basename(sk["path"]), "SKILL.md")
                _, desc = frontmatter(skmd)
                desc = desc or "—"
                lines.append(
                    f"| `{sk['id']}` | {desc} | {sk.get('type', '')} | {sk.get('stability', '')} "
                    f"| `{sk['path']}` |"
                )
            lines.append("")
        lines.append("")

    lines.extend([
        "---",
        "",
        "Generated by `tools/reports/gen_skills_catalog.py` from `registry/skills.yaml`.",
        "",
    ])

    out = os.path.join(root, "docs", "SKILLS.md")
    open(out, "w", encoding="utf-8").write("\n".join(lines))
    print(f"Wrote {out}: {total} skills, {sb} source-backed")


if __name__ == "__main__":
    main()
