import os
import json

root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
skills_root = os.path.join(root, "skills")

paths = []
for domain in sorted(os.listdir(skills_root)):
    dpath = os.path.join(skills_root, domain)
    if not os.path.isdir(dpath):
        continue
    for sid in sorted(os.listdir(dpath)):
        skill_dir = os.path.join(dpath, sid)
        if os.path.isdir(skill_dir) and os.path.exists(os.path.join(skill_dir, "SKILL.md")):
            paths.append(f"./skills/{domain}/{sid}")

marketplace = {
    "name": "trothbyte-low-level-skills",
    "description": "124 verified low-level engineering skills for coding agents",
    "version": "0.2.0",
    "owner": {
        "name": "TrothByte",
        "url": "https://github.com/TrothByte"
    },
    "plugins": [
        {
            "name": "low-level-skills",
            "displayName": "Low-Level Skills",
            "source": "./",
            "description": "124 verified low-level engineering skills for coding agents (C/C++/Rust/asm/kernel/embedded/Zig/GPU/RE/build systems). Every claim is source-traced; 65 skills are executed on real toolchains.",
            "homepage": "https://github.com/TrothByte/low-level-skills-trothbyte",
            "repository": "https://github.com/TrothByte/low-level-skills-trothbyte",
            "license": "MIT",
            "keywords": ["low-level", "c", "cpp", "rust", "assembly", "kernel", "embedded", "zig", "gpu", "reverse-engineering", "verification"],
            "skills": paths
        }
    ]
}

out = os.path.join(root, ".claude-plugin", "marketplace.json")
os.makedirs(os.path.dirname(out), exist_ok=True)
with open(out, "w", encoding="utf-8") as f:
    json.dump(marketplace, f, indent=2, ensure_ascii=False)
    f.write("\n")

print(f"wrote {out}: {len(paths)} skills listed")
