#!/usr/bin/env python3
"""hardening_audit.py — parse readelf/objdump/checksec output and report which
binary hardening properties are present.

Reads saved tool output files (ELF target: readelf -l -d -h, readelf -n,
objdump -t, objdump -d, checksec --file; host PE: objdump -t / objdump -d
disassembly) and reports PIE, RELRO, BIND_NOW, stack canary, FORTIFY, NX,
CET (IBT/SHSTK) and PAC/BTI presence. Flags missing protections.

Usage:
    python hardening_audit.py examples/good/*.txt      # expect exit 0
    python hardening_audit.py examples/bad/*.txt       # expect exit 1
    python hardening_audit.py -                        # read stdin

Facts are only asserted from evidence actually present in the input; a
protection with no data at all is reported as "unknown (no data)" and does
NOT fail the check, so ELF-only properties do not fail on PE host outputs.
"""
import re
import sys

REPORT = {
    "pie":        ("PIE (ET_DYN / ASLR)", "missing"),
    "relro":      ("RELRO", "missing"),
    "bind_now":   ("BIND_NOW (immediate binding)", "missing"),
    "canary":     ("stack canary", "missing"),
    "fortify":    ("FORTIFY (*_chk)", "missing"),
    "nx":         ("NX (noexec stack)", "missing"),
    "cet_ibt":    ("CET IBT (endbr64 / IBT property)", "missing"),
    "cet_shstk":  ("CET SHSTK (shadow stack)", "missing"),
    "pac_bti":    ("ARM64 PAC/BTI", "missing"),
}
# fact -> "missing" is the danger state; "unknown" (no data) is neutral.
FOUND = "present"
MISSING = "missing"
UNKNOWN = "unknown"
NEUTRAL = "not-applicable"


def _read(path: str) -> str:
    if path == "-":
        return sys.stdin.read()
    with open(path, encoding="utf-8", errors="replace") as fh:
        return fh.read()


def sniff(text: str) -> set:
    kinds = set()
    if "ELF Header:" in text and re.search(r"^\s*Type:", text, re.M):
        kinds.add("readelf_h")
    if "Program Headers:" in text:
        kinds.add("readelf_l")
    if "Dynamic section at offset" in text:
        kinds.add("readelf_d")
    if "NT_GNU_PROPERTY_TYPE_0" in text or re.search(r"Displaying notes found in:.*\.note\.gnu\.property", text):
        kinds.add("readelf_n")
    if "Displaying notes found in:" in text:
        kinds.add("readelf_notes")
    if "SYMBOL TABLE:" in text:
        kinds.add("objdump_t")
    if re.search(r"^\s*[0-9a-f]+:\s+([0-9a-f]{2} )+", text, re.M):
        kinds.add("objdump_d")
    if re.search(r"\bRELRO\b", text) and re.search(r"\bNX\b", text) and re.search(r"\bPIE\b", text):
        kinds.add("checksec")
    if "file format pei-x86-64" in text:
        kinds.add("pe")
    return kinds


