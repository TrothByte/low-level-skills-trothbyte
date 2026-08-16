#!/usr/bin/env python3
"""token_measure.py — estimate token budget of a skill's files (v2.0).

Measurement:
  - Uses tiktoken (cl100k_base) when available for ±1-token accuracy
  - Falls back to a chars/3.5 + words/1.3 blended heuristic when tiktoken
    is not installed (no hard dependency)

Reports:
  metadata tokens (name+description frontmatter)
  SKILL.md body tokens (the "activation cost")
  per-reference tokens
  per-example tokens
  totals

Hard-limit mode:
  python token_measure.py --check 2000 <skill-dir> [...]
  -> exit 1 if ANY skill's SKILL.md activation cost (metadata+body) exceeds 2000
     (v2.0 token gate; used by CI and validate.py)

Usage: python token_measure.py <skill-dir> [skill-dir ...]
       python token_measure.py --check <limit> <skill-dir> [...]
"""
import os
import re
import sys

try:
    import tiktoken
    _ENCODER = tiktoken.get_encoding("cl100k_base")
except Exception:  # pragma: no cover - tiktoken optional
    _ENCODER = None


def est_tokens(text: str) -> int:
    """Best-effort token estimate for English technical text."""
    if _ENCODER is not None:
        return max(1, len(_ENCODER.encode(text)))
    chars = len(text)
    words = len(text.split())
    # blended heuristic: ~3.5 chars/token, ~1.3 words/token
    return max(1, round(chars / 3.5 * 0.6 + words / 1.3 * 0.4))


def measure(path: str):
    text = open(path, encoding="utf-8").read()
    meta = 0
    total = est_tokens(text)
    if text.startswith("---"):
        m = re.match(r"^---\n(.*?)\n---\n", text, re.S)
        if m:
            meta = est_tokens(m.group(1))
            total = est_tokens(text[m.end():])
    return meta, total


def main() -> int:
    args = sys.argv[1:]
    if not args:
        print("usage: token_measure.py [--check <limit>] <skill-dir> [...]")
        return 2

    check_limit = None
    if args[0] == "--check":
        if len(args) < 3:
            print("error: --check requires a token limit argument")
            return 2
        check_limit = int(args[1])
        dirs = args[2:]
    else:
        dirs = args

    backend = "tiktoken cl100k_base" if _ENCODER else "heuristic (chars/3.5 + words/1.3)"
    print(f"[token_measure] backend: {backend}")

    max_activation = 0
    worst_skill = None
    any_violation = False

    for d in dirs:
        print(f"== {d}")
        activation = 0
        total = 0
        sk = os.path.join(d, "SKILL.md")
        if os.path.isfile(sk):
            meta, body = measure(sk)
            activation = meta + body
            total += activation
            flag = ""
            if check_limit is not None and activation > check_limit:
                flag = f"  <-- EXCEEDS {check_limit}-token gate"
                any_violation = True
            print(f"  SKILL.md: metadata {meta}, body {body} "
                  f"(activation cost {activation}){flag}")
            if activation > max_activation:
                max_activation = activation
                worst_skill = d
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

    if check_limit is not None:
        print()
        if any_violation:
            print(f"[token_measure] FAIL: activation cost exceeded {check_limit} tokens "
                  f"(worst: {worst_skill} at {max_activation})")
            return 1
        print(f"[token_measure] OK: all skills within {check_limit}-token activation gate "
              f"(worst: {worst_skill} at {max_activation})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
