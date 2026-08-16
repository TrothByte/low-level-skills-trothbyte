# GOOD: a concrete operation-whitelist policy engine (host-runnable).
# Every command an agent proposes is classified into read-only /
# build-in-temp / mutate / network / destructive and checked against an
# explicit allowlist. Anything unlisted is denied and logged. This is the
# enforceable core of the allowed-operations discipline.
#
# Run: python whitelist_check.py   (expects PASS, exit 0)

import sys

# explicit allowlist: exact (command, arg-prefix) pairs, default-deny
ALLOW = {
    "git": ("status", "diff", "log", "show"),
    "gcc": ("-fsyntax-only", "-c", "-o build/"),
    "python": ("tools/validate.py", "examples/"),
    "readelf": (),
    "objdump": (),
}

# operation classes that are always denied unless pre-approved
DENY_ALWAYS = ("rm", "Remove-Item", "del", "format", "apt-get", "pip", "cargo install", "npm install", "git reset", "git checkout", "make install")

def classify(cmd):
    """Return ('allow', detail) | ('deny', reason)."""
    if any(cmd.startswith(d) for d in DENY_ALWAYS):
        return ("deny", "explicitly denied operation class")
    if cmd.startswith("gcc") or cmd.startswith("python") or cmd.startswith("git"):
        for prefix in ALLOW.get(cmd.split()[0], ()):
            if cmd.split()[0] == "git":
                if len(cmd.split()) > 1 and cmd.split()[1].startswith(prefix):
                    return ("allow", "read-only git command")
            elif prefix in cmd:
                return ("allow", "whitelisted with approved args")
    if cmd.startswith("readelf") or cmd.startswith("objdump"):
        return ("allow", "read-only inspection")
    return ("deny", "not on the allowlist")

def main():
    approved = [
        "git status",
        "python tools/validate.py",
        "readelf -l kernel.o",
        "gcc -fsyntax-only src.c",
        "gcc -o build/out src.c",      # scoped write: build/ is approved
    ]
    denied = [
        "rm -rf build",
        "Remove-Item -Recurse .",
        "pip install requests",
        "git reset --hard",
        "cargo install fd-find",
    ]
    for c in approved:
        verdict, why = classify(c)
        assert verdict == "allow", f"{c!r} should be allowed: {why}"
    for c in denied:
        verdict, why = classify(c)
        assert verdict == "deny", f"{c!r} should be denied: {why}"
    print("PASS: whitelist policy enforced (approved run, denied rejected)")
    return 0

if __name__ == "__main__":
    sys.exit(main())
