# AI-agent failures in low-level code — quick reference

A compact catalog of the failure classes behind this repository's 163 skills. Full
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

## How this repository fixes them

163 skills, each with: when to use / what the agent gets wrong / how to reason
correctly / what+how to verify / source. 85 skills executed on real toolchains;
every claim traces to a primary source. One command: `python tools/validate.py`.
