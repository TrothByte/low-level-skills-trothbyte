---
name: Bug report
about: Report an incorrect claim, broken example, or failing validator
title: "[Bug] "
labels: bug
assignees: ""
---

## What is wrong

Describe the bug clearly. This repository ships knowledge and examples — a bug can be:
an incorrect normative claim, an example that does not compile/run as documented, or a
validator failure.

## Skill or file affected

Path to the skill or file (e.g. `skills/assembly/asm-x86-64-registers-and-addressing/SKILL.md`).

## Expected vs actual

- Expected:
- Actual:

## Reproduction

The exact commands to reproduce (compiler/toolchain versions matter — we verify with
GCC 16.1, rustc 1.97, GDB, objdump, Python 3.11).

## Primary source

If this is a factual/claim error, cite the primary source that contradicts the claim
(standard, datasheet, CVE, paper). Every normative claim in this repository must trace
to a primary source in `registry/claims.yaml` / `registry/sources.yaml`.
