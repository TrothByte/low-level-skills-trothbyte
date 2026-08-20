#!/usr/bin/env python3
"""napi_check.py — static checker for C driver snippets with NAPI poll paths.

Flags the classic NAPI bugs in driver text (no kernel headers needed, so it
runs on this host against the TARGET-ONLY sketches):

  1. netif_receive_skb() inside a NAPI poll() function  -> should be
     napi_gro_receive() (GRO bypassed / wrong semantics for GRO-enabled NICs).
  2. packet processing (netif_receive_skb/napi_gro_receive) inside the IRQ
     handler -> heavy work in IRQ context defeats NAPI.
  3. napi_complete()/napi_complete_done() in poll() WITHOUT a preceding
     `if (work_done < budget)` guard -> must not complete at full budget.
  4. blocking APIs in poll() (mutex_lock, msleep, wait_event, ...) ->
     forbidded in softirq context.
  5. napi_schedule*() in the IRQ handler without masking the queue IRQ ->
     re-entrancy / interrupt storm.

Heuristics are textual and documented: comments are stripped, functions are
split by brace counting, a call is considered "guarded" if an `if (...)` line
mentioning `work_done` (or a `budget` comparison) appears within the preceding
6 lines. Honest limitation: this cannot evaluate C control flow; a real review
reads the surrounding code.

Usage:
  python examples/tools/napi_check.py examples/good/driver_sketch.c \
        examples/bad/driver_sketch.c

Exit 0 = no violations found; 1 = violations found.
"""

import re
import sys


BLOCKING_APIS = [
    "mutex_lock", "mutex_lock_interruptible", "mutex_lock_killable",
    "msleep", "msleep_interruptible", "usleep_range", "schedule_timeout",
    "wait_event", "wait_event_interruptible", "wait_event_timeout",
    "wait_for_completion", "down(", "down_interruptible", "flush_work",
    "flush_delayed_work", "flush_scheduled_work", "kthread_stop",
    "cond_resched", "might_sleep",
]

GUARD_RE = re.compile(r"\bif\s*\([^)]*\b(?:work_done|budget)\b")
NAPI_COMPLETE_RE = re.compile(r"\bnapi_complete(?:_done)?\s*\(")
NETIF_RECV_RE = re.compile(r"\bnetif_receive_skb\s*\(")
GRO_RECV_RE = re.compile(r"\bnapi_gro_receive\s*\(")
SCHED_RE = re.compile(r"\bnapi_schedule(?:_prep)?\s*\(|__napi_schedule\s*\(")
MASK_RE = re.compile(r"(?:disable|mask).*(?:irq|irq)", re.IGNORECASE)
POLL_SIG_RE = re.compile(r"\b(int|void)\s+\w+\s*\([^;]*\bbudget\b[^)]*\)")
IRQ_SIG_RE = re.compile(r"\b(irqreturn_t|int|void)\s+[\w_]*irq[\w_]*\s*\(")


def strip_comments(text):
    out = []
    i = 0
    n = len(text)
    in_str = False
    while i < n:
        c = text[i]
        nxt = text[i + 1] if i + 1 < n else ""
        if not in_str and c == "/" and nxt == "/":
            while i < n and text[i] != "\n":
                i += 1
            continue
        if not in_str and c == "/" and nxt == "*":
            i += 2
            while i < n and not (text[i] == "*" and i + 1 < n and text[i + 1] == "/"):
                i += 1
            i += 2
            continue
        if c == '"':
            in_str = not in_str
        out.append(c)
        i += 1
    return "".join(out)


