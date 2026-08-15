#!/usr/bin/env python3
"""check_dts.py - host-side stand-in for the dtc/west structural checks this
skill documents. It is NOT dtc: it implements a small subset of the
devicetree rules on the example overlays:
  1. unit-addresses must be unique among sibling nodes
  2. every node with a unit-address must have a reg property
  3. compatible strings must be present in the known registry

The real verification is `dtc -I dts -O dtb` and `west build` (see SKILL.md).
Usage: python check_dts.py <file.dts>...
"""

import re
import sys

KNOWN_COMPATIBLES = {
    "sitronix,st7789v",
    "jedec,spi-nor",
}

NODE_RE = re.compile(r"^([\w@,-]+):\s*([\w-]+)(?:@([\w]+))?\s*{$")
REF_RE = re.compile(r"^&([\w-]+)\s*{$")
COMPAT_RE = re.compile(r'compatible\s*=\s*"([^"]+)"')
REG_RE = re.compile(r"\breg\s*=")

def check(path):
    problems = []
    stack = []
    try:
        lines = open(path, encoding="utf-8").read().splitlines()
    except OSError as e:
        return [f"{path}: cannot read: {e}"]

    for lineno, line in enumerate(lines, 1):
        text = line.strip()
        if not text or text.startswith("//") or text.startswith("#") or text.startswith("/*"):
            continue
        m = NODE_RE.match(text)
        if m:
            label, name, unit = m.groups()
            node = {"name": name, "unit": unit, "has_reg": False, "children": {},
                    "compat": [], "line": lineno, "path": "/".join(x["name"] for x in stack)}
            stack.append(node)
            continue
        m = REF_RE.match(text)
        if m:
            node = {"name": "&" + m.group(1), "unit": None, "has_reg": True,
                    "children": {}, "compat": [], "line": lineno,
                    "path": "/".join(x["name"] for x in stack)}
            stack.append(node)
            continue
        if text in ("}", "};"):
            if not stack:
                continue
            node = stack.pop()
            if node["unit"] is not None and not node["has_reg"]:
                problems.append(f"{path}:{node['line']}: node '{node['name']}@{node['unit']}' "
                                f"has a unit-address but no 'reg' property")
            for compat in node["compat"]:
                if compat not in KNOWN_COMPATIBLES:
                    problems.append(f"{path}:{node['line']}: unknown compatible "
                                    f"'{compat}' (not in binding registry)")
            if node["unit"] is not None and stack:
                sibling_map = stack[-1]["children"]
                if node["unit"] in sibling_map:
                    problems.append(f"{path}:{node['line']}: duplicate unit-address "
                                    f"'{node['unit']}' among siblings of "
                                    f"'{stack[-1]['name']}'")
                sibling_map[node["unit"]] = True
            continue
        if stack:
            cm = COMPAT_RE.search(text)
            if cm:
                stack[-1]["compat"].append(cm.group(1))
            if REG_RE.search(text):
                stack[-1]["has_reg"] = True
    return problems

def main():
    rc = 0
    for path in sys.argv[1:]:
        problems = check(path)
        if problems:
            rc = 1
            for p in problems:
                print("FAIL " + p)
        else:
            print(f"PASS {path}")
    return rc

if __name__ == "__main__":
    raise SystemExit(main())
