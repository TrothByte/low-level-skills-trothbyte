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

    # tools.yaml: implemented tools must exist on disk (V-002 gate)
    tools_data = documents.get("tools.yaml", {})
    tools_impl = 0
    tools_total = 0
    for cat, items in tools_data.get("tools", {}).items():
        for t in items:
            tools_total += 1
            tpath = t.get("path", "")
            tstatus = t.get("status", "registered")
            if tstatus == "implemented" and tpath:
                tools_impl += 1
                p = os.path.join(os.path.dirname(base), tpath.replace("/", os.sep))
                if not os.path.isfile(p):
                    print(f"ERROR tools.yaml: tool {t.get('id')} marked implemented but missing at {tpath}")
                    rc = 2
            elif tstatus == "implemented":
                print(f"WARN tools.yaml: tool {t.get('id')} has no path but marked implemented")
                rc = max(rc, 1)
    print(f"  tools: {tools_impl}/{tools_total} implemented on disk")

    # Load cross-links for cycle detection
    cross_links_data = documents.get("cross-links.yaml", {})

    # === v2.0 CYCLE DETECTION on require edges (Level 2 quality gate) ===
    from collections import defaultdict
    graph: dict[str, list[str]] = defaultdict(list)
    for cl in cross_links_data.get("cross_links", []):
        rel_type = cl.get("rel", "")
        fr = cl.get("from", "")
        to = cl.get("to", "")
        if rel_type == "require" and fr and to:
            graph[fr].append(to)

    # DFS-based cycle detection
    WHITE, GRAY, BLACK = 0, 1, 2
    color: dict[str, int] = defaultdict(lambda: WHITE)
    parent: dict[str, str | None] = {}
    cycles_found = 0

    def dfs(node: str):
        nonlocal cycles_found
        color[node] = GRAY
        for nbr in graph.get(node, []):
            if color[nbr] == GRAY:
                # Found a back edge => cycle
                cycle_path = [nbr]
                cur = node
                parent_map = {v: k for k, vals in parent.items() for v in vals}
                while cur != nbr and cur is not None:
                    cycle_path.append(cur)
                    cur = parent.get(cur)
                cycle_path.append(nbr)
                cycle_str = " => ".join(reversed(cycle_path))
                print(f"ERROR cycle detected: {cycle_str}")
                cycles_found += 1
                rc = 2
            elif color[nbr] == WHITE:
                parent[nbr] = node
                dfs(nbr)
        color[node] = BLACK

    all_nodes = set(graph.keys())
    for targets in graph.values():
        all_nodes.update(targets)
    
    # Use iterative approach to avoid recursion limit
    stack_visited = []
    def dfs_iter(start):
        visited_local = set()
        stk = [(start, iter(graph.get(start, [])))]
        path = [start]
        while stk:
            node, neighbors = stk[-1]
            try:
                nxt = next(neighbors)
                if color[nxt] == GRAY:
                    idx = len(path) - 1
                    while idx >= 0 and path[idx] != nxt:
                        idx -= 1
                    if idx >= 0:
                        cycle = path[idx:] + [nxt]
                        print(f"ERROR cycle detected: {' => '.join(cycle)}")
                        cycles_found += 1
    
                    rc |= 2
                elif color[nxt] == WHITE:
                    parent[nxt] = node
                    stk.append((nxt, iter(graph.get(nxt, []))))
                    path.append(nxt)
            except StopIteration:
                stk.pop()
                if path: path.pop()
                color[node] = BLACK

    for node in sorted(all_nodes):
        if color[node] == WHITE:
            dfs_iter(node)

    if cycles_found == 0:
        pass  # no cycles — clean graph
    else:
        print(f"  ^^^ {cycles_found} cycle(s) found — fix cross-links.yaml")

    print(f"\nregistry_check: {len(documents)} files parsed\n"
          f"  sources: {len(sources)}, skills: {len(skills)}, claims: {len(claims.get('claims', []))},\n"
          f"  cross-links: {len(cross_links_data.get('cross_links', []))}\n"
          f"  requires: {sum(len(v) for v in graph.values())}\n"
          f"  cycles found: {cycles_found}")
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
