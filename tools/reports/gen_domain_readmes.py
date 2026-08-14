#!/usr/bin/env python3
"""gen_domain_readmes.py — regenerate skills/<domain>/README.md files.

Each domain README gives a neural-network (or human) consumer a quick orientation:
what the domain covers, what each skill does, its stability, and where to look.

Usage: python tools/reports/gen_domain_readmes.py [repo-root]
Reads registry/skills.yaml (paths) + each skill's SKILL.md frontmatter description.
"""
import os
import re
import sys

import yaml

DOMAIN_INTROS = {
    "c": (
        "C is the lingua franca of low-level engineering. These skills cover the places where C silently breaks — "
        "undefined behavior, integer promotion, strings and buffers, errno and syscall returns, and signal handlers. "
        "Use them when writing, reviewing, or debugging C that must survive -O2 and real deployments."
    ),
    "cpp": (
        "C++ adds object lifetimes and RAII on top of C's sharp edges. These skills cover object lifecycle, move "
        "semantics, and — uniquely — positive API design: descriptor types, RAII wrappers, typed errors, and builders "
        "that make misuse unrepresentable."
    ),
    "rust": (
        "Rust makes memory safety the default, but unsafe blocks move the burden onto the author. These skills teach "
        "unsafe semantics (aliasing, validity, provenance), FFI boundaries, and panic safety — the three places where "
        "Rust code can still be wrong."
    ),
    "concurrency": (
        "Concurrency bugs compile. These skills cover the memory-ordering model (Relaxed/Acquire/Release/SeqCst), "
        "the C11/C++11/Rust atomics API, and lock/condvar discipline — so you reason about happens-before edges "
        "instead of guessing."
    ),
    "compiler": (
        "The compiler is the agent's least-trusted and least-understood colleague. This domain explains how compilers "
        "interpret undefined behavior — the single most common cause of 'works at -O0, breaks at -O2'."
    ),
    "llvm": (
        "LLVM is the reference compiler infrastructure. These skills teach reading LLVM IR and writing passes — "
        "the entry points for compiler engineering work."
    ),
    "assembly": (
        "Assembly is the ground truth every higher language compiles to. These skills cover x86-64 registers and "
        "addressing, calling conventions, inline asm constraints, signed/unsigned branches, and optimizer artifacts "
        "in disassembly."
    ),
    "abi": (
        "The ABI is the contract between compilers, libraries, and languages. This skill teaches computing struct "
        "layout and argument passing for SysV AMD64, AAPCS64, and RISC-V — and how to verify every claim with the compiler."
    ),
    "ffi": (
        "FFI is where one language's safety guarantees end. These skills cover cross-language boundaries: layout "
        "pinning, ownership transfer, error translation, and the no-unwind rule."
    ),
    "elf": (
        "ELF is the binary format of Linux and friends. These skills trace the whole pipeline: layout, relocations, "
        "GOT/PLT dynamic linking, and how the linker and debugger cooperate."
    ),
    "dwarf": (
        "DWARF is the debug information format. This skill teaches reading debug info and — critically — why optimized "
        "builds show 'value optimized out' and how to debug them anyway."
    ),
    "kernel": (
        "The Linux kernel is the largest low-level system in the world. These skills cover uaccess safety, RCU and "
        "memory barriers, and atomic-context rules — the mistakes that become CVE-2022-0185 and Dirty COW."
    ),
    "networking": (
        "High-performance networking is kernel-adjacent. These skills cover the eBPF verifier model — what programs "
        "load and why, from the verifier's perspective."
    ),
    "embedded": (
        "Embedded development runs on bare metal with MMIO, interrupts, and linker scripts. These skills cover volatile/"
        "memory ordering, interrupts and nesting, linker scripts, MPU/TrustZone security, and RTOS ISR discipline."
    ),
    "bootloader": (
        "Bootloaders own the first instructions of a machine. This skill covers the boot stages — real mode to long "
        "mode, AArch64/RISC-V handoff, and relocation — where link addresses and load addresses diverge."
    ),
    "qemu": (
        "QEMU is the universal verification host for code you cannot yet run on real hardware. This skill covers "
        "machine models, kernels, firmware, and gdb stubs for every emulation workflow."
    ),
    "performance": (
        "Performance work starts with measurement. These skills enforce the measure-before-optimize discipline and "
        "teach cache/NUMA-aware code that actually moves real timings."
    ),
    "simd": (
        "SIMD is where CPUs get fast. These skills bridge compiler auto-vectorization and hand-written vector code: "
        "reading -fopt-info, finding blockers, and reasoning about alignment and aliasing."
    ),
    "gpu": (
        "GPUs have a memory model of their own. These skills cover GPU coherence scopes, the PTX assembly language, "
        "and why CPU reasoning fails on the GPU."
    ),
    "sanitizers": (
        "Sanitizers turn 'probably fine' into evidence. These skills teach the agent CI loop (build→run→parse→dedupe→"
        "track) and how to read ASan/UBSan/TSan/MSan reports."
    ),
    "binary-analysis": (
        "Reading binaries means recovering what the compiler encoded. This skill teaches type recovery from "
        "disassembly — pointer vs integer, struct offsets, function signatures."
    ),
    "reverse-engineering": (
        "Reverse engineering covers Go and Rust binaries with their special metadata (pclntab, panic strings) and "
        "automated protocol reverse engineering with a verify-as-gate."
    ),
    "_meta": (
        "Meta-skills govern agent behavior: routing to the minimal skill set, evidence discipline (KNOWN/INFERRED/"
        "UNVERIFIED), verification gates, surfacing assumptions, rejecting rationalizations, and honest completion. "
        "Also here: the flagship cross-layer skills safe-low-level-from-scratch, zeroize-constant-time, and "
        "wasm-runtime-from-scratch."
    ),
}

