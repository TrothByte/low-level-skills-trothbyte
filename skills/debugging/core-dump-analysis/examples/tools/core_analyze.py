#!/usr/bin/env python3
"""
core_analyze.py — parse gdb batch output into a post-mortem summary.

Input:  gdb output captured from a crash session (stdin or a file path).
        Works on the output of `bt`, `info registers rip rsp rbp`, and a
        memory-map listing (`info files` on Windows gdb, or `info proc
        mappings` on Linux gdb).

Output: a compact summary:
  - faulting thread and signal
  - faulting function (top of backtrace, symbolized)
  - faulting address candidates (rip, signal fault address, "Cannot access
    memory" hints)
  - memory-region classification for every address of interest
  - stack-integrity verdict: return addresses that fall outside any mapped
    region are evidence of a corrupted stack (e.g. clobbered with 0x41414141)

Usage:
  python core_analyze.py gdb_live.txt
  gdb ... | python core_analyze.py

The parser is intentionally tolerant of gdb version and target differences.
It is a deterministic summarizer, not a root-cause engine: it reports what
the evidence shows and where the evidence is missing.
"""

import re
import sys


HEX_RE = r"0x[0-9a-fA-F]+"


def addr(s):
    return int(s, 16)


def parse(text):
    """Return a dict of parsed evidence."""
    ev = {
        "signal": None,
        "signal_thread": None,
        "threads": {},         # tid -> {"name": str, "frames": [(no,addr,func,rest)]}
        "frames": [],          # frames of the faulting thread (or flattened)
        "registers": {},       # name -> int
        "mappings": [],        # (base, end, label)
        "bad_addrs": [],       # 'Cannot access memory at address 0x...'
        "si_addr": None,
        "proc_mappings_unsupported": False,
        "raw": text,
    }
    cur = None  # current thread id from a "Thread N (Thread pid.tid):" header

    for line in text.splitlines():
        line = line.strip()

        m = re.match(r"Thread (\d+) received signal (\S+)", line)
        if m and ev["signal"] is None:
            ev["signal_thread"] = m.group(1)
            ev["signal"] = m.group(2)

        m = re.match(
            r"Thread (\d+) \(Thread \d+\.[0-9a-fA-Fx]+(?: \"([^\"]+)\")?\):",
            line,
        )
        if m:
            cur = m.group(1)
            ev["threads"].setdefault(
                cur, {"name": m.group(2) or "", "frames": []}
            )
            continue

        m = re.match(r"si_addr = (" + HEX_RE + r")", line)
        if m:
            ev["si_addr"] = addr(m.group(1))

        m = re.search(r"Cannot access memory at address (" + HEX_RE + r")", line)
        if m:
            ev["bad_addrs"].append(addr(m.group(1)))

        # frames:  #2  0x4141414141414141 in ?? ()
        #          #1  0x00007ff67a2d14b8 in crash_here (args) at file.c:27
        m = re.match(
            r"#(\d+)\s+(" + HEX_RE + r")\s+in\s+([^\s(]+)\s*(\([^)]*\))?(.*)$", line
        )
        if m:
            frame = (int(m.group(1)), addr(m.group(2)), m.group(3),
                     (m.group(4) or "") + (m.group(5) or ""))
            if cur is not None:
                ev["threads"][cur]["frames"].append(frame)
            else:
                ev["frames"].append(frame)
            continue

        # registers:  rip  0x7fff2608d900  0x7fff2608d900 <ucrtbase!strlen+16>
        m = re.match(r"^([a-z][a-z0-9]+)\s+(" + HEX_RE + r")\s+(" + HEX_RE + r")\b", line)
        if m:
            ev["registers"][m.group(1)] = addr(m.group(2))
            continue

        # mappings, Windows `info files` shape:
        #   0x00007ff67a2d1000 - 0x00007ff67a2d2ae0 is .text [in module]
        m = re.match(
            r"^(" + HEX_RE + r")\s*-\s*(" + HEX_RE + r")\s+is\s+(.+)$", line
        )
        if m:
            ev["mappings"].append((addr(m.group(1)), addr(m.group(2)), m.group(3)))
            continue

        # mappings, Linux `info proc mappings` shape:
        #   0x7ff6f0c00000-0x7ff6f0c01000 r-xp 00000000 00:00 0  [vvar]
        m = re.match(
            r"^(" + HEX_RE + r")\s*-\s*(" + HEX_RE + r")\s+[r-][w-][x-][ps](.*)$", line
        )
        if m:
            ev["mappings"].append((addr(m.group(1)), addr(m.group(2)), m.group(3)))
            continue

        if "proc mappings" in line.lower() and "not supported" in line.lower():
            ev["proc_mappings_unsupported"] = True

    # If thread sections were seen, the faulting thread's frames are the ones
    # that matter; fall back to the flattened list otherwise.
    def _dedupe(frames):
        out = []
        seen = set()
        for fr in frames:
            key = (fr[0], fr[1], fr[2])
            if key in seen:
                continue
            seen.add(key)
            out.append(fr)
        return out

    for frames in ev["threads"].values():
        frames["frames"] = _dedupe(frames["frames"])
    ev["frames"] = _dedupe(ev["frames"])

    if ev["signal_thread"] and ev["signal_thread"] in ev["threads"]:
        ev["frames"] = ev["threads"][ev["signal_thread"]]["frames"]
    elif ev["threads"] and ev["signal"]:
        # signal seen but no matching header: take the first section's frames
        ev["frames"] = next(iter(ev["threads"].values()))["frames"]
    elif ev["threads"]:
        # no signal at all (clean run): no frames are meaningful
        ev["frames"] = []

    return ev


