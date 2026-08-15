# Evaluation — zig-inline-asm-and-abi

Skill: `skills/zig/zig-inline-asm-and-abi`.
Stability target: `researched`. Toolchain: zig is NOT installed on this host; the code
targets the 0.15–0.17 API surface (verified against the langref Assembly section and the
0.15.x release notes). Verification commands below are the recorded plan, not run results.

## Synthetic evals

| Case | Fixture | Expected | Command |
|---|---|---|---|
| easy/negative | `bad/string_clobbers.zig` | fails on 0.15+: typed clobbers required | `zig test` |
| easy/negative | `bad/non_volatile.zig` | compiles; asm deleted at -O2 (flag by review) | `zig test` + objdump |
| medium/negative | `bad/missing_clobber.zig` | compiles; unchecked Illegal Behavior (review) | `zig test` + review |
| hard/negative | review | wrong AT&T operand order or Intel syntax in the asm string | review |
| positive | `good/syscall.zig` | passes; writes "hello world" | `zig test -target x86_64-linux` |
| positive | `good/global_asm.zig` | passes (needs `-fllvm` on master) | `zig test -target x86_64-linux -fllvm` |
| positive | `good/export_extern.zig` | passes; export/C ABI | `zig test` |

## False-positive evals (correct code must not be flagged)

- `good/syscall.zig` — `asm volatile` with typed `.{ .rcx = true, .r11 = true }` clobbers
  is the documented idiom.
- `good/global_asm.zig` — namespace-level `comptime { asm (...) }` with no volatile/
  inputs/outputs is correct global assembly.
- `good/export_extern.zig` — `export fn` and `@export` of a `callconv(.c)` function.
- A deliberately non-volatile `asm ("nop")` whose output is consumed — no, `nop` has no
  output; the correct FP case is asm with a used result and no `volatile`.

## Historical evals

- 0.15.0 typed clobbers: `bad/string_clobbers.zig` reproduces the pre-0.15 pattern that
  no longer compiles.
- 0.15.0 `usingnamespace` removal does not affect asm; but the "Don't Forget To Flush"
  Writergate did change the `syscall.zig` main (`std.Io`), so older tutorials mix eras.
- Calling-convention naming drift (`.x86_64`/`.win64` pre-0.16 vs `.sysv`/`.winapi` on
  0.16+) is an INFERRED version-sensitive claim to verify per pin.

## Adversarial evals

- A syscall wrapper that compiles cleanly but omits the `rcx`/`r11` clobbers: it must be
  recognized as unchecked Illegal Behavior, not an assembler error (mirrors the
  asm-domain "assembles cleanly but faults" class).
- An `@export` of a function lacking `callconv(.c)` that links, but whose ABI is wrong
  for C callers — review-time catch.
- Hand-written asm assuming args in the wrong SysV registers (`rbx`, `rcx`) — register
  role rule from sysv-amd64-abi §3.2.

## Verified facts

- KNOWN (from langref Assembly section and 0.15.1 release notes; not run on this host):
  - Clobber syntax since 0.15.0: `: .{ .rcx = true, .r11 = true }`.
  - Failure to declare the full clobber set is unchecked Illegal Behavior.
  - `volatile` is required to prevent deletion of side-effecting asm.
  - x86/x86-64 inline asm is AT&T syntax.
  - Global assembly = `comptime { asm (...) }`, no volatile/inputs/outputs; langref test
    command uses `-fllvm` on x86_64-linux.
  - SysV AMD64: args rdi rsi rdx rcx r8 r9; return in rax (sysv-amd64-abi §3.2).
- INFERRED: `.sysv`/`.winapi` vs `.x86_64`/`.win64` convention-name mapping across
  0.15/0.16 — check pinned langref.
- UNVERIFIED (needs zig on this host): exact diagnostic for `string_clobbers.zig`;
  runtime symptom of `missing_clobber.zig`; actual test output.

## Target toolchains (absent, documented)

- zig 0.15.2 / 0.16.0 / 0.17.0-dev: not installed. `objdump`/`nm` are documented via
  binutils-docs. First execution plan: install zig, then run the commands in SKILL.md
  §How to verify.
