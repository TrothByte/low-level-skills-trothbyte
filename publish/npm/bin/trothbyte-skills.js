#!/usr/bin/env node
"use strict";

const { execSync } = require("child_process");
const fs = require("fs");
const os = require("os");
const path = require("path");

const REPO = "https://github.com/TrothByte/low-level-skills-trothbyte.git";

function usage() {
  console.log(`trothbyte-skills — install the TrothByte low-level skills collection

Usage:
  trothbyte-skills install [targetDir]

  targetDir   where to copy the skills (default: ~/.claude/skills)
              each skill lands in <targetDir>/<domain>/<skill-id>/SKILL.md
`);
}

function install(targetDir) {
  const tmp = fs.mkdtempSync(path.join(os.tmpdir(), "trothbyte-skills-"));
  try {
    console.log(`Cloning ${REPO} ...`);
    execSync(`git clone --depth 1 ${REPO} ${path.join(tmp, "repo")}`, {
      stdio: "inherit",
    });
    const srcSkills = path.join(tmp, "repo", "skills");
    if (!fs.existsSync(targetDir)) {
      fs.mkdirSync(targetDir, { recursive: true });
    }
    let count = 0;
    for (const domain of fs.readdirSync(srcSkills)) {
      const dPath = path.join(srcSkills, domain);
      if (!fs.statSync(dPath).isDirectory()) continue;
      for (const skill of fs.readdirSync(dPath)) {
        const sPath = path.join(dPath, skill);
        if (!fs.statSync(sPath).isDirectory()) continue;
        if (!fs.existsSync(path.join(sPath, "SKILL.md"))) continue;
        fs.cpSync(sPath, path.join(targetDir, domain, skill), { recursive: true });
        count++;
      }
    }
    console.log(`Installed ${count} skills into ${targetDir}`);
    console.log(`Tip: run "python tools/validate.py" in the cloned repo to re-verify everything.`);
  } finally {
    fs.rmSync(tmp, { recursive: true, force: true });
  }
}

const args = process.argv.slice(2);
if (args.length === 0 || args[0] === "--help" || args[0] === "-h") {
  usage();
  process.exit(args.length === 0 ? 1 : 0);
}
if (args[0] !== "install") {
  console.error(`Unknown command: ${args[0]}`);
  usage();
  process.exit(1);
}
const targetDir = args[1] || path.join(os.homedir(), ".claude", "skills");
install(targetDir);
