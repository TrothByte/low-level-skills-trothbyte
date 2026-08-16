# BAD: plausibility-only warning dismissal. The filter dismisses a buffer
# overflow warning because "the main parser validates the length" -- a
# plausible story -- WITHOUT a reachability witness. A second entry point
# (parse_async) reaches the vulnerable copy with an unvalidated length. This
# reproduces the failure mode arxiv-2606-15122 documents: LLM-based filtering
# falsely dismissed a real vulnerability (the Evident case).
# # intentionally incorrect
#
# The correct disposition is in
# examples/good/good_require_unreachability_witness.py.

ENTRIES = {
    "parse_http":  [("read", "n"), ("guard_max", 16), ("copy", 32)],
    "parse_async": [("read", "n"), ("copy", 32)],     # 2nd entry, unguarded
}


def plausible_dismissal(warning):
    """The 'reasoning': narrative only, no witness, one entry considered."""
    print(f"  warning {warning}:")
    print("    'the main parser validates n <= 16, so the overflow cannot")
    print("     occur in practice' -> DISMISS (no witness)")
    return "DISMISS"


def reachability_check():
    """The eval's ground truth: walks ALL entries."""
    for name, fn in ENTRIES.items():
        hi = float("inf")
        for stmt in fn:
            if stmt[0] == "guard_max":
                hi = min(hi, stmt[1])
            elif stmt[0] == "copy":
                if hi > stmt[1]:
                    print(f"  ground truth: entry {name!r} reaches copy with "
                          f"n.hi={hi:.0f} > cap={stmt[1]} -> OVERFLOW REACHABLE")
                    return True
    return False


if __name__ == "__main__":
    print("plausibility-only dismissal (Evident failure mode)\n")
    verdict = plausible_dismissal("overflow at copy(buf, src, n)")
    reachable = reachability_check()
    print(f"\n  filter verdict: {verdict}")
    if reachable and verdict == "DISMISS":
        print("  >>> UNSOUND: dismissed without a witness; the error state is")
        print("      reachable via parse_async. Plausibility is not a discharge.")
