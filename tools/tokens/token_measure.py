#!/usr/bin/env python3
"""token_measure.py — estimate token budget of a skill's files.

Rough heuristic: ~4 chars per token (English text). Reports:
  metadata tokens (name+description frontmatter)
  SKILL.md body tokens
  per-reference tokens
  per-example tokens
  totals
Usage: python token_measure.py <skill-dir> [skill-dir ...]
"""
import os
import re
import sys


def est_tokens(text: str) -> int:
    return max(1, len(text) // 4)


def measure(path: str):
    text = open(path, encoding="utf-8").read()
    total = est_tokens(text)
    meta = 0
    if text.startswith("---"):
        m = re.match(r"^---\n(.*?)\n---\n", text, re.S)
        if m:
            meta = est_tokens(m.group(1))
            total = est_tokens(text[m.end():])
    return meta, total


def main() -> int:
    dirs = sys.argv[1:]
    if not dirs:
        print("usage: token_measure.py <skill-dir> [...]")
        return 2
    for d in dirs:
        print(f"== {d}")
        total = 0
        sk = os.path.join(d, "SKILL.md")
        if os.path.isfile(sk):
            meta, body = measure(sk)
            total += meta + body
            print(f"  SKILL.md: metadata {meta}, body {body} (activation cost {meta + body})")
        refs = os.path.join(d, "references")
        if os.path.isdir(refs):
            for f in sorted(os.listdir(refs)):
                p = os.path.join(refs, f)
                t = est_tokens(open(p, encoding="utf-8").read())
                total += t
                print(f"  references/{f}: {t}")
        for sub in ("examples", "evals"):
            s = os.path.join(d, sub)
            if os.path.isdir(s):
                for root, _, files in os.walk(s):
                    for f in sorted(files):
                        if not f.endswith((".c", ".h", ".rs", ".md")):
                            continue
                        p = os.path.join(root, f)
                        t = est_tokens(open(p, encoding="utf-8").read())
                        total += t
                        print(f"  {os.path.relpath(p, d)}: {t}")
        print(f"  TOTAL estimated tokens: {total}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
