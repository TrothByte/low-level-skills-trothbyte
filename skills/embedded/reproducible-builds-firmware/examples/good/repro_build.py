#!/usr/bin/env python3
"""repro_build.py — the reproducible recipe, demonstrated on host.

Builds examples/good/fixed_timestamp.c twice in two fresh temporary
directories using the full reproducible recipe and proves the artifacts are
byte-identical (sha256 match). Also demonstrates the gzip mtime pin.

Recipe demonstrated (portable to firmware build systems):
  1. SOURCE_DATE_EPOCH fixed at a known value
     (real world: SOURCE_DATE_EPOCH=$(git log -1 --format=%ct)).
  2. -ffile-prefix-map=<abs build dir>=src scrubs paths from debug info
     and __FILE__.
  3. -Wl,--no-insert-timestamp (binutils 2.40+) removes the PE/COFF header
     timestamp that otherwise makes MinGW/PE builds non-reproducible.
  4. Compression pinning: gzip mtime fixed to 0 (CLI equivalent: gzip -n).

Run: python examples/good/repro_build.py
"""
import gzip
import hashlib
import os
import shutil
import subprocess
import sys
import tempfile
import time

EPOCH = "1600000000"  # 2020-09-13 12:26:40 UTC
CC = os.environ.get("CC", "gcc")
HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(os.path.dirname(HERE), "good", "fixed_timestamp.c")


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def build(src_dir):
    """Compile fixed_timestamp.c in src_dir with the reproducible recipe."""
    exe = os.path.join(src_dir, "firmware" + (".exe" if os.name == "nt" else ""))
    cmd = [
        CC, "-Wall", "-Wextra", "-Werror", "-g", "-O2",
        "-DSOURCE_DATE_EPOCH=" + EPOCH,
        "-ffile-prefix-map=" + src_dir.replace("\\", "/") + "=src",
        "-Wl,--no-insert-timestamp",
        os.path.join(src_dir, "fixed_timestamp.c"),
        "-o", exe,
    ]
    subprocess.run(cmd, check=True, capture_output=True)
    return exe


def main():
    work = tempfile.mkdtemp(prefix="repro_build_")
    try:
        d1 = os.path.join(work, "buildA")
        d2 = os.path.join(work, "buildB")
        os.makedirs(d1)
        os.makedirs(d2)
        for d in (d1, d2):
            shutil.copy(SRC, os.path.join(d, "fixed_timestamp.c"))

        print("== compile 1 (dir A) ...")
        exe1 = build(d1)
        time.sleep(2)  # let the wall clock move; a reproducible build ignores it
        print("== compile 2 (dir B, 2 s later) ...")
        exe2 = build(d2)

        h1, h2 = sha256(exe1), sha256(exe2)
        print("sha256 build A:", h1)
        print("sha256 build B:", h2)
        ok = h1 == h2
        print("reproducible:", "PASS (byte-identical)" if ok else "FAIL (differ)")

        out1 = subprocess.run([exe1], capture_output=True, text=True).stdout.strip()
        out2 = subprocess.run([exe2], capture_output=True, text=True).stdout.strip()
        print("output A:", out1)
        print("output B:", out2)
        assert out1 == out2 == "build time: 2020-09-13 12:26:40 UTC"

        print("\n== gzip mtime pin (python gzip; CLI equivalent: gzip -n)")
        data = b"firmware image payload\n"
        g1 = gzip.compress(data, mtime=0)
        time.sleep(2)
        g2 = gzip.compress(data, mtime=0)
        g_ok = g1 == g2
        print("gzip mtime=0 sha256:", hashlib.sha256(g1).hexdigest(),
              "==" if g_ok else "!=", hashlib.sha256(g2).hexdigest(),
              "->", "PASS" if g_ok else "FAIL")
        sys.exit(0 if (ok and g_ok) else 1)
    finally:
        shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    main()
