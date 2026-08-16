# GOOD: diff-first kernel patch review. The commit log is untrusted; every
# claimed fix is checked against the diff, and the changed code is probed with
# the triggering input. Modeled on the failure mode reported at LSFMM+BPF 2026
# (lwn.net/Articles/1073583): Sashiko accepted commit-log bug-fix claims at
# face value; and on Starovoitov's countermeasure (lwn.net/Articles/1075067):
# ignore the commit log, look at the patch. Runs with plain python 3.11.


def has_fix_claim(message):
    keywords = ("fix", "prevent", "guard", "validate", "bounds", "overflow",
                "oob", "out-of-bounds")
    return any(k in message.lower() for k in keywords)


def diff_changed_vulnerable_line(before_code, after_code, marker):
    """The diff must add a semantic guard line (marker) that was absent."""
    b = {ln.strip() for ln in before_code.splitlines()}
    a = {ln.strip() for ln in after_code.splitlines()}
    return any(marker in ln for ln in a) and not any(marker in ln for ln in b)


def review_patch(message, before_code, after_code, before_fn, after_fn,
                 probe, marker):
    """Return (verdict, evidence). before_fn/after_fn(n, cap) -> overflows."""
    if not has_fix_claim(message):
        return ("NO_CLAIM",
                "message claims no fix; review the diff on its own merits")
    if after_code.strip() == before_code.strip():
        return ("REFUSE", "diff is a no-op despite the fix claim")
    if not diff_changed_vulnerable_line(before_code, after_code, marker):
        return ("REFUSE",
                "claimed fix does not touch the vulnerable decision line")
    overflow_before = before_fn(*probe)
    overflow_after = after_fn(*probe)
    if overflow_after and not overflow_before:
        return ("REFUSE",
                f"simulation: change made the defect WORSE on probe {probe}")
    if not overflow_after and overflow_before:
        return ("ACCEPT",
                f"diff evidence: '{marker}' added; probe {probe} now safe "
                f"(was unsafe)")
    return ("REFUSE",
            f"simulation: probe {probe} unchanged by the patch "
            f"(before={overflow_before}, after={overflow_after})")


def vulnerable_before(n, cap):
    return True                      # no check: unbounded copy always overflows


def fixed_after(n, cap):
    return n > cap                   # real fix: copy only within capacity


if __name__ == "__main__":
    print("diff-first review (commit log untrusted)\n")

    msg_fix = "parse_name: fix out-of-bounds write (CVE-2026-XXXX)"
    code_fixed_before = "void parse_name(char*d,size_t c,const char*s,size_t n){ memcpy(d,s,n); }"
    code_fixed_after = ("void parse_name(char*d,size_t c,const char*s,size_t n)"
                        "{ if (n > c) return; memcpy(d,s,n); }")

    code_rename_before = ("void parse_name(char*d,size_t c,const char*s,size_t n)"
                          "{ size_t n_copy=n; memcpy(d,s,n); }")
    code_rename_after = ("void parse_name(char*d,size_t c,const char*s,size_t n)"
                         "{ size_t how_many=n; memcpy(d,s,n); }")

    msg_refactor = "parse_name: rename local for clarity"

    cases = [
        ("A real fix + honest message", msg_fix, code_fixed_before,
         code_fixed_after, vulnerable_before, fixed_after),
        ("No-op diff + fix claim (Sashiko failure)", msg_fix, code_rename_before,
         code_rename_after, vulnerable_before, fixed_after),
        ("Refactor, no fix claim", msg_refactor, code_rename_before,
         code_rename_after, vulnerable_before, fixed_after),
    ]

    for name, msg, before, after, bfn, afn in cases:
        verdict, evidence = review_patch(msg, before, after, bfn, afn,
                                         probe=(32, 32), marker="n > c")
        print(f"  {name:<38} {verdict}")
        print(f"    {evidence}")

    # the real fix must be accepted; the no-op-diff "fix" must be refused
    v1, _ = review_patch(msg_fix, code_fixed_before, code_fixed_after,
                         vulnerable_before, fixed_after, (32, 32), "n > c")
    v2, _ = review_patch(msg_fix, code_rename_before, code_rename_after,
                         vulnerable_before, fixed_after, (32, 32), "n > c")
    assert v1 == "ACCEPT", v1
    assert v2 == "REFUSE", v2
    print("\nRESULT: real fix accepted on diff evidence; no-op-diff 'fix'")
    print("refused because the diff never touches the vulnerable line.")
