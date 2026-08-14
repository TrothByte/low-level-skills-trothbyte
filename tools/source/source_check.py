#!/usr/bin/env python3
"""source_check.py — verify that skills reference registered sources.

Scans each skill's SKILL.md and references/*.md for `- **SOURCE**` lines
(in the reference RULE format) and checks the cited source ids exist in
registry/sources.yaml. Also verifies every claim in registry/claims.yaml
has a non-empty source and section.

Usage: python source_check.py [repo-root]
Exit: 0 clean, 1 warnings, 2 errors.
"""
import glob
import os
import re
import sys

import yaml


def main() -> int:
    root = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", ".."))
    rc = 0
    sources = {}
    sp = os.path.join(root, "registry", "sources.yaml")
    with open(sp, encoding="utf-8") as f:
        for s in yaml.safe_load(f)["sources"]:
            sources[s["id"]] = s

    # claims check
    cp = os.path.join(root, "registry", "claims.yaml")
    with open(cp, encoding="utf-8") as f:
        claims = yaml.safe_load(f)["claims"]
    for c in claims:
        if not c.get("source") or not c.get("section"):
            print(f"ERROR claims.yaml: claim {c.get('id')} missing source or section")
            rc = 2
        if c.get("source") and c["source"] not in sources:
            print(f"ERROR claims.yaml: claim {c.get('id')} references unknown source '{c.get('source')}'")
            rc = 2

    # reference SOURCE lines
    allowed_free = re.compile(r"\bN1570\b|\bN4861\b|\bN4971\b|\bCVE-\d{4}-\d{4,}\b|\bSDM\b|\bAPM\b|\bempirical\b|\bLDD3\b")
    aliases = {  # short citation forms -> registered source ids
        "cg r": "cpp-core-guidelines",
        "core guidelines": "cpp-core-guidelines",
        "c++ [": "iso-cpp20-n4861",  # standard section citations
        "class.copy": "iso-cpp20-n4861",
        "stroustrup": "herbsutter-gotw",
    }
    norm = lambda s: re.sub(r"[^a-z0-9]", "", s.lower())
    source_tokens = []
    for sid in sources:
        tokens = set(norm(t) for t in re.split(r"[^a-z0-9]+", sid) if len(norm(t)) >= 3)
        tokens.add(norm(sid))  # full normalized id (handles short ids like ptx-isa)
        source_tokens.append(tokens)
    for md in glob.glob(os.path.join(root, "skills", "**", "*.md"), recursive=True):
        text = open(md, encoding="utf-8").read()
        for m in re.finditer(r"^- \*\*SOURCE\*\*: (.+)$", text, re.M):
            cited = m.group(1)
            nc = norm(cited)
            matched = any(
                any(tok in nc for tok in toks) for toks in source_tokens if toks
            )
            if not matched:
                ncc = norm(cited)
                for alias, target in aliases.items():
                    if norm(alias) in ncc and target in sources:
                        matched = True
                        break
            if not matched and not allowed_free.search(cited):
                print(f"WARN {os.path.relpath(md, root)}: SOURCE line not matched to registry: {cited}")
                rc = max(rc, 1)
    print(f"source_check: {len(sources)} sources, {len(claims)} claims checked")
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
