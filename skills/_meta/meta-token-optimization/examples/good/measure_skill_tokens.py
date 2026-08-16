# GOOD: measure_skill_tokens.py
"""
Measures a skill's activation cost using the
same blended heuristic as tools/tokens/token_measure.py, then applies the
2000-token gate. This is the deterministic, executable core of the
meta-token-optimization skill: measure BEFORE and AFTER every edit.

Usage: python examples/good/measure_skill_tokens.py <skill-dir>
Exit:   0 if activation cost <= 2000, 1 otherwise.
"""

import os
import re
import sys


def est_tokens(text):
    chars = len(text)
    words = len(text.split())
    # blended heuristic from token_measure.py (no tiktoken dependency):
    return max(1, round(chars / 3.5 * 0.6 + words / 1.3 * 0.4))


def activation_cost(skill_dir):
    path = os.path.join(skill_dir, "SKILL.md")
    text = open(path, encoding="utf-8").read()
    meta = 0
    total = est_tokens(text)
    m = re.match(r"^---\n(.*?)\n---\n", text, re.S)
    if m:
        meta = est_tokens(m.group(1))
        total = est_tokens(text[m.end():])
    return meta, total, len(text.splitlines())


def main():
    if len(sys.argv) != 2:
        print("usage: measure_skill_tokens.py <skill-dir>")
        return 2
    skill_dir = sys.argv[1]
    meta, body, lines = activation_cost(skill_dir)
    activation = meta + body
    flag = "  <-- EXCEEDS 2000-token gate" if activation > 2000 else ""
    print(f"SKILL.md: metadata {meta}, body {body} (activation cost {activation}){flag}")
    print(f"body lines: {lines} (limit 250)")
    return 1 if activation > 2000 else 0


if __name__ == "__main__":
    raise SystemExit(main())
