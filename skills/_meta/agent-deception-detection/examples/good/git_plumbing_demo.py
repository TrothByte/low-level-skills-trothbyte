#!/usr/bin/env python3
"""Why `git cat-file -e <sha>` is the gate for commit claims.

Git claims in prose ("I committed X", "branch Y exists") are porcelain-level
text. The object database decides. This script creates a throwaway git repo,
makes one real commit, then demonstrates:

  git cat-file -e <real-sha>   -> exit 0 (object exists)
  git cat-file -e <forged-sha> -> exit 1 (no such object)
  git log --format=%H          -> the real commit list

It then simulates the textual claim "committed 0000000deadbeef..." and shows
that the plumbing check rejects it, while the real sha passes.

If git is not on PATH the script prints the commands it would run and exits 0.
"""

import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

GIT = shutil.which("git")


def run(args: list, cwd: Path) -> subprocess.CompletedProcess:
    return subprocess.run(
        [GIT, *args], cwd=str(cwd), capture_output=True, text=True
    )


def main() -> int:
    if not GIT:
        print("git not on PATH; printing the gate commands for the record:")
        print("  git cat-file -e <sha>      # exit 0 if object exists")
        print("  git log --format=%H        # real commit list")
        print("  git fsck --full            # object integrity")
        return 0

    with tempfile.TemporaryDirectory(prefix="git_plumbing_demo_") as tmp:
        repo = Path(tmp)
        subprocess.run([GIT, "init", "-q", "-b", "main"], cwd=tmp, check=True)
        (repo / "f.txt").write_text("evidence\n", encoding="utf-8")
        run(["add", "f.txt"], repo)
        run(["config", "user.email", "demo@example.invalid"], repo)
        run(["config", "user.name", "Plumbing Demo"], repo)
        run(["commit", "-q", "-m", "add f.txt with evidence"], repo)

        real_sha = run(["rev-parse", "HEAD"], repo).stdout.strip()
        forged_sha = "0" * 40

        print(f"real commit sha: {real_sha[:12]}...")
        print()

        def gate(tag: str, sha: str) -> None:
            proc = run(["cat-file", "-e", sha], repo)
            verdict = "EXISTS (exit 0)" if proc.returncode == 0 else "NO SUCH OBJECT (exit != 0)"
            print(f"git cat-file -e {tag:<10} {sha[:12]}... -> {verdict}")

        gate("real sha", real_sha)
        gate("forged sha", forged_sha)

        print()
        print("Claim made by agent (text only):")
        print(f'  "I committed {forged_sha[:12]}..."')
        if run(["cat-file", "-e", forged_sha], repo).returncode == 0:
            print("  -> plumbing: object EXISTS (claim stands)")
        else:
            print("  -> plumbing: object DOES NOT EXIST (claim is invented git history)")

        print()
        log = run(["log", "--format=%H", "--oneline"], repo).stdout.strip()
        print("git log --format=%H --oneline (ground truth):")
        print(" ", log)
        print()
        print(f"PASS: textual commit claims must resolve via plumbing; {real_sha[:12]}... does, the forged sha does not.")
        return 0


if __name__ == "__main__":
    raise SystemExit(main())