def find_region(mappings, a):
    for base, end, label in mappings:
        if base <= a < end:
            return label
    return None


def summarize(ev):
    out = []
    ap = out.append

    ap("=== core-dump-analysis summary ===")
    if ev["signal"]:
        st = ev["signal_thread"]
        ap(f"signal      : {ev['signal']}" + (f" (thread {st})" if st else ""))
    else:
        ap("signal      : (none detected - no crash evidence)")

    regs = ev["registers"]
    rip = regs.get("rip")
    ap(f"rip         : 0x{rip:016x}" if rip is not None else "rip         : (missing)")

    # fault address candidates
    candidates = []
    if rip is not None:
        candidates.append(("rip", rip))
    if ev["si_addr"] is not None:
        candidates.append(("si_addr", ev["si_addr"]))
    for b in ev["bad_addrs"]:
        candidates.append(("si_addr?", b))

    if candidates:
        for what, a in candidates[:4]:
            region = find_region(ev["mappings"], a)
            if region:
                ap(f"fault @0x{a:016x}  -> {region}")
            else:
                ap(f"fault @0x{a:016x}  -> NOT IN ANY MAPPED REGION")

    # faulting function from frame 0
    if ev["frames"]:
        fn, fa, func, rest = ev["frames"][0]
        ap(f"faulting    : frame {fn} 0x{fa:016x} in {func}{rest[:80]}")
    else:
        ap("faulting    : (no backtrace frames parsed)")

    # stack integrity: return addresses outside mapped regions
    n_frames = len(ev["frames"]) - 1
    unmapped = []
    unsymbolized = []
    for fn, fa, func, rest in ev["frames"][1:]:
        if find_region(ev["mappings"], fa) is None:
            unmapped.append((fn, fa))
        elif func == "??":
            unsymbolized.append((fn, fa))
    if n_frames < 1:
        ap("stack       : (no frames to check)")
    elif unmapped:
        ap(f"STACK CORRUPTION: {len(unmapped)} of {n_frames} return addresses "
           f"outside mapped regions:")
        for fn, fa in unmapped[:6]:
            ap(f"  frame {fn}: 0x{fa:016x} unmapped")
        marker = next((fa for _, fa in unmapped if fa >> 40 == 0x414141), None)
        if marker is not None:
            ap(f"  pattern 0x4141414141414141 ('A' fill) -> buffer overflow / overwritten return address")
    else:
        ap(f"stack       : all {n_frames} return addresses inside mapped regions")
    if unsymbolized:
        ap(f"  note: {len(unsymbolized)} frames mapped but unsymbolized (not necessarily corrupt)")

    # register sanity against maps
    if "rsp" in regs:
        rsp = regs["rsp"]
        region = find_region(ev["mappings"], rsp)
        ap(f"rsp         : 0x{rsp:016x} " + (f"-> {region}" if region else "-> NOT IN ANY MAPPED REGION"))
    if "rbp" in regs:
        rbp = regs["rbp"]
        region = find_region(ev["mappings"], rbp)
        ap(f"rbp         : 0x{rbp:016x} " + (f"-> {region}" if region else "-> NOT IN ANY MAPPED REGION"))

    if ev["proc_mappings_unsupported"]:
        ap("note        : 'info proc mappings' unsupported on this target; used module sections")

    if not ev["frames"] and ev["signal"] is None:
        ap("verdict     : no crash evidence - clean run (no fault)")
    elif ev["frames"]:
        ap(f"verdict     : {len(ev['frames'])} frames, signal {ev['signal']}")

    return "\n".join(out)


def main(argv):
    if len(argv) > 1:
        with open(argv[1], encoding="utf-8", errors="replace") as fh:
            text = fh.read()
    else:
        text = sys.stdin.read()
    print(summarize(parse(text)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
