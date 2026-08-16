# abi — Skills

The ABI is the contract between compilers, libraries, and languages.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `abi-layout-reasoning` | Use when writing or reviewing code that crosses a calling convention or binary interface — structs by value, FFI boundaries, inline asm, varargs, or layout-dependent code. Teaches how to compute struct layout and argument passing for SysV AMD64, AAPCS64, RISC-V psABI, and how to verify with the compiler. | cross-layer | source-backed | `skills/abi/abi-layout-reasoning` |

## How to use

- Read the `SKILL.md` of a skill for its triggers, reasoning rules, and verification commands.
  (`references/` hold deep knowledge; `examples/good` and `examples/bad` are compiled/run
  fixtures; `evals/README.md` defines eval cases.)
- Load only the skill you need (see `skills/_meta/meta-routing`; references load on demand.

## Related

[Back to repository root](../../README.md)
