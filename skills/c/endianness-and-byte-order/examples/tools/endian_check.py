#!/usr/bin/env python3
"""endian_check.py — static checker for endianness/byte-order anti-patterns
in C source files. Flags:

  1. pointer-cast reads/writes on byte buffers:  *(uint16_t *)p, (uint32_t*)buf
  2. whole-struct serialization:                 fwrite(&s, sizeof s, ...),
                                                 memcpy(dst, &s, sizeof s)
                                                 (only when s is a user-defined
                                                 struct type, so the legitimate
                                                 memcpy type-punning of a scalar
                                                 is not flagged)
  3. invented 64-bit host<->network helpers:     htonll / ntohll / be64toh /
     htobe64 (non-portable: POSIX/Linux-only, absent from MSVC)
  4. bitfields used to describe wire/file records (layout is
     implementation-defined, not a byte-order mechanism)
  5. union type punning (union with both an integer and a byte-array member;
     reading the inactive member is UB)

It does NOT flag the legitimate uses of memcpy (type-punning an integer
into a byte array), shift-based serialization, htonl/htons/ntohl/ntohs,
or `#if __BYTE_ORDER__` preprocessor branches.

Usage: python endian_check.py FILE...   (exit 0 = no findings, 1 = findings)
Run the skill eval: endian_check.py ../../good/*.c  -> clean
                   endian_check.py ../../bad/*.c   -> findings
"""
import glob
import re
import sys


def _strip_comments(text):
    """Remove C line and block comments so that descriptions of the
    anti-patterns (which literally spell them out) are not flagged."""
    text = re.sub(r"//[^\n]*", "", text)
    return re.sub(r"/\*.*?\*/", "", text, flags=re.S)


def _struct_variables(text):
    """Return identifiers whose declared type is a user-defined struct."""
    types = set()
    for m in re.finditer(r"\}\s*(\w+)\s*;", text):            # } Name;
        types.add(m.group(1))
    for m in re.finditer(r"typedef\s+struct\s+\w+\s+(\w+)\s*;", text):
        types.add(m.group(1))
    for m in re.finditer(r"typedef\s+(?:struct\s+)?(\w+)\s+(\w+)\s*;", text):
        if m.group(1) not in ("unsigned", "signed", "const"):
            types.add(m.group(2))
    names = set()
    for t in types:
        for m in re.finditer(r"\b" + t + r"\s+(\w+)\s*(?:[;=])", text):
            names.add(m.group(1))
    return names


def check(path):
    text = open(path, encoding="utf-8", errors="replace").read()
    findings = []
    text = _strip_comments(text)

    # 1. pointer-cast access of multi-byte ints: strict-aliasing +
    #    alignment + host byte order all become a hazard.
    for m in re.finditer(
        r"\(\s*(?:const\s+)?uint(?:16|32|64)_t\s*\*+\s*\)", text
    ):
        findings.append(
            f"{path}: pointer cast to uint(16|32|64)_t "
            f"(aliasing/alignment/endianness UB): {m.group(0).strip()}"
        )

    # 2. whole-struct serialization: host padding + byte order baked in.
    struct_vars = _struct_variables(text)
    for m in re.finditer(r"\bfwrite\s*\(\s*&?\s*\w+\s*,\s*sizeof\s+", text):
        findings.append(
            f"{path}: whole-struct fwrite serialization "
            f"(host padding + byte order): {m.group(0).strip()}"
        )
    for m in re.finditer(r"\bmemcpy\s*\(\s*\w+\s*,\s*&\s*(\w+)\s*,\s*sizeof\s+", text):
        if m.group(1) in struct_vars:
            findings.append(
                f"{path}: whole-struct memcpy serialization "
                f"(host padding + byte order): {m.group(0).strip()}"
            )

    # 3. invented 64-bit host<->network helpers (absent on MSVC).
    for name in ("htonll", "ntohll", "htobe64", "be64toh"):
        for m in re.finditer(r"\b" + name + r"\s*\(", text):
            findings.append(
                f"{path}: non-portable 64-bit byte-order helper "
                f"'{name}()' (absent on MSVC; use shifts)"
            )

    # 4. bitfields in record structs (layout implementation-defined).
    for m in re.finditer(
        r"(?<!\w)(?:unsigned\s+)?(?:int|char|uint(?:8|16|32|64)_t)\s+"
        r"\w+\s*:\s*\d+\s*;",
        text,
    ):
        findings.append(
            f"{path}: bitfield in record struct "
            f"(layout implementation-defined): {m.group(0).strip()}"
        )

    # 5. union with an integer member + a byte-array member: reading the
    #    inactive member is type punning, UB in C.
    for m in re.finditer(r"union\s*\w*\s*\{.*?\}", text, re.S):
        body = m.group(0)
        has_int = re.search(r"\b(?:uint(?:8|16|32|64)_t|int|unsigned)\s+\w+\s*;", body)
        has_arr = re.search(r"\w+\s*\[\d+\]\s*;", body)
        if has_int and has_arr:
            findings.append(
                f"{path}: union type punning (integer + array members), "
                f"reading inactive member is UB: {m.group(0).strip()[:60]}"
            )

    return findings


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    paths = []
    for p in sys.argv[1:]:
        if glob.has_magic(p):
            paths.extend(glob.glob(p))
        else:
            paths.append(p)
    total = 0
    for p in paths:
        for f in check(p):
            print("FLAG", f)
            total += 1
    if total:
        print(f"endian_check: {total} finding(s)")
        return 1
    print("endian_check: clean")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
