# GOOD: claim_extract_demo.py
"""
Extracts candidate claims from a SKILL.md /
references snippet into the (text, source, section, skill) model used by
registry/claims.yaml.

Demonstrates the traceability rule: every normative claim needs source,
section, confidence, evidence. The demo parses lines shaped like

  - claim: <text> | source: <source-id> | section: <section> | skill: <id>

and validates the source id against a small registry set. In the real repo,
source_check.py does this against registry/sources.yaml; here we inline a
mini registry so the example runs anywhere.

Run: python examples/good/claim_extract_demo.py
"""

import re
import sys

MINI_SOURCE_REGISTRY = {"iso-c11-n1570", "sysv-amd64-abi", "rust-reference"}

SAMPLE = [
    "claim: signed overflow is UB | source: iso-c11-n1570 | section: 6.5 p5 | skill: c-undefined-behavior",
    "claim: first 6 args in rdi/rsi/rdx/rcx/r8/r9 | source: sysv-amd64-abi | section: 3.2.1 | skill: abi-layout-reasoning",
    "claim: drop runs in scope exit order | source: rust-reference | section: drop-scopes | skill: cpp-object-lifecycle",
]

CLAIM_RE = re.compile(
    r"claim:\s*(.+?)\s*\|\s*source:\s*(\S+)\s*\|\s*section:\s*(.+?)\s*\|\s*skill:\s*(\S+)"
)


def extract(line):
    m = CLAIM_RE.match(line)
    if not m:
        return None
    text, source, section, skill = m.groups()
    # Evidence is classified from the source's authority, never from tone:
    evidence = "KNOWN" if source in {"iso-c11-n1570", "sysv-amd64-abi"} else "INFERRED"
    confidence = "MUST_VERIFY" if source == "iso-c11-n1570" else "MUST_CONSIDER"
    return {
        "claim": text,
        "source": source,
        "section": section,
        "skill": skill,
        "confidence": confidence,
        "evidence": evidence,
    }


def main():
    ok = True
    for line in SAMPLE:
        rec = extract(line)
        if rec is None:
            print(f"MISS  unparseable: {line}")
            ok = False
            continue
        if rec["source"] not in MINI_SOURCE_REGISTRY:
            print(f"WARN  unknown source id: {rec['source']} (claim='{rec['claim']}')")
            ok = False
            continue
        print(f"OK    {rec['source']} :: {rec['section']} :: {rec['skill']}")
        print(f"      {rec['claim']}  [{rec['confidence']} / {rec['evidence']}]")
    if not ok:
        sys.exit(1)
    print("all claims traceable")


if __name__ == "__main__":
    main()
