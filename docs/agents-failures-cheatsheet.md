# AI-agent failures in low-level code — quick reference

A compact catalog of the failure classes behind this repository's 193 skills. Full
source-traced detail: `research/` surveys and `registry/claims.yaml`.

## 1. Assembly & instruction-level

| Failure | Example | Fix |
|---|---|---|
| Invented mnemonics | `movqad` — no such instruction | assemble → disassemble → compare bytes |
| AT&T/Intel operand inversion | `movl src, dst` vs `mov dst, src` | pin the dialect, verify with `objdump` |
| Truncated immediates | `imul eax, eax, 38` encodes `69 c0 00 00 00 00` | compare encoded bytes against intent |
| Missing size hints | `inc [counter]` ambiguous | explicit `qword`/`dword`/`byte` |
| Register-class errors | Thumb-2 `cbz r8, ...` invalid (r0-r7 only) | check the ISA encoding rules |
| Byte-blindness | "AX is 8-bit", `[esp+4]` vs `[esp]` | read the disassembly, not the guess |

Calibration: LLM disassembly exact match ~14%; decompiler fixes correct ~37%;
ideal decompilation ~7% (SCDBench); capability cliff at ~200 instructions.

## 2. Concurrency

| Failure | Example | Fix |
|---|---|---|
| Fake parallelism | thread-safe primitives, one thread, wall 1.2s vs 0.3s at 4 threads | count live threads + wall-clock scaling |
| Deadlocks invisible to linear tests | CONCUR 43 tasks + 72 mutants | stress + lock-order analysis |
| zsh `jobs`-in-substitution bypass | 86 processes → kernel panic (codex#37653) | respect the platform's process limits |

## 3. Rust

| Failure | Example | Fix |
|---|---|---|
| API drift (behavioral changes) | 38% solved vs 65.8% for stabilizations (RustEvo²) | pin toolchain, check signatures, `#[deprecated]` |
| Crate hallucination / typosquat | nonexistent crates resembling real ones | `cargo search` + name-distance check |
| Crypto misuse | 23.3% compile, 57% of those vulnerable; nonce reuse | audited crates + known-answer vectors |
| Fabricated SAFETY comments | "caller guarantees…" with no lifetime/PhantomData | verify the invariant is encoded in types |

## 4. Verification illusions

| Failure | Example | Fix |
|---|---|---|
| Harness that can't fail | unconditional pass masks the bug | ablation: harness must fail when target breaks |
| Fixed-shape oracle | allclose certifies buggy GPU kernels (9/9 caught by fuzz+fp64) | fuzz unseen shapes + fp64 reference |
| "It compiles" = correct | compiles at -O0, breaks at -O2 | build at multiple -O levels + sanitizers |
| Compile@k ≠ pass@k | decompiled code compiles but behaves differently | re-executability tests |

## 5. Systems & memory

| Failure | Example | Fix |
|---|---|---|
| VM-level leaks invisible to heap tools | Ghostty page pool: mmap reused, no munmap → 37-130 GB | track RSS + `/proc/<pid>/maps`, not just malloc |
| UB assumptions | NULL-deref symptom hiding an earlier OOB write | reproduce → minimize → find the corruptor |
| Time-timing side channels | early-exit memcmp leaks length (CWE-1254) | constant-time compare + dudect/ctgrind |

## 6. Safety standards & determinism

| Failure | Example | Fix |
|---|---|---|
| MISRA non-compliance at scale | LLMs emit 23-29 violations/KLOC; no model fully compliant (arXiv 2506.23535) | Top-k rule packs cut violations 83% (RS-8123173) + static analyzer gate |
| Non-boolean control expressions | `if (ptr)` in MISRA C:2012 code (Rule 14.4) | essential-type model; rule 10.x casts |
| Hard-RT violations | malloc in a task, recursion, exceptions → WCET unprovable | static allocation, bounded loops, WCET analyzer |
| Runtime violations of a "deterministic" loop | priority inversion; deadline misses | RMS/EDF schedulability analysis, priority inheritance |

## 7. Agent-integrity failures (meta)

| Failure | Example | Fix |
|---|---|---|
| Fabricated evidence | "tests passed" without raw output; invented terminal logs | demand raw command output; re-run; verify provenance |
| Fake git history | claiming a commit that doesn't exist | `git cat-file -e` / `git fsck`, not narrative |
| Destructive refactoring | thousands of LOC deleted and replaced with broken code | diff-before/after, LOC accounting, compile+test before delete |
| Warning dismissal | verifier warning rejected as "false positive" without witness | require a reachability witness (llm-verifier-warning-disposition) |

## 8. Modern low-level stacks

| Failure | Example | Fix |
|---|---|---|
| Compiler-dependent behavior | same source diverges at -O0 vs -O2 (signed overflow) | differential testing across compilers and -O levels (UBfuzz/DiffSpec) |
| FP semantics | `0.1+0.2 == 0.3` false; `-ffast-math` breaks NaN/errno; x87 excess precision | IEEE 754 rules; avoid `==`; audit `-ffast-math` |
| Endianness/alignment | struct `fwrite` on the wire; union punning (UB); unaligned casts | shift-based serialization + `memcpy` |
| Post-quantum crypto misuse | early-exit decapsulation (CVE oracle); non-constant-time rejection sampling | FIPS 203 implicit rejection; fixed-iteration sampling; ACVP vectors |
| Hardening claims unverified | "-fstack-protector" on the command line, no canary in the binary | verify with readelf/objdump/checksec, never the flag alone |
| io_uring ring misuse | SQ tail without release store; buffer reused before CQE | ring protocol discipline; liburing verification |

## How this repository fixes them

193 skills, each with: when to use / what the agent gets wrong / how to reason
correctly / what+how to verify / source. 93 skills executed on real toolchains;
every claim traces to a primary source. One command: `python tools/validate.py`.
