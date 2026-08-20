#!/usr/bin/env python3
"""repro_check.py — run the reproducible-builds-firmware host demonstrations.

Builds the bad and good cases with the local gcc, hashes the outputs, and
reports each check as PASS (reproducible) or FAIL (not reproducible), with
the expected outcome per case.

Checks:
  A. timestamp, bad:    examples/bad/nonrepro_time.c compiled twice two
                        seconds apart -> different sha256 (expected FAIL).
  B. timestamp, good:   examples/good/fixed_timestamp.c compiled twice in
                        different dirs with SOURCE_DATE_EPOCH +
                        -ffile-prefix-map + -Wl,--no-insert-timestamp
                        -> identical sha256 (expected PASS).
  C. path leak:         examples/bad/pathleak.c compiled from two different
                        absolute dirs: without -ffile-prefix-map they differ
                        (expected FAIL); with it they match (expected PASS).
  D. compression:       python gzip default mtime -> differs (expected FAIL);
                        mtime=0 -> identical (expected PASS).

Exit code 0 when every check behaves as documented, 1 otherwise.
Run: python examples/tools/repro_check.py
"""
import gzip
import hashlib
import os
import shutil
import subprocess
import sys
import tempfile
import time

CC = os.environ.get("CC", "gcc")
EPOCH = "1600000000"
HERE = os.path.dirname(os.path.abspath(__file__))
SKILL = os.path.dirname(os.path.dirname(HERE))
BAD = os.path.join(SKILL, "examples", "bad")
GOOD = os.path.join(SKILL, "examples", "good")
GAP_SECONDS = 2


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def compile_to(src, out, extra=()):
    cmd = [CC, "-Wall", "-Wextra", "-g", "-O2"] + list(extra) + [src, "-o", out]
    subprocess.run(cmd, check=True, capture_output=True)


def check(name, expected, actual, detail):
    status = "PASS" if actual else "FAIL"
    ok = (status == expected)
    print("[{}{}] {}: {} ({})".format("OK " if ok else "!! ", status, name, detail,
          "expected " + expected))
    return ok


def main():
    work = tempfile.mkdtemp(prefix="repro_check_")
    results = []
    try:
        # A. __DATE__/__TIME__ -> non-reproducible
        a1 = os.path.join(work, "a1" + (".exe" if os.name == "nt" else ""))
        a2 = os.path.join(work, "a2" + (".exe" if os.name == "nt" else ""))
        compile_to(os.path.join(BAD, "nonrepro_time.c"), a1)
        time.sleep(GAP_SECONDS)
        compile_to(os.path.join(BAD, "nonrepro_time.c"), a2)
        ha1, ha2 = sha256_file(a1), sha256_file(a2)
        results.append(check("A timestamp __DATE__/__TIME__ (bad)", "FAIL",
                             ha1 == ha2, "sha256 {} vs {}".format(ha1[:16], ha2[:16])))

        # B. SOURCE_DATE_EPOCH recipe -> reproducible
        bd1 = os.path.join(work, "buildA")
        bd2 = os.path.join(work, "buildB")
        os.makedirs(bd1)
        os.makedirs(bd2)
        for d in (bd1, bd2):
            shutil.copy(os.path.join(GOOD, "fixed_timestamp.c"), d)
        recipe = [
            "-DSOURCE_DATE_EPOCH=" + EPOCH,
            "-ffile-prefix-map=" + bd1.replace("\\", "/") + "=src",
            "-Wl,--no-insert-timestamp",
        ]
        recipe2 = [
            "-DSOURCE_DATE_EPOCH=" + EPOCH,
            "-ffile-prefix-map=" + bd2.replace("\\", "/") + "=src",
            "-Wl,--no-insert-timestamp",
        ]
        b1 = os.path.join(bd1, "fw" + (".exe" if os.name == "nt" else ""))
        b2 = os.path.join(bd2, "fw" + (".exe" if os.name == "nt" else ""))
        compile_to(os.path.join(bd1, "fixed_timestamp.c"), b1, recipe)
        time.sleep(GAP_SECONDS)
        compile_to(os.path.join(bd2, "fixed_timestamp.c"), b2, recipe2)
        hb1, hb2 = sha256_file(b1), sha256_file(b2)
        results.append(check("B SOURCE_DATE_EPOCH recipe (good)", "PASS",
                             hb1 == hb2, "sha256 {}".format(hb1[:16])))

        # C. build-path leak, no prefix map -> non-reproducible
        cd1 = os.path.join(work, "leakA")
        cd2 = os.path.join(work, "leakB")
        os.makedirs(cd1)
        os.makedirs(cd2)
        for d in (cd1, cd2):
            shutil.copy(os.path.join(BAD, "pathleak.c"), d)
        c1 = os.path.join(cd1, "x" + (".exe" if os.name == "nt" else ""))
        c2 = os.path.join(cd2, "x" + (".exe" if os.name == "nt" else ""))
        compile_to(os.path.join(cd1, "pathleak.c"), c1,
                   ["-Wl,--no-insert-timestamp"])
        compile_to(os.path.join(cd2, "pathleak.c"), c2,
                   ["-Wl,--no-insert-timestamp"])
        hc1, hc2 = sha256_file(c1), sha256_file(c2)
        results.append(check("C path leak, no -ffile-prefix-map (bad)", "FAIL",
                             hc1 == hc2, "sha256 {} vs {}".format(hc1[:16], hc2[:16])))

        # C2. with prefix map -> reproducible
        abs1 = os.path.abspath(cd1).replace("\\", "/")
        abs2 = os.path.abspath(cd2).replace("\\", "/")
        c3 = os.path.join(cd1, "y" + (".exe" if os.name == "nt" else ""))
        c4 = os.path.join(cd2, "y" + (".exe" if os.name == "nt" else ""))
        compile_to(os.path.join(cd1, "pathleak.c"), c3,
                   ["-Wl,--no-insert-timestamp", "-ffile-prefix-map=" + abs1 + "=src"])
        compile_to(os.path.join(cd2, "pathleak.c"), c4,
                   ["-Wl,--no-insert-timestamp", "-ffile-prefix-map=" + abs2 + "=src"])
        hc3, hc4 = sha256_file(c3), sha256_file(c4)
        results.append(check("C2 path leak, with -ffile-prefix-map (good)", "PASS",
                             hc3 == hc4, "sha256 {}".format(hc3[:16])))

        # D. compression mtime
        payload = b"firmware image payload\n"
        d1 = gzip.compress(payload)
        time.sleep(GAP_SECONDS)
        d2 = gzip.compress(payload)
        results.append(check("D gzip default mtime (bad)", "FAIL",
                             d1 == d2,
                             "sha256 {} vs {}".format(hashlib.sha256(d1).hexdigest()[:16],
                                                      hashlib.sha256(d2).hexdigest()[:16])))
        d3 = gzip.compress(payload, mtime=0)
        time.sleep(GAP_SECONDS)
        d4 = gzip.compress(payload, mtime=0)
        results.append(check("D2 gzip mtime=0 (good, CLI: gzip -n)", "PASS",
                             d3 == d4,
                             "sha256 {}".format(hashlib.sha256(d3).hexdigest()[:16])))

        print("--")
        ok_all = all(results)
        print("overall:", "PASS (all checks behave as documented)" if ok_all
              else "FAIL (a check misbehaved)")
        sys.exit(0 if ok_all else 1)
    finally:
        shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    main()
