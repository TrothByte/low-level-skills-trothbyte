#!/usr/bin/env python3
"""skill_lint.py v2.0 — validate SKILL.md against strict authoring rules.

v2.0 improvements:
  - desc <= 50 words => ERROR (was WARNING)
  - body <= 250 lines   => ERROR (was WARNING at 300)
  - body >= 200 lines   => WARNING (approaching cap)
  - "Where the knowledge comes from" must contain >= 1 source
Exit codes: 0 = clean | 1 = warnings only | 2 = any errors

Usage: python tools/lint/skill_lint.py <SKILL.md> [...]
"""
import re
import sys


REQUIRED_SECTIONS = [
    "When to use",
    "When not to use",
    "What the agent often gets wrong",
    "How to reason correctly",
    "What to verify",
    "How to verify",
    "Where the knowledge comes from",
    "Related skills",
    "Evaluation",
]

MAX_LINES = 250       # v2.0 hard limit (was 300)
MAX_DESC_WORDS = 50   # v2.0 hard threshold
WARN_LINE_THRESHOLD = 200


def lint(path: str):
    """Return (warnings:list[str], errors:list[str])."""
    w: list[str] = []
    e: list[str] = []
    try:
        text = open(path, encoding="utf-8").read()
    except OSError as exc:
        return ([], [f"cannot read {path}: {exc}"])

    # --- frontmatter ---
    if not text.startswith("---"):
        return ([], ["frontmatter missing: file must start with '---'"])
    
    lines_arr = text.splitlines()
    fm_end = None
    for i, ln in enumerate(lines_arr[1:], start=1):
        if ln.strip() == "---":
            fm_end = i
            break

    if fm_end is None:
        return ([], ["frontmatter not closed (no second '---' marker)"])
    
    fm_text = "\n".join(lines_arr[1:fm_end])
    name_m = re.search(r"^name:\s*(.+)$", fm_text, re.M)
    desc_m = re.search(r"^description:\s*(.+)$", fm_text, re.M)
    
    if not name_m:
        e.append("frontmatter missing required field 'name'")
    
    if desc_m:
        wc = len(desc_m.group(1).strip().split())
        if wc > MAX_DESC_WORDS:
            e.append(f"description too long: {wc} words (max {MAX_DESC_WORDS})")
    else:
        e.append("frontmatter missing required field 'description'")
    
    if fm_end is None:
        return (w, e)

    # --- body size ---
    body = "\n".join(lines_arr[fm_end:])
    body_lines = len(body.splitlines())
    if body_lines > MAX_LINES:
        e.append(f"body too large: {body_lines} lines (max {MAX_LINES}; was {MAX_LINES+50} in v0.x)")
    elif body_lines > WARN_LINE_THRESHOLD:
        pct = round(body_lines / MAX_LINES * 100)
        w.append(f"body approaching limit: {body_lines}/{MAX_LINES} lines ({pct}%)")
    
    # --- required sections ---
    for sec in REQUIRED_SECTIONS:
        pattern = r"^#{1,6}\s+" + re.escape(sec) + r"\b"
        if not re.search(pattern, text, re.MULTILINE):
            w.append(f"missing required section: '{sec}'")
    
    # --- WTK provenance: must have source bullets ---
    wtk_match = re.search(
        r"#+\s*Where the knowledge comes from.*?(?=^##|\Z)",
        text, re.S | re.M
    )
    if wtk_match:
        bullets = re.findall(r"^[\s]*[-*]\s+", wtk_match.group(), re.M)
        if not bullets or len(bullets) < 1:
            e.append("'Where the knowledge comes from' has zero source references")
    else:
        e.append("section 'Where the knowledge comes from' not found")
    
    # --- absolute paths ---
    abspath_re = re.compile(r"(?:/Users/|C:\\|\\\\)")
    for m in abspath_re.finditer(text):
        lineno = text[:m.start()].count("\n") + 1
        w.append(f"absolute path on line {lineno}")
        break  # report first occurrence only
    
    return w, e


def main():
    paths = sys.argv[1:]
    if not paths:
        print("usage: skill_lint.py <SKILL.md> [...]")
        return 2
    
    rc = 0
    total_w = 0
    for p in paths:
        warnings, errors = lint(p)
        for err in errors:
            print(f"ERROR  {p}: {err}")
        for warn in warnings:
            print(f"WARN   {p}: {warn}")
        if errors:
            rc = 2
        elif warnings and rc == 0:
            rc = 1
        total_w += len(warnings)
        tag = f"{len(warnings)}W/{len(errors)}E" if warnings or errors else "OK"
        print(f"  {tag}")
    
    print(f"\nskill_lint summary: {len(paths)} files checked, {total_w} total warnings")
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