AREAS = [
    ("Languages & semantics", ["c", "cpp", "rust", "concurrency"]),
    ("Compilers & IR", ["compiler", "llvm"]),
    ("Machine level", ["assembly", "abi", "ffi", "elf", "dwarf"]),
    ("Systems engineering", ["kernel", "networking", "embedded", "bootloader", "qemu"]),
    ("Analysis & performance", ["binary-analysis", "reverse-engineering", "performance", "simd", "gpu"]),
    ("Tooling & agent behavior", ["sanitizers", "_meta"]),
]


def extract_frontmatter(path):
    try:
        text = open(path, encoding="utf-8").read()
    except OSError:
        return None, None
    m = re.match(r"^---\n(.*?)\n---\n", text, re.S)
    if not m:
        return None, None
    fm = m.group(1)
    name = re.search(r"^name:\s*(.+)$", fm, re.M)
    desc = re.search(r"^description:\s*(.+)$", fm, re.M)
    return (name.group(1).strip() if name else None,
            desc.group(1).strip() if desc else None)


def main():
    root = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", ".."))
    skills_dir = os.path.join(root, "skills")
    registry = yaml.safe_load(open(os.path.join(root, "registry", "skills.yaml"), encoding="utf-8"))
    by_dir = {}
    for sk in registry["skills"]:
        rel = sk.get("path", "")
        parts = rel.split("/")
        if len(parts) >= 3:
            domain_dir = parts[1]
            by_dir.setdefault(domain_dir, []).append(sk)

    written = []
    for domain_dir in sorted(by_dir):
        if not os.path.isdir(os.path.join(skills_dir, domain_dir)):
            continue
        skills = sorted(by_dir[domain_dir], key=lambda s: s["id"])
        rows = []
        for sk in skills:
            skmd = os.path.join(skills_dir, domain_dir, os.path.basename(sk["path"]), "SKILL.md")
            name, desc = extract_frontmatter(skmd)
            if not desc:
                desc = "—"
            stability = sk.get("stability", "draft")
            rows.append(f"| `{sk['id']}` | {desc} | {stability} | `{sk['path']}` |")

        intro = DOMAIN_INTROS.get(domain_dir, "Low-level engineering skills for this domain.")
        lines = [
            f"# {domain_dir} — Skills",
            "",
            intro,
            "",
            "## Skills in this domain",
            "",
            "| Skill | What it does | Stability | Path |",
            "|---|---|---|---|",
            *rows,
            "",
            "## How to use",
            "",
            "- Read the `SKILL.md` of a skill for its triggers, reasoning rules, and verification commands.",
            "- `references/` hold the deep knowledge; `examples/good|bad` are verified compile-and-run fixtures;",
            "  `evals/` define how the skill is tested.",
            "- Load only the skill you need (see `skills/_meta/meta-routing`); references load on demand.",
            "",
            "## Related",
            "",
            "- [Back to repository root](../../README.md)",
            "",
        ]
        out = os.path.join(skills_dir, domain_dir, "README.md")
        open(out, "w", encoding="utf-8").write("\n".join(lines))
        written.append(domain_dir)

    # Area navigation block for the root README (printed for the coordinator to paste/verify)
    print("Domain READMEs written:", ", ".join(written))
    print("count:", len(written))
    print()
    for area, domains in AREAS:
        print(f"### {area}")
        for d in domains:
            print(f"- **{d}** — `skills/{d}/README.md`")
        print()


if __name__ == "__main__":
    main()
