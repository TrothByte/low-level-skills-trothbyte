# _meta — Skills

Meta-skills govern agent behavior: routing, evidence discipline, verification gates,
assumption surfacing, rationalization rejection, harness validity, crypto safety, and honest completion.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `agent-deception-detection` | Use when an agent reports results from tools (test runs, build logs, git history, benchmarks) or when verifying claims made by another agent. Teaches demanding raw artifact evidence instead of summaries, and detecting fabricated evidence, hallucinated terminal logs, and invented git history. | unique | source-backed | `skills/_meta/agent-deception-detection` |
| `agent-scope-management` | Use when an agent session spans many edits, spawns subagents, or loses context. Teaches persisting state to files (progress/WORKLOG), single-responsibility scope boundaries, stopping rules, and why conclusions not written down do not exist. | common | source-backed | `skills/_meta/agent-scope-management` |
| `agent-tool-whitelist` | Use when an agent must decide which shell/tool operations are allowed in a low-level repo or CI environment: maintaining a whitelist of safe, deterministic operations, rejecting ad-hoc dangerous commands, and enforcing read-only-by-default. Teaches the discipline of allowed-operations gates that prevent environment-destructive or non-reproducible commands. | common | source-backed | `skills/_meta/agent-tool-whitelist` |
| `destructive-refactoring-guard` | Use when an agent proposes large refactors or deletions, replacing functions, rewriting modules, or removing dead code, to prevent destroying thousands of lines of working code and replacing them with broken equivalents. Teaches diff-first review, LOC accounting, and compile-before-delete gates. | unique | source-backed | `skills/_meta/destructive-refactoring-guard` |
| `llm-verifier-warning-disposition` | Use when an LLM agent accepts or dismisses bug reports, sanitizer warnings, or static-analysis findings: dismissal requires a reachability argument (witness/trace or solver pass), not plausibility. Teaches the burden-of-proof discipline so agents never self-certify that a reported error state is unreachable. | unique | researched | `skills/_meta/llm-verifier-warning-disposition` |
| `meta-assumptions` | Use when code correctness depends on implicit assumptions — compiler, ABI, platform, memory model, optimization level, or endianness. Forces surfacing and documenting every assumption before concluding. | common | researched | `skills/_meta/meta-assumptions` |
| `meta-claim-extraction` | Use when documenting claims in a skill or SKILL.md/references. Teaches extracting claim → source → section → skill into registry/claims.yaml with confidence levels and KNOWN/INFERRED/UNVERIFIED evidence classification. | common | researched | `skills/_meta/meta-claim-extraction` |
| `meta-completion` | Use before declaring a low-level task complete. Enforces honest completion criteria: verifiable success, no hidden partial results, updated state files, and explicit uncertainty. | common | researched | `skills/_meta/meta-completion` |
| `meta-eval-runner` | Use when running evals for skills: synthetic, false-positive (FP), adversarial, and historical-CVE loops. Teaches the eval loop, scoring, and recording results in evals/README.md and registry/evals.yaml. | common | researched | `skills/_meta/meta-eval-runner` |
| `meta-evidence` | Use whenever making a normative or factual claim about C/C++/Rust/asm/ABI/UB/compiler behavior. Enforces the KNOWN / INFERRED / UNVERIFIED classification and requires source-backed evidence for strong claims. | common | researched | `skills/_meta/meta-evidence` |
| `meta-formal-verification` | Use when deciding whether formal verification is required or empirical testing suffices for a low-level claim. Teaches Kani/CBMC/Frama-C/Z3 selection and sound loop-invariant encoding. | common | researched | `skills/_meta/meta-formal-verification` |
| `meta-rationalizations` | Use during code review or self-review to catch and reject rationalizations that excuse unsafe or incorrect low-level code. Contains the "Rationalizations to Reject" list derived from trailofbits and failure modes B1-B22. | common | researched | `skills/_meta/meta-rationalizations` |
| `meta-routing` | Use at the start of any low-level task to choose the minimal relevant skill set. Prevents "load everything" behavior, enables dependency expansion, and routes to the correct skill from the registry. | common | researched | `skills/_meta/meta-routing` |
| `meta-security-audit` | Use when reviewing low-level code or this repository for security. Teaches license compatibility checks, CVE regression verification, examples/bad scrutiny, and a security-review checklist with evidence. | common | researched | `skills/_meta/meta-security-audit` |
| `meta-token-optimization` | Use when authoring or editing SKILL.md to keep activation cost at or under 2000 tokens. Teaches measuring with tools/tokens/token_measure.py, moving depth to references/, and avoiding duplication across the skill. | common | researched | `skills/_meta/meta-token-optimization` |
| `meta-verification` | Use before concluding that low-level code is correct or that a bug is found. Enforces executable verification (compile+run, sanitizers, asm inspection, debugger) instead of "it compiles" or "tests pass". | common | researched | `skills/_meta/meta-verification` |
| `meta-verification-harness-validity` | Use before trusting a "passing" test harness, eval, or CI gate. Verifies the verification: a harness that cannot fail when its target is broken (unconditional pass, never-executed path, self-test bypass) certifies nothing. Teaches ablation-delta, coverage gates, and --self-test. | improved | source-backed | `skills/_meta/meta-verification-harness-validity` |
| `safe-low-level-from-scratch` | Use when writing NEW low-level code (C/C++/Rust/asm) from scratch that must be memory-safe and correct across optimization levels and platforms. Provides the positive writing process integrating UB semantics, layout/alignment, ownership, atomics, and FFI, with verification gates at each step. | cross-layer | source-backed | `skills/_meta/safe-low-level-from-scratch` |
| `wasm-runtime-from-scratch` | Use when writing, reviewing, or debugging a WebAssembly runtime, interpreter, loader, or validator in C — module binary parsing, validation, linear memory bounds, tables and call_indirect, traps vs undefined behavior, memory.grow, and host function imports. | unique | source-backed | `skills/_meta/wasm-runtime-from-scratch` |
| `zeroize-constant-time` | Use when writing or reviewing code handling secrets (keys, passwords, nonces) that must be zeroized or compared in constant time. Triggers on memset-before-return, secret-dependent branches or indexing, memcmp on secrets, and claims that a secret is cleared. Teaches volatile-sink zeroization, explicit_bzero, ct_memcmp, and asm verification. | improved | source-backed | `skills/_meta/zeroize-constant-time` |

## How to use

- Read the `SKILL.md` of a skill for its triggers, reasoning rules, and verification commands.
  (`references/` hold deep knowledge; `examples/good` and `examples/bad` are compiled/run
  fixtures; `evals/README.md` defines eval cases.)
- Load only the skill you need (see `skills/_meta/meta-routing`; references load on demand.

## Related

[Back to repository root](../../README.md)
