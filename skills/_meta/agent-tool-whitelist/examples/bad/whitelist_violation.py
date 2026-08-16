# BAD: # intentionally incorrect — a "cleanup" routine that globs then
# deletes without scoping. The destructive command class is denied by the
# whitelist, yet this code runs it on a wildcard that can match the repo
# root. The PowerShell `Copy-Item -Force *` / `Remove-Item -Recurse`
# wildcard-class incident destroyed files exactly this way.
#
# Run: python whitelist_violation.py   (expects non-zero exit)

import sys
import shutil
import os
import tempfile

def cleanup_artifacts(glob_pattern):
    # // intentionally incorrect: no scope check, deletes on a glob
    for entry in glob_pattern:  # wildcard expansion with no bounds
        path = os.path.join(os.getcwd(), entry)
        if os.path.isdir(path):
            shutil.rmtree(path)  # // intentionally incorrect: rmtree on glob
        elif os.path.isfile(path):
            os.remove(path)

def main():
    d = tempfile.mkdtemp()
    with open(os.path.join(d, "keep.txt"), "w") as f:
        f.write("data")
    try:
        # simulates a glob that resolves to the working tree
        cleanup_artifacts(["*"])
        print("BUG: destructive cleanup ran on an unbounded glob")
        return 0  # destructive command "succeeded" — the failure
    except Exception:
        print("BUG: destructive cleanup attempted")
        return 1

if __name__ == "__main__":
    sys.exit(main())
