#!/usr/bin/env python3
"""refactor_diff_guard.py — flag destructive refactors before they land.

Reads a unified diff (git-style or difflib-style) or two file snapshots and
reports per-file added/deleted/net LOC. A file is FLAGGED when:

  - deletions exceed additions beyond --threshold lines (default 50), i.e.
    the change is deletion-dominant with a large net negative delta, or
  - more than --max-loss percent of the original file was deleted
    (default 50).

Usage:
  python refactor_diff_guard.py <file.diff>            # unified diff
  python refactor_diff_guard.py --before a.c --after b.c
  python refactor_diff_guard.py --before a.c --after b.c --threshold 20
  python refactor_diff_guard.py <file.diff> --max-loss 60

Exit status: 0 = nothing flagged, 1 = one or more files flagged.
LOC accounting mirrors `git diff --numstat` (added/deleted per file).

This is a guard, not a judge: it measures the diff and asks the agent to
justify flagged files. A justified large deletion (failing test, recorded
baseline, zero remaining references) can still pass review.
"""
import argparse
import difflib
import re
import sys

HUNK_RE = re.compile(r"^@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@")


def _norm_path(p):
    p = p.strip()
    for prefix in ("a/", "b/"):
        if p.startswith(prefix):
            p = p[len(prefix):]
    return p


def parse_unified(text):
    """Return {path: {'added', 'deleted', 'old_size'}} from a unified diff.

    Handles both git-style output (diff --git headers) and difflib-style
    output (--- / ++++ pairs). old_size is derived from the @@ hunk ranges
    (max old line reached); for snapshot mode it is the exact file size.
    """
    lines = text.splitlines()
    files = {}
    cur = None
    i = 0
    n = len(lines)
    while i < n:
        line = lines[i]
        if line.startswith("diff --git "):
            rest = line[len("diff --git "):]
            name = rest.split(" b/", 1)[-1] if " b/" in rest else rest
            cur = {"added": 0, "deleted": 0, "old_size": 0, "max_old_end": 0}
            files.setdefault(_norm_path(name), cur)
            i += 1
            continue
        if line.startswith("--- ") and i + 1 < n and lines[i + 1].startswith("+++ "):
            newp = _norm_path(lines[i + 1][4:])
            cur = {"added": 0, "deleted": 0, "old_size": 0, "max_old_end": 0}
            files.setdefault(newp, cur)
            i += 2
            continue
        if line.startswith("@@"):
            m = HUNK_RE.match(line)
            if m and cur is not None:
                old_start = int(m.group(1))
                old_cnt = int(m.group(2) or "1")
                cur["max_old_end"] = max(cur["max_old_end"], old_start + old_cnt - 1)
            i += 1
            continue
        if cur is not None:
            if line.startswith("+") and not line.startswith("+++"):
                cur["added"] += 1
            elif line.startswith("-") and not line.startswith("---"):
                cur["deleted"] += 1
        i += 1
    for f in files.values():
        f["old_size"] = f["max_old_end"]
    return files


def from_snapshots(before_path, after_path):
    with open(before_path, "r", encoding="utf-8") as fh:
        old = fh.readlines()
    with open(after_path, "r", encoding="utf-8") as fh:
        new = fh.readlines()
    sm = difflib.SequenceMatcher(a=old, b=new, autojunk=False)
    added = deleted = 0
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag in ("delete", "replace"):
            deleted += i2 - i1
        if tag in ("insert", "replace"):
            added += j2 - j1
    return {
        after_path: {
            "added": added,
            "deleted": deleted,
            "old_size": len(old),
            "max_old_end": len(old),
        }
    }


def report(files, threshold, max_loss):
    flagged = 0
    print(f"{'file':<24} {'+added':>6} {'-deleted':>8} {'net':>7} "
          f"{'lost%':>7}  status")
    print("-" * 64)
    for name, f in files.items():
        added = f["added"]
        deleted = f["deleted"]
        old = f["old_size"]
        net = added - deleted
        loss = (deleted / old * 100.0) if old else 0.0
        flags = []
        if deleted > added and (deleted - added) >= threshold:
            flags.append(f"deletion-dominant (net -{deleted - added} >= {threshold})")
        if old and loss > max_loss:
            flags.append(f">{max_loss}% of lines lost ({loss:.1f}%)")
        status = "FLAG: " + "; ".join(flags) if flags else "ok"
        if flags:
            flagged += 1
        print(f"{name:<24} {added:>6} {deleted:>8} {net:>7} {loss:>6.1f}%  {status}")
    print("-" * 64)
    if flagged:
        print(f"RESULT: {flagged} file(s) flagged — justify each deletion or "
              "split into reversible commits (baseline, refs, tests).")
        return 1
    print("RESULT: clean — no file lost a dominant share of its lines.")
    return 0


def main():
    ap = argparse.ArgumentParser(description="Flag destructive refactors in a diff.")
    ap.add_argument("diff", nargs="?", help="unified diff file to analyze")
    ap.add_argument("--before", help="original file snapshot")
    ap.add_argument("--after", help="rewritten file snapshot")
    ap.add_argument("--threshold", type=int, default=50,
                    help="net-negative lines that count as deletion-dominant")
    ap.add_argument("--max-loss", type=float, default=50.0,
                    help="max percent of a file that may be deleted")
    args = ap.parse_args()

    if args.before and args.after:
        files = from_snapshots(args.before, args.after)
    elif args.diff:
        try:
            text = open(args.diff, "r", encoding="utf-8").read()
        except OSError as exc:
            print(f"cannot read {args.diff}: {exc}", file=sys.stderr)
            return 2
        files = parse_unified(text)
        if not files:
            print("no file sections found in the diff", file=sys.stderr)
            return 2
    else:
        ap.print_usage(file=sys.stderr)
        return 2

    return report(files, args.threshold, args.max_loss)


if __name__ == "__main__":
    raise SystemExit(main())
