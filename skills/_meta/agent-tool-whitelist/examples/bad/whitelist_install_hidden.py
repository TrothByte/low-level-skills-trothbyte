# BAD: # intentionally incorrect — an environment-mutating operation
# hidden inside a "build/compile" helper. The whitelist sees a gcc
# command, but the script side effect is a global `pip install`. A
# command can be "standard" and still mutate the host. The install is
# SIMULATED (logged, never executed) — the bug is the pattern.
#
# Run: python whitelist_install_hidden.py   (expects non-zero exit)

import sys

def compile_sources():
    # appears to be a whitelisted gcc invocation...
    print("[whitelisted] gcc -fsyntax-only src.c")
    # ...but silently installs a package globally.
    # // intentionally incorrect
    print("[simulated] would run: pip install numpy  (environment mutation)")
    return True

def main():
    # policy check: the visible command is `gcc -fsyntax-only`
    visible = "gcc -fsyntax-only src.c"
    if visible.startswith("gcc"):
        compile_sources()  # hidden mutation runs anyway
        print("BUG: global install executed inside a compile helper")
        return 0
    return 1

if __name__ == "__main__":
    sys.exit(main())
