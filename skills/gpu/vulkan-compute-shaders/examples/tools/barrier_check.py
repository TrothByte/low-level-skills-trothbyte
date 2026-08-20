#!/usr/bin/env python3
"""barrier_check.py — host-runnable model of Vulkan memory-barrier placement.

Models a recorded command sequence as JSON ops: buffer writes/reads with a
pipeline stage and an access mask, and vkCmdPipelineBarrier calls with
srcStage/srcAccess (the producing side) and dstStage/dstAccess (the consuming
side). Verifies that every write->read (RAW) and read->write (WAR) dependency
on the same buffer has a barrier strictly between them whose masks cover the
producer and the consumer. Flags missing barriers and wrong masks/stages.

Usage:
    python barrier_check.py <sequence.json>

Exit code 0 = SAFE (all dependencies synchronized), 1 = UNSAFE (hazards).

Sequence JSON schema (each op is a recorded command):
  {"op": "write"|"read", "buffer": "name", "stage": "COMPUTE"|"TRANSFER"|...,
   "access": "SHADER_WRITE"|"SHADER_READ"|"TRANSFER_READ"|...}
  {"op": "barrier", "srcStage": [...], "srcAccess": [...],
   "dstStage": [...], "dstAccess": [...]}

Coverage rule (mirrors vkCmdPipelineBarrier semantics):
  a barrier covers a dependency when
    srcStage covers the producer stage AND srcAccess covers the producer access
    dstStage covers the consumer stage AND dstAccess covers the consumer access
  where the broad flag "ALL_COMMANDS" covers any stage and "MEMORY_READ" /
  "MEMORY_WRITE" cover any access. Specific flags cover only themselves.

Runs with plain python 3.11; no third-party packages.
"""
import json
import sys

BROAD_STAGE = "ALL_COMMANDS"
BROAD_ACCESS = {"MEMORY_READ", "MEMORY_WRITE"}


def covers_stage(mask, stage):
    return BROAD_STAGE in mask or stage in mask


def covers_access(mask, access):
    return bool(BROAD_ACCESS & set(mask)) or access in mask


def barrier_covers(bar, producer, consumer):
    """Return (ok, why_list) for one barrier vs one dependency edge."""
    why = []
    if not covers_stage(bar["srcStage"], producer["stage"]):
        why.append(f"srcStage {bar['srcStage']} does not cover producer "
                   f"stage {producer['stage']}")
    if not covers_access(bar["srcAccess"], producer["access"]):
        why.append(f"srcAccess {bar['srcAccess']} does not cover producer "
                   f"access {producer['access']}")
    if not covers_stage(bar["dstStage"], consumer["stage"]):
        why.append(f"dstStage {bar['dstStage']} does not cover consumer "
                   f"stage {consumer['stage']}")
    if not covers_access(bar["dstAccess"], consumer["access"]):
        why.append(f"dstAccess {bar['dstAccess']} does not cover consumer "
                   f"access {consumer['access']}")
    return not why, why


def analyze(seq, name):
    print(f"sequence: {name}")
    ops = seq["ops"]
    events, barriers = [], []
    for i, op in enumerate(ops):
        if op["op"] == "barrier":
            barriers.append((i, op))
        else:
            events.append((i, op))

    hazards = []
    for (i, a) in events:
        for (j, b) in events:
            if i >= j or b["buffer"] != a["buffer"]:
                continue
            if a["op"] == "write" and b["op"] == "read":
                kind = "RAW (write->read)"
                producer, consumer = a, b
            elif a["op"] == "read" and b["op"] == "write":
                kind = "WAR (read->write)"
                producer, consumer = b, a
            else:
                continue
            between = [(k, bar) for (k, bar) in barriers if i < k < j]
            covered = False
            reasons = []
            for (k, bar) in between:
                ok, why = barrier_covers(bar, producer, consumer)
                if ok:
                    covered = True
                    break
                reasons = why
            if not covered:
                if between:
                    msg = ("barrier present but NOT covering: "
                           + "; ".join(reasons))
                else:
                    msg = "NO barrier between the two commands"
                hazards.append(
                    f"HARDWARE HAZARD {kind} on {a['buffer']}: "
                    f"op@{i}({a['op']} {a['stage']} {a['access']}) -> "
                    f"op@{j}({b['op']} {b['stage']} {b['access']}) — {msg}")

    for h in hazards:
        print(f"  FLAG: {h}")
    if not hazards:
        print("  no hazards: every same-buffer write->read and read->write")
        print("  dependency has a covering vkCmdPipelineBarrier between the")
        print("  commands. Same-queue submission order is NOT sufficient.")
        print("  verdict: SAFE (0 hazards)")
    else:
        print(f"  verdict: UNSAFE ({len(hazards)} hazard(s)); works only by")
        print("  luck — later commands may read stale data.")
    print()
    return not hazards


def main():
    if len(sys.argv) != 2:
        print("usage: barrier_check.py <sequence.json>")
        return 2
    seq = json.load(open(sys.argv[1], encoding="utf-8"))
    ok = analyze(seq, seq.get("comment", sys.argv[1]))
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