def split_functions(text):
    """Split C text into (name, signature, body, start_line) chunks by brace
    counting. Handles signatures whose opening brace is on the following line
    and one-line bodies. Returns a list of dicts."""
    funcs = []
    lines = text.splitlines()
    n = len(lines)
    sig_start = re.compile(r"^\s*(?:static\s+)?(?:inline\s+)?[\w\s\*]+\b\w+\s*\(")
    non_func = re.compile(r"^\s*(?:if|while|for|return|switch|else|do|case|#)\b")
    i = 0
    while i < n:
        stripped = lines[i].strip()
        if (sig_start.match(stripped) and "(" in stripped
                and ")" in stripped and not non_func.match(stripped)):
            sig_lines = [lines[i]]
            i += 1
            while i < n and "{" not in lines[i]:
                sig_lines.append(lines[i])
                i += 1
            if i >= n:
                break
            body = list(sig_lines)
            depth = 0
            while i < n:
                body.append(lines[i])
                depth += lines[i].count("{") - lines[i].count("}")
                i += 1
                if depth <= 0:
                    break
            sig = " ".join(sig_lines)
            name = sig.split("(")[0].split()[-1] if "(" in sig else sig
            funcs.append({
                "name": name,
                "sig": sig,
                "body": "\n".join(body),
                "start_line": i - len(body),
            })
            continue
        i += 1
    return funcs


def classify(funcs):
    polls = []
    irqs = []
    for f in funcs:
        if POLL_SIG_RE.search(f["sig"]) and "napi" in f["sig"]:
            polls.append(f)
        elif IRQ_SIG_RE.search(f["sig"]) or "irq" in f["name"]:
            irqs.append(f)
    return polls, irqs


def check_poll(f, findings):
    body = f["body"]
    lines = body.splitlines()

    for m in NETIF_RECV_RE.finditer(body):
        findings.append(
            f"{f['name']}:{line_of(lines, m.start())}: netif_receive_skb() in "
            "poll path bypasses GRO; use napi_gro_receive() for GRO-enabled NICs"
        )

    for m in NAPI_COMPLETE_RE.finditer(body):
        ln = line_of(lines, m.start())
        window = "\n".join(lines[max(0, ln - 7):ln])
        if not GUARD_RE.search(window):
            findings.append(
                f"{f['name']}:{ln}: napi_complete() called without "
                "`work_done < budget` guard; instance must stay scheduled at "
                "full budget"
            )

    for api in BLOCKING_APIS:
        for m in re.finditer(r"\b" + re.escape(api) + r"\s*\(", body):
            findings.append(
                f"{f['name']}:{line_of(lines, m.start())}: blocking call "
                f"`{api}()` in poll() — forbidded in softirq context"
            )


def check_irq(f, findings):
    body = f["body"]
    lines = body.splitlines()

    for m in re.finditer(r"\b(?:netif_receive_skb|napi_gro_receive)\s*\(", body):
        findings.append(
            f"{f['name']}:{line_of(lines, m.start())}: packet processing in "
            "IRQ handler defeats NAPI — schedule the poll round instead"
        )

    sched = [m for m in SCHED_RE.finditer(body)]
    if sched and not MASK_RE.search(body):
        findings.append(
            f"{f['name']}:{line_of(lines, sched[0].start())}: napi_schedule() "
            "without masking the queue IRQ — re-entrancy / interrupt storm"
        )


def line_of(lines, offset):
    count = 0
    for idx, l in enumerate(lines):
        count += len(l) + 1
        if count > offset:
            return idx + 1
    return len(lines)


def check_file(path):
    text = open(path, encoding="utf-8", errors="replace").read()
    text = strip_comments(text)
    funcs = split_functions(text)
    polls, irqs = classify(funcs)
    findings = []
    for f in polls:
        check_poll(f, findings)
    for f in irqs:
        check_irq(f, findings)
    return findings, polls, irqs


def main():
    paths = sys.argv[1:]
    if not paths:
        print("usage: napi_check.py <driver.c> [...]")
        return 2
    total = 0
    for path in paths:
        findings, polls, irqs = check_file(path)
        total += len(findings)
        tag = "OK" if not findings else f"{len(findings)} VIOLATION(S)"
        print(f"[{tag}] {path} (polls={len(polls)}, irqs={len(irqs)})")
        for f in findings:
            print(f"  FLAG {f}")
    print(f"\nnapi_check: {total} violation(s) across {len(paths)} file(s)")
    return 0 if total == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
