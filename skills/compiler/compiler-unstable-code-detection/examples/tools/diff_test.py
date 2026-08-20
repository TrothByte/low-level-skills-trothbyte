#!/usr/bin/env python3
"""diff_test.py — differential runner: same source, several -O levels, one compiler.

Compiles each input C file at -O0/-O1/-O2/-O3 (configurable) with gcc, runs
each binary, captures stdout + exit code, and reports any observable
divergence across levels. A divergence means the code's behavior depends on
the optimizer — the usual cause is undefined behavior in the source.

Usage:
    python examples/tools/diff_test.py <file.c> [file2.c ...]
    python examples/tools/diff_test.py --levels=0,2 --cc=gcc bad/signed_overflow_o2.c
"""

import argparse
import os
import subprocess
import sys
import tempfile

DEFAULT_LEVELS = ["0", "1", "2", "3"]
WARN_FLAGS = ["-Wall", "-Wextra", "-Werror"]


def run(cmd):
    p = subprocess.run(cmd, capture_output=True, text=True)
    return p.returncode, p.stdout, p.stderr


def test_level(src, level, workdir, cc):
    exe = os.path.join(workdir, "prog" + level + (".exe" if os.name == "nt" else ""))
    rc, out, err = run([cc, "-O" + level] + WARN_FLAGS + [src, "-o", exe])
    if rc != 0:
        return (level, None, err.strip().splitlines()[-1] if err.strip() else "compile error")
    rc, out, _ = run([exe])
    return (level, (rc, out), None)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("sources", nargs="+", help=".c files to test")
    ap.add_argument("--levels", default=",".join(DEFAULT_LEVELS), help="comma list, e.g. 0,2,3")
    ap.add_argument("--cc", default="gcc", help="compiler driver (default gcc)")
    args = ap.parse_args()
    levels = [lv.strip() for lv in args.levels.split(",") if lv.strip()]

    failed = False
    for src in args.sources:
        print(f"== {src}")
        results = []
        with tempfile.TemporaryDirectory() as td:
            for lv in levels:
                results.append(test_level(src, lv, td, args.cc))
        for lv, obs, errmsg in results:
            if obs is None:
                print(f"  -O{lv}: COMPILE FAIL ({errmsg})")
            else:
                rc, out = obs
                print(f"  -O{lv}: rc={rc} stdout={out.strip()!r}")
        ok = [obs for _, obs, _ in results if obs is not None]
        if len(set(ok)) > 1:
            print("  RESULT: DIVERGENCE — observable behavior differs across -O levels\n")
            failed = True
        else:
            print("  RESULT: stable across -O levels\n")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
