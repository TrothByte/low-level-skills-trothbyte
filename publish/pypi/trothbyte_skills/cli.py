"""Command-line entry point for trothbyte-skills."""

import argparse
import os
import shutil
import subprocess
import sys
import tempfile

REPO = "https://github.com/TrothByte/low-level-skills-trothbyte.git"


def install(target_dir: str) -> None:
    tmp = tempfile.mkdtemp(prefix="trothbyte-skills-")
    try:
        print(f"Cloning {REPO} ...")
        subprocess.run(
            ["git", "clone", "--depth", "1", REPO, os.path.join(tmp, "repo")],
            check=True,
        )
        src_skills = os.path.join(tmp, "repo", "skills")
        os.makedirs(target_dir, exist_ok=True)
        count = 0
        for domain in sorted(os.listdir(src_skills)):
            d_path = os.path.join(src_skills, domain)
            if not os.path.isdir(d_path):
                continue
            for skill in sorted(os.listdir(d_path)):
                s_path = os.path.join(d_path, skill)
                if not os.path.isdir(s_path):
                    continue
                if not os.path.exists(os.path.join(s_path, "SKILL.md")):
                    continue
                shutil.copytree(
                    s_path,
                    os.path.join(target_dir, domain, skill),
                    dirs_exist_ok=True,
                )
                count += 1
        print(f"Installed {count} skills into {target_dir}")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(
        prog="trothbyte-skills",
        description="Install the TrothByte low-level skills collection into an agent skills directory.",
    )
    sub = parser.add_subparsers(dest="command")
    install_p = sub.add_parser("install", help="copy skills into a directory")
    install_p.add_argument(
        "target_dir",
        nargs="?",
        default=os.path.join(os.path.expanduser("~"), ".claude", "skills"),
        help="destination directory (default: ~/.claude/skills)",
    )
    args = parser.parse_args(argv)
    if args.command is None:
        parser.print_help()
        return 1
    if args.command == "install":
        install(args.target_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
