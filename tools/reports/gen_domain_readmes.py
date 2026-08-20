#!/usr/bin/env python3
"""gen_domain_readmes.py — generate skills/<domain>/README.md for every domain.

Reads registry/skills.yaml + frontmatter from each SKILL.md + intros from
roadmap/coverage.yaml DOMAIN_INTROS or default text. Writes one README.md per
domain under `skills/<domain>/`.

Usage: python tools/reports/gen_domain_readmes.py [repo-root]
"""
import os
import re
import sys

import yaml


def load_yaml(path):
    with open(path, encoding="utf-8") as f:
        return yaml.safe_load(f)


FRONTMATTER_RE = re.compile(r"^---\n(.*?)\n---", re.S)
NAME_RE = re.compile(r"^name:\s*(.+)$", re.M)
DESC_RE = re.compile(r"^description:\s*(.+)$", re.M)


def read_frontmatter(path):
    try:
        text = open(path, encoding="utf-8").read()
    except OSError:
        return None, None
    m = FRONTMATTER_RE.match(text)
    if not m:
        return None, None
    name_m = NAME_RE.search(m.group(1), re.M)
    desc_m = DESC_RE.search(m.group(1), re.M)
    n = name_m.group(1).strip() if name_m else ""
    d = desc_m.group(1).strip() if desc_m else ""
    return n, d


DOMAIN_INTROS = {
    "abi": "The ABI is the contract between compilers, libraries, and languages.",
    "assembly": "Assembly is the ground truth every higher language compiles to.",
    "binary-analysis": "Reading binaries means recovering what the compiler encoded.",
    "bootloader": "Bootloaders own the first instructions of a machine.",
    "build-systems": "Build systems turn source code into executable binaries.",
    "c": "C is the lingua franca of low-level engineering.",
    "compiler": "The compiler is the agent's least-trusted and least-understood colleague.",
    "concurrency": "Concurrency bugs compile.",
    "cpp": "C++ adds object lifetimes and RAII on top of C's sharp edges.",
    "debugging": "Debugging is instrumentation over reasoning — measure before you conclude.",
    "dwarf": "DWARF is the universal debug information format.",
    "elf": "ELF is the binary format of Linux and friends.",
    "embedded": "Embedded development runs on bare metal with MMIO, interrupts, and linker scripts.",
    "ffi": "FFI is where one language's safety guarantees end.",
    "gpu": "GPUs have a memory model of their own.",
    "hdl": "HDL/FPGA design targets silicon behavior, not just simulation.",
    "hpc": "High-performance computing pushes hardware limits with parallel compute.",
    "kernel": "The Linux kernel is the largest low-level system in the world.",
    "llvm": "LLVM is the reference compiler infrastructure.",
    "mobile": "Android reverse engineering requires understanding two runtime layers.",
    "networking": "High-performance networking is kernel-adjacent.",
    "performance": "Performance work starts with measurement, never assumptions.",
    "qemu": "QEMU is the universal verification host for code you cannot yet run on real hardware.",
    "reverse-engineering": "Reverse engineering covers Go/Rust binaries with special metadata and automated protocol recovery.",
    "riscv": "RISC-V is the ascending open-source ISA for edge through HPC.",
    "rust": "Rust makes memory safety the default — unsafe blocks move the burden onto the author.",
    "sanitizers": "Sanitizers turn 'probably fine' into evidence.",
    "security": "Security at this level is about constant-time, formal specs, and SMT solvers.",
    "simd": "SIMD is where CPUs get fast — auto-vectorization has block detectors worth knowing.",
    "virtualization": "Virtualization hides hardware behind hypercalls and state transitions.",
    "zig": "Zig gives explicit control over build, allocator, comptime, and FFI semantics.",
    "accelerator": "AI accelerator pipeline programs: cross-unit (DMA/vector/matrix/scalar) synchronization and barrier coverage on shared on-chip buffers.",
    "design": "Designer-mode skills for AI agents: design tokens (DTCG), typography hierarchy, WCAG 2.2 color/contrast accessibility, layout grids and reflow, visual hierarchy, and anti-AI-look originality review.",
    "safety": "Functional-safety and deterministic-systems skills: MISRA C/C++ compliance and hard real-time determinism for automotive, aerospace, and medical code.",
    "_meta": "Meta-skills govern agent behavior: routing, evidence discipline, verification gates,\nassumption surfacing, rationalization rejection, harness validity, crypto safety, and honest completion.",
}


