#!/usr/bin/env python3
"""registry_check.py — validate registry/*.yaml integrity.

Checks:
  - every YAML file in registry/ parses
  - no duplicate ids within each file
  - every source id referenced in claims.yaml exists in sources.yaml
  - every skill id in cross-links.yaml (from/to) exists in skills.yaml
  - registry paths in skills.yaml point at existing SKILL.md
Usage: python registry_check.py [registry-dir]
Exit: 0 clean, 1 warnings, 2 errors.
"""
import glob
import os
import re
import sys

import yaml


def load(path: str):
    with open(path, encoding="utf-8") as f:
        return yaml.safe_load(f)


def collect_ids(data, keys=("id",)):
    ids = []
    def walk(node):
        if isinstance(node, dict):
            if any(k in node for k in keys) and "id" in node:
                ids.append(node["id"])
            for v in node.values():
                walk(v)
        elif isinstance(node, list):
            for v in node:
                walk(v)
    walk(data)
    return ids


def main() -> int:
    base = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", "..", "registry")
    base = os.path.abspath(base)
    rc = 0
    documents = {}
    for yf in sorted(glob.glob(os.path.join(base, "*.yaml"))):
        try:
            documents[os.path.basename(yf)] = load(yf)
        except Exception as e:
            print(f"ERROR {os.path.basename(yf)}: cannot parse YAML: {e}")
            rc = 2

    # duplicate ids per file
    for name, doc in documents.items():
        ids = collect_ids(doc)
        seen, dup = set(), set()
        for i in ids:
            if i in seen:
                dup.add(i)
            seen.add(i)
        for d in sorted(dup):
            print(f"ERROR {name}: duplicate id '{d}'")
            rc = 2

    sources = set(collect_ids(documents.get("sources.yaml", {})))
    skills = set(collect_ids(documents.get("skills.yaml", {})))
    claims = documents.get("claims.yaml", {})

    # claims -> sources
    for claim in claims.get("claims", []):
        if claim.get("source") not in sources:
            print(f"ERROR claims.yaml: claim {claim.get('id')} references unknown source '{claim.get('source')}'")
            rc = 2

    # cross-links -> skills
    for cl in documents.get("cross-links.yaml", {}).get("cross_links", []):
        if cl.get("from") not in skills:
            print(f"ERROR cross-links.yaml: 'from' skill '{cl.get('from')}' not in skills.yaml")
            rc = 2
        if cl.get("to") not in skills:
            print(f"ERROR cross-links.yaml: 'to' skill '{cl.get('to')}' not in skills.yaml")
            rc = 2

    # skills.yaml paths -> SKILL.md presence
    for sk in documents.get("skills.yaml", {}).get("skills", []):
        rel = sk.get("path")
        if not rel:
            continue
        p = os.path.join(os.path.dirname(base), rel.replace("/", os.sep), "SKILL.md")
        if sk.get("status") == "implemented" and not os.path.isfile(p):
            print(f"WARN skills.yaml: implemented skill {sk.get('id')} missing SKILL.md at {p}")
            rc = max(rc, 1)

    print(f"registry_check: {len(documents)} files parsed, ids: {len(sources)} sources, {len(skills)} skills, {len(claims.get('claims', []))} claims")
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
