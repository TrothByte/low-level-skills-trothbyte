#!/usr/bin/env python3
"""binding_check.py — host-runnable model of the shader<->host descriptor mirror.

Parses a simplified GLSL compute shader (layout(set=, binding=, ...) buffer/
uniform/sampler/image declarations) and a host descriptor layout (JSON), and
verifies they match 1:1, flagging the mismatches that Vulkan only reports via
validation layers (or silently produces garbage for).

Usage:
    python binding_check.py <shader.comp> <host_layout.json>

Exit code 0 = PASS (1:1 match), 1 = FAIL (mismatches found).

Host layout JSON schema:
{
  "sets": [
    {"set": 0, "bindings": [
       {"binding": 0, "type": "STORAGE_BUFFER", "count": 1,
        "stageFlags": ["COMPUTE"]}
    ]}
  ]
}

Runs with plain python 3.11; no third-party packages.
"""
import json
import re
import sys

KNOWN_TYPES = {"STORAGE_BUFFER", "UNIFORM_BUFFER", "STORAGE_IMAGE",
               "SAMPLED_IMAGE"}


def parse_qualifiers(q):
    """Parse a GLSL layout(...) qualifier list into a dict, case-insensitive."""
    out = {}
    for part in q.split(","):
        part = part.strip()
        if not part:
            continue
        if "=" in part:
            k, v = part.split("=", 1)
            out[k.strip().lower()] = v.strip()
        else:
            out[part.strip().lower()] = ""
    return out


def parse_shader(path):
    """Extract (local_size, [bindings]) from a simplified GLSL compute shader."""
    text = open(path, encoding="utf-8").read()
    bindings = []

    m = re.search(r"local_size_x\s*=\s*(\d+)", text)
    local_size = int(m.group(1)) if m else None

    for m in re.finditer(r"layout\s*\(([^)]*)\)", text):
        quals = parse_qualifiers(m.group(1))
        if "push_constant" in quals:
            continue
        if "set" not in quals or "binding" not in quals:
            continue
        tail = text[m.end():]
        tm = re.match(r"[^{;\[\]]*", tail)
        stmt = tm.group(0).strip() if tm else ""
        toks = re.findall(r"[\w]+", stmt)
        if not toks:
            continue
        key = toks[0]
        rest = toks[1:]
        if key == "buffer":
            kind, name = "STORAGE_BUFFER", (rest[0] if rest else "?")
        elif key == "uniform":
            if not rest:
                continue
            nxt = rest[0]
            if nxt.startswith("sampler") or nxt.startswith("texture"):
                kind, name = "SAMPLED_IMAGE", (rest[1] if len(rest) > 1 else "?")
            elif nxt.startswith("image"):
                kind, name = "STORAGE_IMAGE", (rest[1] if len(rest) > 1 else "?")
            else:
                kind, name = "UNIFORM_BUFFER", nxt
        else:
            continue
        am = re.search(r"\[(\d+)\]", stmt)
        count = int(am.group(1)) if am else 1
        bindings.append({
            "set": int(quals["set"]),
            "binding": int(quals["binding"]),
            "type": kind,
            "count": count,
            "name": name,
        })

    return {"local_size": local_size, "bindings": bindings}


def parse_host(path):
    """Host layout JSON -> {set: {binding: info}}."""
    data = json.load(open(path, encoding="utf-8"))
    by_set = {}
    for s in data.get("sets", []):
        by_set.setdefault(int(s["set"]), {})
        for b in s.get("bindings", []):
            by_set[int(s["set"])][int(b["binding"])] = b
    return by_set


def compare(shader, host):
    """Return (errors, notes). errors are mismatches; notes are non-fatal."""
    errors, notes = [], []
    for b in shader["bindings"]:
        hb = host.get(b["set"], {}).get(b["binding"])
        if hb is None:
            errors.append(
                f"set={b['set']} binding={b['binding']} ({b['name']}): "
                f"shader declares {b['type']} but the host layout has NO such "
                f"binding")
            continue
        if hb["type"] != b["type"]:
            errors.append(
                f"set={b['set']} binding={b['binding']} ({b['name']}): "
                f"type mismatch — shader={b['type']}, host={hb['type']}")
        if int(hb.get("count", 1)) < b["count"]:
            errors.append(
                f"set={b['set']} binding={b['binding']} ({b['name']}): "
                f"shader array count {b['count']} exceeds host "
                f"descriptorCount {hb.get('count', 1)}")
        stages = hb.get("stageFlags") or []
        if "COMPUTE" not in stages:
            errors.append(
                f"set={b['set']} binding={b['binding']} ({b['name']}): "
                f"host stageFlags {stages} does not include COMPUTE for a "
                f"compute shader")

    for s, host_bindings in host.items():
        for bno, hb in host_bindings.items():
            if not any(sh["set"] == s and sh["binding"] == bno
                       for sh in shader["bindings"]):
                notes.append(
                    f"set={s} binding={bno}: host-only binding "
                    f"(legal — unused by this shader)")
    return errors, notes


def main():
    if len(sys.argv) != 3:
        print("usage: binding_check.py <shader.comp> <host_layout.json>")
        return 2
    shader = parse_shader(sys.argv[1])
    host = parse_host(sys.argv[2])

    print(f"shader  : {sys.argv[1]}")
    print(f"host    : {sys.argv[2]}")
    ls = shader["local_size"]
    print(f"local_size_x = {ls}  (workgroup size; dispatch count is separate)")
    print("shader bindings:")
    for b in shader["bindings"]:
        arr = f"[{b['count']}]" if b["count"] > 1 else ""
        print(f"  set={b['set']} binding={b['binding']} {b['type']} {b['name']}{arr}")
    print("host bindings:")
    for s in sorted(host):
        for bno in sorted(host[s]):
            hb = host[s][bno]
            print(f"  set={s} binding={bno} {hb['type']} count={hb.get('count', 1)} "
                  f"stages={hb.get('stageFlags', [])}")

    errors, notes = compare(shader, host)
    for e in errors:
        print(f"MISMATCH: {e}")
    for n in notes:
        print(f"note    : {n}")

    if errors:
        print(f"VERDICT: FAIL — {len(errors)} mismatch(es); without "
              "VK_LAYER_KHRONOS_validation the device reads garbage silently.")
        return 1
    print("VERDICT: PASS — every shader binding has a matching host binding "
          "(set, type, count, compute stage).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