def gen_readme(domain_name, skills_list, intro_override=None):
    lines = []
    lines.append(f"# {domain_name} — Skills")
    lines.append("")
    base_intro = DOMAIN_INTROS.get(domain_name, "")
    if intro_override:
        lines.append(intro_override.strip())
    elif base_intro:
        # Use first sentence only if there's more content in the key
        sentence = base_intro.split(".")[0] + "."
        lines.append(sentence)
    else:
        lines.append(f"Skills covering '{domain_name}' topics.")
    lines.append("")
    if not skills_list:
        lines.append("_No registered skills yet._")
        lines.append("")
        return "\n".join(lines)

    lines.append("| Skill | What it does | Type | Stability | Path |")
    lines.append("|---|---|---|---|---|")
    for sk in sorted(skills_list, key=lambda s: s["id"]):
        skmd = os.path.join(sk["path"], "SKILL.md")
        _, desc = read_frontmatter(skmd)
        if not desc:
            desc = sk.get("id", "")
        t = sk.get("type", "")
        st = sk.get("stability", "")
        path_f = sk.get("path", "")
        lines.append(f"| `{sk['id']}` | {desc} | {t} | {st} | `{path_f}` |")

    lines.append("")
    lines.append("## How to use")
    lines.append("")
    lines.append("- Read the `SKILL.md` of a skill for its triggers, reasoning rules, and verification commands.")
    lines.append("  (`references/` hold deep knowledge; `examples/good` and `examples/bad` are compiled/run")
    lines.append("  fixtures; `evals/README.md` defines eval cases.)")
    lines.append("- Load only the skill you need (see `skills/_meta/meta-routing`; references load on demand.")
    lines.append("")
    lines.append("## Related")
    lines.append("")
    lines.append(
        "[Back to repository root](../../README.md)"
    )
    lines.append("")  # final newline

    return "\n".join(lines)


def main():
    root = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", ".."))
    coverage = load_yaml(os.path.join(root, "roadmap", "coverage.yaml"))
    skills_yml = load_yaml(os.path.join(root, "registry", "skills.yaml"))

    by_domain = {}
    for sk in skills_yml.get("skills", []):
        parts = sk["path"].split("/")
        dom = parts[1] if len(parts) >= 2 else "?"
        by_domain.setdefault(dom, []).append(sk)

    domain_intros_map = {}
    for d_info in coverage.get("domains", []):
        d_name = d_info.get("name", "").lower()
        if d_name and d_info.get("existing_coverage"):
            desc = d_info.get("gap", "")
            if desc and len(desc) > 20:
                domain_intros_map[d_name] = desc.split(".")[0].capitalize() + "."

    wrote_count = 0
    for domain, sks in by_domain.items():
        readme_path = os.path.join(root, "skills", domain, "README.md")
        # Curated intros take priority; coverage gap text is a fallback only.
        intro = DOMAIN_INTROS.get(domain) or domain_intros_map.get(domain)
        readme_text = gen_readme(domain, sks, intro_override=intro)
        existing = ""
        if os.path.exists(readme_path):
            existing = open(readme_path, encoding="utf-8").read()
        if existing != readme_text:
            with open(readme_path, "w", encoding="utf-8") as f:
                f.write(readme_text)
            wrote_count += 1
        else:
            print(f"SKIP {domain}/README.md (unchanged)")

    print(f"Wrote (or skipped): {len(by_domain)} domains, {wrote_count} changed")


if __name__ == "__main__":
    raise SystemExit(main())