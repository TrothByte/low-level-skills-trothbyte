# BAD: commit-log-credulous reviewer. The verdict is derived from the commit
# message ("contains a fix claim") and never from the diff. This reproduces the
# Sashiko failure documented at lwn.net/Articles/1073583 — an LLM reviewer
# accepting commit-log bug-fix claims at face value — and the bias maintainers
# now work around by ignoring logs entirely (lwn.net/Articles/1075067).
# # intentionally incorrect
#
# The correct approach is examples/good/good_diff_first_review.py.


def has_fix_claim(message):
    keywords = ("fix", "prevent", "guard", "validate", "bounds", "overflow",
                "oob", "out-of-bounds")
    return any(k in message.lower() for k in keywords)


PATCHES = [
    # (commit message, does the diff actually change the vulnerable code?)
    ("parse_name: fix out-of-bounds write (CVE-2026-XXXX)", True),
    ("net: prevent double-free on error path", True),
    ("buffer overflow in parse_name now guarded (bounds check added)", False),
    # ^--- the diff is a no-op; only the message mentions a fix
]

for message, diff_changed in PATCHES:
    verdict = "ACCEPT" if has_fix_claim(message) else "NEEDS_REVIEW"
    print(f"message: {message!r}")
    print(f"  diff actually fixes anything? {diff_changed}")
    print(f"  verdict (message-only) ............ {verdict}")
    if verdict == "ACCEPT" and not diff_changed:
        print("  >>> WRONG: accepted a no-op patch purely on the message")

print("\nThe commit-log-credulous reviewer accepted a patch whose diff is a")
print("no-op. A diff-first review refuses it: the changed lines never touch")
print("the vulnerable code (see examples/good/good_diff_first_review.py).")
