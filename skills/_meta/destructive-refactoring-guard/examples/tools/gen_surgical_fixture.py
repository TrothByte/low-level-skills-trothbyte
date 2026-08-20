#!/usr/bin/env python3
"""Deterministic generator for examples/good/surgical_fix.diff.

Reads examples/good/handler_before.c and examples/good/handler_after.c and
writes the unified diff between them as examples/good/surgical_fix.diff
(path src/format_size.c). Re-running this script reproduces the diff
byte-identically on any host with python 3.11.
"""
import difflib
import os

HERE = os.path.dirname(os.path.abspath(__file__))
GOOD = os.path.normpath(os.path.join(HERE, "..", "good"))


def main():
    with open(os.path.join(GOOD, "handler_before.c"), "r", encoding="utf-8") as fh:
        before = fh.read()
    with open(os.path.join(GOOD, "handler_after.c"), "r", encoding="utf-8") as fh:
        after = fh.read()

    diff_lines = difflib.unified_diff(
        before.splitlines(), after.splitlines(),
        fromfile="src/format_size.c", tofile="src/format_size.c", lineterm="")
    diff_text = "\n".join(diff_lines) + "\n"

    out = os.path.join(GOOD, "surgical_fix.diff")
    with open(out, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(diff_text)

    print("surgical_fix.diff: %d lines" % len(diff_text.splitlines()))


if __name__ == "__main__":
    main()
