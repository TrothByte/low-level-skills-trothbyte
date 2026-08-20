#!/usr/bin/env python3
"""fp_check.py — static scan for floating-point anti-patterns in C source.

Heuristics (deterministic; conservative, no FP-value reasoning):
  FP_EQ         exact ==/!= against a floating literal (0.1, 1e-3, 0.5f)
  NAN_CMP       ==/!= against the NAN macro (always false; x != x is the fix)
  FAST_MATH     -ffast-math anywhere in the file (comments/build lines too)
  NOF_SUFFIX    unsuffixed float literal assigned to a float variable
  FLOAT_LOOP    float or double used as a loop counter

Not detected (documented limits): x87 excess precision, FMA contraction,
NaN propagation logic — none are statically visible; verify by building.

Lines containing the marker FP_CHECK_ANTIPATTERN_DEMO are skipped so a
self-demonstrating example (x == NAN shown to be false) can live in
examples/good without being flagged.

Usage: python fp_check.py [files or dirs ...]
Exit:  0 = no issues, 1 = issues found, 2 = usage error
"""
import os
import re
import sys

SKIP_MARKER = "FP_CHECK_ANTIPATTERN_DEMO"

FLOAT_NUM = (
    r"(?:0[xX](?:[0-9a-fA-F]+\.[0-9a-fA-F]*|\.[0-9a-fA-F]+)[pP][+-]?\d+|"
    r"(?:\d+\.\d*|\.\d+)(?:[eE][+-]?\d+)?|\d+[eE][+-]?\d+)"
)
FLOAT_LIT = FLOAT_NUM + r"f?"

EQ_FLOAT_L = re.compile(r"(?:==|!=)\s*(" + FLOAT_LIT + r")")
EQ_FLOAT_R = re.compile(r"(" + FLOAT_LIT + r")\s*(?:==|!=)")
NAN_CMP = re.compile(r"(?:==|!=)\s*NAN\b|\bNAN\s*(?:==|!=)")
FAST_MATH = re.compile(r"-ffast-math")
FLOAT_LOOP = re.compile(r"for\s*\(\s*(?:float|double)\s+\w+")

issues = []


def strip_code(lines):
    """Return comment/string-free text of each line (multi-line /* */ aware)."""
    out = []
    in_block = False
    for line in lines:
        line = re.sub(r"\"(?:[^\"\\]|\\.)*\"", "", line)
        line = re.sub(r"'(?:[^'\\]|\\.)*'", "", line)
        if not in_block:
            idx = line.find("//")
            if idx != -1:
                line = line[:idx]
        buf = []
        i = 0
        while True:
            if in_block:
                j = line.find("*/", i)
                if j == -1:
                    break
                in_block = False
                i = j + 2
            else:
                j = line.find("/*", i)
                if j == -1:
                    buf.append(line[i:])
                    break
                buf.append(line[i:j])
                in_block = True
                i = j + 2
        out.append("".join(buf))
    return out


def scan_file(path):
    try:
        with open(path, encoding="utf-8", errors="replace") as f:
            lines = f.read().splitlines()
    except OSError as exc:
        issues.append((path, 0, "IO", str(exc)))
        return

    code_lines = strip_code(lines)
    float_vars = set()

    for no, (raw, code) in enumerate(zip(lines, code_lines), 1):
        if SKIP_MARKER in raw:
            continue
        if FAST_MATH.search(raw):
            issues.append((path, no, "FAST_MATH", "-ffast-math present (assumes no NaN/Inf/errno)"))
        if not code.strip():
            continue
        if EQ_FLOAT_L.search(code) or EQ_FLOAT_R.search(code):
            issues.append((path, no, "FP_EQ", "exact ==/!= against a floating literal"))
        if NAN_CMP.search(code):
            issues.append((path, no, "NAN_CMP", "==/!= against NAN (always false; use x != x / isnan)"))
        if FLOAT_LOOP.search(code):
            issues.append((path, no, "FLOAT_LOOP", "float/double used as a loop counter"))

        m = re.findall(r"\bfloat\s+([A-Za-z_]\w*)\s*(?:=|;)", code)
        float_vars.update(m)

    for var in sorted(float_vars):
        assign = re.compile(r"\b" + var + r"\s*=\s*(" + FLOAT_LIT + r")")
        for no, (raw, code) in enumerate(zip(lines, code_lines), 1):
            if SKIP_MARKER in raw:
                continue
            m = assign.search(code)
            if m and not m.group(1).lower().endswith("f"):
                issues.append((path, no, "NOF_SUFFIX",
                               f"float '{var}' assigned unsuffixed literal '{m.group(1)}'"))


def main(argv):
    if not argv:
        print("usage: fp_check.py <file.c|dir> ...", file=sys.stderr)
        return 2

    files = []
    for p in argv:
        if os.path.isdir(p):
            for root, _dirs, names in os.walk(p):
                for n in sorted(names):
                    if n.endswith((".c", ".h")):
                        files.append(os.path.join(root, n))
        else:
            files.append(p)

    for f in sorted(set(files)):
        scan_file(f)

    issues.sort(key=lambda t: (t[1], t[0]))
    for path, line_no, code, msg in issues:
        print(f"{os.path.relpath(path)}:{line_no}: {code}: {msg}")
    print(f"{len(issues)} issue(s) across {len(set(files))} file(s)")
    return 1 if issues else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
