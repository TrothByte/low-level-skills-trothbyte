#!/usr/bin/env python3
"""DTCG token-set validator for design-token-system-discipline.

Checks DTCG format basics: every non-group token carries $type and $value;
$type is known; color values are #hex; dimension values carry a unit;
aliases ({a.b.c}) resolve to an existing token path; tokens in a group
named `semantic` reference aliases instead of raw literals (semantic layer
must not hard-code values); composite tokens (typography) resolve aliases
inside their $value object. Exit 0 = clean, 1 = issues found.
"""
import json
import re
import sys

KNOWN_TYPES = {
    "color", "dimension", "fontFamily", "number", "fontWeight",
    "fontSize", "lineHeight", "letterSpacing", "duration",
    "typography", "border", "shadow", "gradient", "strokeStyle",
}

ALIAS_RE = re.compile(r"^\{([a-zA-Z0-9_.-]+)\}$")
HEX_RE = re.compile(r"^#([0-9a-fA-F]{3}|[0-9a-fA-F]{4}|[0-9a-fA-F]{6}|[0-9a-fA-F]{8})$")
DIM_RE = re.compile(r"^-?\d+(\.\d+)?(px|rem|em|%)$")


def walk(tree, path):
    for key, node in tree.items():
        cur = path + [key]
        if isinstance(node, dict) and "$value" not in node:
            yield from walk(node, cur)
        else:
            yield cur, node


def main():
    if len(sys.argv) != 2:
        print("usage: verify_dtcg.py <tokens.json>")
        return 2
    path = sys.argv[1]
    data = json.load(open(path, encoding="utf-8"))
    nodes = list(walk(data, []))
    paths = {tuple(p): n for p, n in nodes}
    issues = []

    for p, node in nodes:
        name = ".".join(p)
        if "$type" not in node:
            issues.append(f"{name}: missing $type")
            continue
        t = node["$type"]
        if t not in KNOWN_TYPES:
            issues.append(f"{name}: unknown $type {t!r}")
        if "$value" not in node:
            issues.append(f"{name}: missing $value")
            continue
        value = node["$value"]
        is_semantic = "semantic" in p
        if t == "typography":
            if not isinstance(value, dict):
                issues.append(f"{name}: typography $value must be an object")
                continue
            for k, v in value.items():
                if isinstance(v, str) and ALIAS_RE.match(v):
                    target = tuple(ALIAS_RE.match(v).group(1).split("."))
                    if target not in paths:
                        issues.append(f"{name}.{k}: alias target does not exist: {v}")
        elif isinstance(value, str):
            m = ALIAS_RE.match(value)
            if m:
                target = tuple(m.group(1).split("."))
                if target not in paths:
                    issues.append(f"{name}: alias target does not exist: {value}")
            elif is_semantic:
                issues.append(f"{name}: semantic token must alias a primitive, got raw value {value!r}")
            elif t == "color" and not HEX_RE.match(value):
                issues.append(f"{name}: color value not a #hex literal: {value!r}")
            elif t == "dimension" and not DIM_RE.match(value):
                issues.append(f"{name}: dimension value not a unit length: {value!r}")

    if issues:
        for i in issues:
            print(f"ISSUE {path}: {i}")
        print(f"{path}: {len(issues)} issue(s) found")
        return 1
    print(f"{path}: DTCG structure OK ({len(nodes)} tokens, all aliases resolve)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
