# BAD: fabricated_claim.py
# intentionally incorrect
"""
The agent asserts a normative claim with an invented section number and
classifies it KNOWN even though nothing was verified. The section "12.7.3
register allocation" does not exist in any registered source for this claim,
and the "KNOWN" label is unjustified — this is B6 (confident tone on invented
behavior) applied to claim extraction.
"""
# intentionally incorrect


def extract_claim():
    # This "extraction" fabricates the section and skips verification.
    return {
        "claim": "the compiler always keeps struct fields in declaration order at -O3",
        "source": "gcc-manual",          # exists in the real registry
        "section": "12.7.3",             # INVENTED — no such section
        "skill": "abi-layout-reasoning",  # wrong skill for the claim
        "confidence": "MUST_VERIFY",
        "evidence": "KNOWN",              # FABRICATED — nothing was verified
    }


def main():
    rec = extract_claim()
    # No check that the section exists, no check that the source states the
    # claim, no compiler run. The record is emitted as if authoritative.
    print(f"registered: {rec['claim']}")
    print(f"  source={rec['source']} section={rec['section']} evidence={rec['evidence']}")
    print("claim registered OK")  # misleading: provenance is fake


if __name__ == "__main__":
    main()
