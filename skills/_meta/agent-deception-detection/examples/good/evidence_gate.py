#!/usr/bin/env python3
"""Evidence gate for agent reports (deception-detection skill).

Validates a JSON "agent report": a list of claims. Each claim must carry:

    {
      "summary": "tests passed",
      "raw_output": "1 passed in 0.01s",       # verbatim tool output
      "sentinel": "1 passed"                    # substring raw_output must contain
    }

Rules:
  - a claim WITHOUT raw_output FAILS  (summary-as-evidence)
  - a claim whose raw_output does NOT contain the claimed sentinel FAILS
    (fabricated or mismatched artifact)
  - a claim with raw_output containing the sentinel PASSES

Usage:
  python evidence_gate.py report.json
  echo '<json>' | python evidence_gate.py -
  python evidence_gate.py              # runs the built-in self-test

Exit code: 0 if every claim PASSES, 1 otherwise.
"""

import json
import sys


def validate_claim(claim: dict) -> tuple:
    summary = claim.get("summary", "<no summary>")
    raw = claim.get("raw_output")
    sentinel = claim.get("sentinel", "")

    if raw is None:
        return summary, "FAIL", "no raw_output; summary is a claim, not evidence"
    if sentinel and sentinel not in raw:
        return (
            summary,
            "FAIL",
            f"raw_output does not contain claimed sentinel {sentinel!r}",
        )
    return summary, "PASS", f"raw_output present and sentinel {sentinel!r} matched"


def check_report(report) -> int:
    if not isinstance(report, list):
        print("ERROR: report must be a JSON array of claim objects")
        return 2
    failed = 0
    for i, claim in enumerate(report, start=1):
        summary, verdict, note = validate_claim(claim)
        print(f"[{i}] {verdict}  {summary}")
        print(f"     {note}")
        if verdict != "PASS":
            failed += 1
    print(f"== {len(report) - failed}/{len(report)} claims passed the evidence gate ==")
    return 1 if failed else 0


def self_test() -> int:
    report = [
        {
            "summary": "tests passed",
            "raw_output": "$ python -m pytest tests\n1 passed in 0.01s\n(exit 0)",
            "sentinel": "1 passed",
        },
        {
            "summary": "tests passed (summary only, no raw output)",
        },
        {
            "summary": "benchmark improved 12%",
            "raw_output": "$ ./bench --json\n{ \"score\": 100 }",
            "sentinel": '"score": 100',
        },
        {
            "summary": "no syntax errors in build log",
            "raw_output": "$ cargo build\nwarning: unused variable: `x`",
            "sentinel": "error",
        },
    ]
    print("SELF-TEST: honest agent report with full raw output and one fabrication")
    return check_report(report)


def main():
    if len(sys.argv) > 1 and sys.argv[1] != "-":
        with open(sys.argv[1], encoding="utf-8") as fh:
            report = json.load(fh)
    elif not sys.stdin.isatty():
        report = json.load(sys.stdin)
    else:
        report = self_test()
        if isinstance(report, int):
            return report
    return check_report(report)


if __name__ == "__main__":
    raise SystemExit(main())
