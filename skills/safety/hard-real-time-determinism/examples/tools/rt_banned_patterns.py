#!/usr/bin/env python3
"""rt_banned_patterns.py - heuristic scanner for hard real-time banned patterns.

AST-less, comment/string-stripped regex + brace-tracking scanner. NOT a WCET
analyzer and NOT proof of hard real-time behavior: it is a fast review loop
for the constructs the hard-real-time-determinism skill bans.

Detected (pattern -> indicator):
  dynamic allocation   malloc | calloc | realloc | free | new | delete
                       in task or ISR paths (free is banned in task paths;
                       new/delete target C++ hard-RT paths)
  recursion            a function that calls itself by name
  exceptions           throw (C++ hard-RT paths)
  wall-clock timing    gettimeofday (deadline logic must use monotonic)
  input scans          strlen/strchr/strrchr/gets/scanf over data (no bound)
  unbounded loops      for(;;)/while(1) without a blocking call in the body;
                       while/for conditions without an ordering bound and
                       without a monotonic counter (data-dependent)

Explicitly NOT flagged (provably bounded patterns):
  for loops whose condition contains an ordering comparison (< <= > >=)
    with a counter that moves to the bound (e.g. i < FRAMES)
  while loops that count down to zero (while (items != 0) with items--)
  blocking task superloops: for(;;) / while(1) whose body calls a
    wait/blocking function (event_wait, vTaskDelay, semaphore take, ...)

KNOWN limits (documented): a `while (p != NULL)` pointer walk has `!=` and is
flagged even though some such loops are provably bounded; a `for (;;)` loop
whose body waits is accepted on the presence of a wait call, not on a proof.
The reviewer still proves every bound.

Usage: python rt_banned_patterns.py FILE.c [FILE.c ...]
Exit:  0 = no banned patterns | 1 = one or more found
"""

import re
import sys

STRIP_COMMENT_BLOCK = re.compile(r"/\*.*?\*/", re.S)
STRIP_COMMENT_LINE = re.compile(r"//[^\n]*")
STRIP_STRING = re.compile(r'"(?:[^"\\]|\\.)*"|\'(?:[^\'\\]|\\.)*\'')
ALLOC_C_RE = re.compile(r"\b(malloc|calloc|realloc|free)\s*\(")
NEW_DELETE_RE = re.compile(r"\b(new|delete)\b(?=\s*(?:\[|[A-Za-z_~]))")
THROW_RE = re.compile(r"\bthrow\b")
GETTIMEOFDAY_RE = re.compile(r"\bgettimeofday\b")
SCAN_RE = re.compile(r"\b(strlen|strchr|strrchr|gets|scanf)\s*\(")
FUNC_OPEN_RE = re.compile(r"\b([A-Za-z_]\w*)\s*\([^;{}]*\)\s*\{", re.S)
KEYWORDS = {"if", "while", "for", "switch", "return", "sizeof", "else", "do"}
SYNC_RE = re.compile(
    r"(?<![A-Za-z0-9])(?:wait|delay|pend|receive|sleep|semaphore|yield|lock|block)(?![A-Za-z0-9_])")


def strip_noise(text):
    """Remove comments and string/char literals, preserving line numbers."""
    text = STRIP_COMMENT_BLOCK.sub(" " * 0, text)
    text = STRIP_COMMENT_LINE.sub("", text)
    return STRIP_STRING.sub(" ", text)


