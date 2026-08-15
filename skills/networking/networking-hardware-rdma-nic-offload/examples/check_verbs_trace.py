#!/usr/bin/env python3
"""check_verbs_trace.py - host-side stand-in for the verbs-program rules this
skill teaches. It models a small, honest subset of the RDMA verbs semantics
on plain text traces (one op per line, '#' comments):

  1. QP state machine: RESET -> INIT -> RTR -> RTS (any state -> ERROR)
  2. post_send (data/atomic) requires QP in RTS
  3. post_recv requires QP >= RTR
  4. RDMA_WRITE needs MR remote_write, RDMA_READ needs remote_read,
     ATOMIC ops need remote_atomic

It is NOT libibverbs. The real check is compiling against the verbs header
and running on hardware/soft-RoCE (see SKILL.md).
Usage: python check_verbs_trace.py <trace.txt>...
"""

import sys

QP_STATES = ["RESET", "INIT", "RTR", "RTS"]
MR_FLAGS = {"local_write", "remote_write", "remote_read", "remote_atomic"}
ALLOWED = {"INIT": {"RESET"}, "RTR": {"INIT"}, "RTS": {"RTR"}}
OP_ACCESS = {"rdma_write": "remote_write", "rdma_read": "remote_read",
             "atomic_fetch_add": "remote_atomic", "atomic_cmp_swp": "remote_atomic"}
DATA_OPS = {"send", "rdma_write", "rdma_read", "atomic_fetch_add", "atomic_cmp_swp"}

def check(path):
    problems = []
    qp = "RESET"
    mr_flags = set()
    try:
        lines = open(path, encoding="utf-8").read().splitlines()
    except OSError as e:
        return [f"{path}: cannot read: {e}"]
    for lineno, raw in enumerate(lines, 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split()
        op, args = parts[0], parts[1:]
        if op == "init":
            continue
        if op == "reg_mr":
            flags = set(args)
            unknown = flags - MR_FLAGS
            if unknown:
                problems.append(f"{path}:{lineno}: unknown MR access flag(s) {sorted(unknown)}")
            mr_flags |= flags & MR_FLAGS
            continue
        if op == "modify_qp":
            target = args[0].upper()
            if target == "ERROR":
                qp = "ERROR"
            elif target not in QP_STATES:
                problems.append(f"{path}:{lineno}: unknown QP state '{target}'")
            elif qp not in ALLOWED.get(target, set()):
                problems.append(f"{path}:{lineno}: illegal QP transition {qp}->{target}")
            else:
                qp = target
            continue
        if op == "post_recv":
            if qp not in ("RTR", "RTS"):
                problems.append(f"{path}:{lineno}: post_recv with QP in {qp} (needs RTR)")
            continue
        if op == "post_send":
            req = args[0] if args else "send"
            if qp != "RTS":
                problems.append(f"{path}:{lineno}: post_send {req} with QP in {qp} (needs RTS)")
            if req in OP_ACCESS and OP_ACCESS[req] not in mr_flags:
                problems.append(f"{path}:{lineno}: {req} requires MR '{OP_ACCESS[req]}' "
                                f"(flags: {sorted(mr_flags) or 'none'})")
            if req not in DATA_OPS and req not in OP_ACCESS:
                problems.append(f"{path}:{lineno}: unknown opcode '{req}'")
            continue
        if op == "poll_cq":
            continue
        problems.append(f"{path}:{lineno}: unknown trace op '{op}'")
    return problems

def main():
    rc = 0
    for path in sys.argv[1:]:
        problems = check(path)
        if problems:
            rc = 1
            for p in problems:
                print("FAIL " + p)
        else:
            print(f"PASS {path}")
    return rc

if __name__ == "__main__":
    raise SystemExit(main())
