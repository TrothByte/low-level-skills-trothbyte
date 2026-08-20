#!/usr/bin/env python3
"""Demonstration: a verbatim-looking log that was NEVER produced by a tool.

IMPORTANT (header documentation): every line this program prints is
SYNTHESIZED. No test runner, no build system, and no command line produced
these bytes. The timestamps, exit codes, and PASS/FAIL lines are fabricated
by string concatenation.

Why this fixture exists:
  - "verbatim-looking" is not "verbatim". A reviewer who cannot distinguish
    this output from a real `pytest` run has accepted summary-as-evidence
    dressed up as raw output.
  - Raw output is only evidence when it is tied to a real command the
    reviewer can re-run (command, exit code, cwd), or to a provenance check
    (content hash, file mtime).

The skill's evidence gate rejects this fixture: run it through
`evidence_gate.py` with the sentinel below and note that the gate cannot
distinguish fabrication from real output either -- provenance, not
plausibility, is the arbiter.
"""

import sys

SYNTHESIZED_SENTINEL = "1 passed, 0 failed"

LOG_LINES = [
    "$ python -m pytest tests -q",
    "============================= test session starts =============================",
    "platform win32 -- Python 3.11.8, pytest-8.2.0, pluggy-1.5.0",
    "rootdir: C:\\work\\agent-mission",
    "collecting ... collected 7 items",
    "tests/test_parse.py ......                                            [ 85%]",
    "tests/test_emit.py .                                                  [100%]",
    "",
    "============================== 1 passed, 0 failed =============================",
    "(exit code: 0)",
]


def main():
    print(f"$ echo 'synthesized log fixture; sentinel={SYNTHESIZED_SENTINEL}'")
    for line in LOG_LINES:
        print(line)
    print()
    print(
        f"NOTE: none of the {len(LOG_LINES)} lines above were produced by a tool. "
        "A 'raw output' claim for these bytes is fabricated evidence.",
        file=sys.stderr,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