def _find_brace_block(text, pos):
    """Return (body_text, body_end) of the {...} block starting at pos."""
    start = text.find("{", pos)
    if start < 0:
        return None, -1
    depth = 0
    for i in range(start, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return text[start + 1:i], i
    return None, -1


def _prev_block(text, pos):
    """For do { ... } while(...): return the {...} block ending before pos."""
    m = re.search(r"\{\s*\}\s*$", "")
    _ = m
    pre = text[:pos]
    dm = re.search(r"\bdo\s*\{(.*)\}\s*$", pre, re.S)
    if dm:
        return dm.group(1)
    return None


def _line(text, pos):
    return text[:pos].count("\n") + 1


def scan(path):
    with open(path, encoding="utf-8") as fh:
        raw = fh.read()
    text = strip_noise(raw)
    findings = []

    for m in ALLOC_C_RE.finditer(text):
        findings.append((_line(text, m.start()), "dynamic-alloc",
                         "dynamic allocation `%s` in a real-time code path" % m.group(1)))

    for m in NEW_DELETE_RE.finditer(text):
        findings.append((_line(text, m.start()), "dynamic-alloc",
                         "dynamic allocation `%s` (C++) in a real-time code path" % m.group(1)))

    for m in THROW_RE.finditer(text):
        findings.append((_line(text, m.start()), "exception",
                         "`throw` (C++) has unbounded unwind cost"))

    for m in GETTIMEOFDAY_RE.finditer(text):
        findings.append((_line(text, m.start()), "wall-clock",
                         "`gettimeofday` is adjustable; use a monotonic timer"))

    for m in SCAN_RE.finditer(text):
        findings.append((_line(text, m.start()), "input-scan",
                         "`%s` scans input with no bound" % m.group(1)))

    for m in FUNC_OPEN_RE.finditer(text):
        name = m.group(1)
        if name in KEYWORDS:
            continue
        body, _ = _find_brace_block(text, m.end() - 1)
        if body is None:
            continue
        if re.search(r"\b%s\s*\(" % re.escape(name), body):
            findings.append((_line(text, m.start()), "recursion",
                             "function `%s` calls itself (unbounded stack and WCET)" % name))

    for m in re.finditer(r"\bfor\s*\(", text):
        close = _match_parens(text, m.end())
        parts = text[m.end():close].split(";")
        if len(parts) != 3:
            continue
        body, _ = _find_brace_block(text, close + 1)
        classify_loop(findings, text, parts[1], body, m.start(), "for")

    for m in re.finditer(r"\bwhile\s*\(", text):
        body = _prev_block(text, m.start())
        inner = _find_brace_block(text, m.end())
        if body is None and inner[0] is not None:
            body = inner[0]
        if body is None:
            continue
        cm = _match_parens(text, m.end())
        cond = text[m.end():cm]
        classify_loop(findings, text, cond, body, m.start(), "while")

    return findings


def _match_parens(text, pos):
    depth = 1   # the opening paren has already been consumed by the caller
    for i in range(pos, len(text)):
        if text[i] == "(":
            depth += 1
        elif text[i] == ")":
            depth -= 1
            if depth == 0:
                return i
    return len(text)


def has_sync(body):
    return bool(SYNC_RE.search(body))


def classify_loop(findings, text, cond, body, pos, kind):
    c = cond.strip()
    if c in ("", "1", "true", "(1)", "(true)", "TRUE", "(TRUE)"):
        if has_sync(body):
            return
        findings.append((_line(text, pos), "unbounded-loop",
                         "busy %s loop with no blocking/wait call" % kind))
        return
    if re.search(r"[<>]", c):
        return
    m = re.match(r"^\s*([A-Za-z_]\w*)\s*(?:==|!=)\s*", c)
    if m:
        ident = m.group(1)
        if re.search(r"\b%s\s*(--|\+\+)" % re.escape(ident), body or ""):
            return
    findings.append((_line(text, pos), "unbounded-loop",
                     "data-dependent %s loop condition without a bound: %s" % (kind, c)))


def main():
    paths = sys.argv[1:]
    if not paths:
        print("usage: rt_banned_patterns.py FILE.c [FILE.c ...]")
        return 2

    total = 0
    for path in paths:
        findings = scan(path)
        if findings:
            print("%s: %d banned pattern(s) detected:" % (path, len(findings)))
            for lineno, kind, msg in sorted(set(findings)):
                print("  %s:%d: [%s] %s" % (path, lineno, kind, msg))
            total += len(set(findings))
        else:
            print("%s: OK - no banned patterns detected" % path)

    if total:
        print("\nFAIL: %d banned pattern(s) across %d file(s)" % (total, len(paths)))
        return 1
    print("\nPASS: no banned patterns detected across %d file(s)" % len(paths))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