def audit(text: str, facts: dict):
    kinds = sniff(text)

    if "readelf_h" in kinds:
        m = re.search(r"^\s*Type:\s+(\S+)", text, re.M)
        if m:
            facts["pie"] = FOUND if m.group(1).upper() == "DYN" else MISSING
        m = re.search(r"^\s*Machine:\s+(.+)", text, re.M)
        if m:
            facts["_machine"] = m.group(1)

    if "readelf_l" in kinds:
        if re.search(r"^\s*GNU_STACK\b", text, re.M):
            for line in text.splitlines():
                if line.strip().startswith("GNU_STACK"):
                    flags = line.split()[-1]
                    facts["nx"] = FOUND if "E" not in flags else MISSING
                    break
        if re.search(r"^\s*GNU_RELRO\b", text, re.M):
            if facts["relro"] != MISSING:
                facts["relro"] = "partial"  # upgrade to full only if BIND_NOW seen
        else:
            facts["relro"] = MISSING

    if "readelf_d" in kinds:
        if re.search(r"\(BIND_NOW\)", text) or re.search(r"\(FLAGS\)\s+BIND_NOW", text):
            facts["bind_now"] = FOUND
            if facts["relro"] == "partial":
                facts["relro"] = FOUND  # full RELRO = RELRO + BIND_NOW
        else:
            facts["bind_now"] = MISSING

    if "readelf_n" in kinds or "readelf_notes" in kinds:
        m_feat = re.search(r"x86 feature:\s*([^\n]+)", text)
        m_arm = re.search(r"aarch64 feature:\s*([^\n]+)", text)
        if m_feat:
            feats = m_feat.group(1).upper()
            facts["cet_ibt"] = FOUND if "IBT" in feats else MISSING
            facts["cet_shstk"] = FOUND if "SHSTK" in feats else MISSING
            facts["pac_bti"] = NEUTRAL  # x86 build: PAC/BTI not applicable
        if m_arm:
            feats = m_arm.group(1).upper()
            facts["pac_bti"] = FOUND if "BTI" in feats or "PAC" in feats else MISSING
            facts["cet_ibt"] = NEUTRAL  # aarch64 build: CET not applicable
            facts["cet_shstk"] = NEUTRAL
        if not m_feat and not m_arm:
            # notes present but NO GNU property note -> CET/PAC/BTI absent,
            # except properties that do not apply to this machine class.
            machine = facts.get("_machine", "").lower()
            is_x86 = "x86" in machine
            is_arm = "aarch64" in machine or "arm" in machine
            if facts["cet_ibt"] == UNKNOWN:
                facts["cet_ibt"] = NEUTRAL if is_arm else MISSING
            if facts["cet_shstk"] == UNKNOWN:
                facts["cet_shstk"] = NEUTRAL if is_arm else MISSING
            if facts["pac_bti"] == UNKNOWN:
                facts["pac_bti"] = NEUTRAL if is_x86 else MISSING

    if "objdump_t" in kinds:
        if re.search(r"__stack_chk_fail", text):
            facts["canary"] = FOUND
        else:
            facts["canary"] = MISSING

    if "objdump_d" in kinds:
        if re.search(r"__[a-z0-9_]+_chk\b|_chk@plt", text):
            facts["fortify"] = FOUND
        if re.search(r"\bendbr64\b", text):
            facts["cet_ibt"] = FOUND
        if re.search(r"call.*__stack_chk_fail", text):
            facts["canary"] = FOUND
        if not re.search(r"__[a-z0-9_]+_chk\b|_chk@plt", text):
            if facts["fortify"] == UNKNOWN:
                facts["fortify"] = MISSING
        if not re.search(r"\bendbr64\b", text):
            if facts["cet_ibt"] == UNKNOWN:
                facts["cet_ibt"] = MISSING

    if "checksec" in kinds:
        if re.search(r"Full RELRO", text):
            facts["relro"] = FOUND
        elif re.search(r"Partial RELRO", text):
            facts["relro"] = "partial"
        elif re.search(r"No RELRO", text):
            facts["relro"] = MISSING
        if re.search(r"Canary found", text):
            facts["canary"] = FOUND
        elif re.search(r"No canary found", text):
            facts["canary"] = MISSING
        if re.search(r"NX enabled", text):
            facts["nx"] = FOUND
        elif re.search(r"NX disabled", text):
            facts["nx"] = MISSING
        if re.search(r"PIE enabled", text):
            facts["pie"] = FOUND
        elif re.search(r"No PIE", text):
            facts["pie"] = MISSING
        if re.search(r"FORTIFY\s+Yes", text):
            facts["fortify"] = FOUND
        elif re.search(r"FORTIFY\s+No", text):
            facts["fortify"] = MISSING

    if "pe" in kinds:
        # ELF-only properties are not measurable from PE objdump output.
        for k in ("pie", "relro", "bind_now", "nx"):
            if facts[k] == UNKNOWN:
                facts[k] = NEUTRAL


def main() -> int:
    paths = sys.argv[1:]
    if not paths:
        print("usage: python hardening_audit.py <readelf/objdump/checksec output files...>")
        return 2
    facts = {k: UNKNOWN for k in REPORT}
    for p in paths:
        try:
            text = _read(p)
        except OSError as exc:
            print(f"error: {p}: {exc}")
            return 2
        audit(text, facts)

    print("=== hardening audit ===")
    bad = 0
    for key, (label, _danger) in REPORT.items():
        st = facts[key]
        if st == "partial":
            line = f"[PARTIAL] {label}: GNU_RELRO without BIND_NOW (full RELRO missing)"
            bad += 1
        elif st == FOUND:
            line = f"[PASS] {label}: present"
        elif st == MISSING:
            line = f"[FAIL] {label}: MISSING"
            bad += 1
        elif st == NEUTRAL:
            line = f"[n/a]  {label}: not applicable or not in this evidence"
        else:
            line = f"[?]    {label}: no evidence in input"
        print(line)
    print(f"=== result: {'FAIL' if bad else 'PASS'} ({bad} missing) ===")
    return 1 if bad else 0


if __name__ == "__main__":
    raise SystemExit(main())
