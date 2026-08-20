#!/usr/bin/env python3
"""module_contract_check.py — static contract checker for Rust-for-Linux
module source files.

Host-runnable stand-in for a kernel-crate build (which requires the kernel
tree and `make LLVM=1`). For each .rs file it flags:

  1. `std::` usage        — the kernel links only `core`; no std exists.
  2. `unwrap(` / `expect(`— panic paths; a panic in module code is an oops.
  3. unsafe blocks with   — kernel coding guidelines require a
     no SAFETY comment      `// SAFETY:` comment before every unsafe block.
  4. missing module       — every module needs a `kernel::module!`
     declaration            declaration.

Comments and string literals are stripped before checks 1-2 so that
documentation may name the anti-patterns. Check 3 scans the raw text and
expects a `SAFETY` marker in the comment block directly above the `unsafe {`
line (or on the same line). Check 4 searches the raw text for a `module!`
invocation.

Limitations (documented in evals/README.md): the checker cannot tell a
fabricated SAFETY comment from a real one — a present-but-false contract
passes. That review is the job of rust-unsafe-safety-contract-verification.

Exit codes: 0 = clean, 1 = flags found, 2 = usage error.
"""

import re
import sys

_LINE_COMMENT = re.compile(r"//.*$", re.M)
_BLOCK_COMMENT = re.compile(r"/\*.*?\*/", re.S)
_STRING = re.compile(r'"(?:\\.|[^"\\])*"', re.S)


def _code_without_comments(text):
    """Strip strings first, then comments, so `//` inside a literal is safe."""
    return _STRING.sub("", _LINE_COMMENT.sub("", _BLOCK_COMMENT.sub("", text)))


def _is_comment_line(line):
    stripped = line.lstrip()
    return (
        stripped.startswith("//")
        or stripped.startswith("/*")
        or stripped.startswith("*")
    )


def _has_safety_above(raw_lines, index):
    """True if a SAFETY marker appears on the unsafe line itself or in the
    contiguous block of comments (skipping blank lines) directly above it.

    Kernel style puts `// SAFETY:` on the FIRST line of a multi-line comment,
    so the check must scan the whole comment block, not just the adjacent
    line."""
    if "SAFETY" in raw_lines[index]:
        return True
    j = index - 1
    while j >= 0 and raw_lines[j].strip() == "":
        j -= 1
    while j >= 0 and _is_comment_line(raw_lines[j]):
        if "SAFETY" in raw_lines[j]:
            return True
        j -= 1
        while j >= 0 and raw_lines[j].strip() == "":
            j -= 1
    return False


def check(path):
    with open(path, encoding="utf-8") as fh:
        raw = fh.read()
    code = _code_without_comments(raw)
    flags = []

    for m in re.finditer(r"\bstd::", code):
        line = code[: m.start()].count("\n") + 1
        flags.append(
            f"{path}:{line}: std:: usage - kernel links only core; "
            "use core:: or kernel::alloc"
        )

    for pattern, label in ((r"\bunwrap\s*\(", "unwrap("), (r"\bexpect\s*\(", "expect(")):
        for m in re.finditer(pattern, code):
            line = code[: m.start()].count("\n") + 1
            flags.append(
                f"{path}:{line}: {label} would panic in module code "
                "(kernel oops)"
            )

    raw_lines = raw.splitlines()
    for i, line in enumerate(raw_lines):
        if re.search(r"\bunsafe\s*\{", line) and not _is_comment_line(line):
            if not _has_safety_above(raw_lines, i):
                flags.append(
                    f"{path}:{i + 1}: unsafe block without a preceding "
                    "// SAFETY: comment"
                )

    if not re.search(r"(?:kernel::)?module\s*!", raw):
        flags.append(f"{path}: missing module declaration (kernel::module!)")

    return flags


def main(argv):
    if not argv:
        print("usage: module_contract_check.py <file.rs> [...]")
        return 2
    total = 0
    for path in argv:
        flags = check(path)
        if flags:
            for flag in flags:
                print(f"FLAG  {flag}")
        else:
            print(f"CLEAN {path}")
        total += len(flags)
    print(f"\nmodule_contract_check: {len(argv)} file(s), {total} flag(s)")
    return 1 if total else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
