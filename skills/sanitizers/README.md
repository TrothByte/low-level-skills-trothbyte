# sanitizers — Skills

Sanitizers turn 'probably fine' into evidence. These skills teach the agent CI loop (build→run→parse→dedupe→track) and how to read ASan/UBSan/TSan/MSan reports.

## Skills in this domain

| Skill | What it does | Stability | Path |
|---|---|---|---|
| `fuzzing-harness-evidence-gate` | Use when reporting or reviewing fuzzing results to decide whether a finding is evidence or noise. Enforces the proof standard: reproducible sanitizer report, minimized crashing input, demonstrated reachable path, before any bug or CVE claim. Covers libFuzzer and AFL++ harnesses. | researched | `skills/sanitizers/fuzzing-harness-evidence-gate` |
| `sanitizer-agent-ci-loop` | Use when integrating sanitizers (ASan/UBSan/TSan/MSan) into an agent's build-and-test loop for C/C++/Rust — so every change is automatically checked, reports are parsed and deduplicated, and regressions are caught. Fills the gap where "how to use sanitizers" exists but the universal agent loop does not. | source-backed | `skills/sanitizers/sanitizer-agent-ci-loop` |
| `sanitizer-report-reading` | Use when interpreting sanitizer output — ASan/UBSan/TSan/MSan/LSan reports from builds, CI logs, or fuzzing — to identify bug category, access site versus allocation/free site, root cause, and fix. Triggers on report headers like 'ERROR: AddressSanitizer', shadow bytes, 'data race', or 'use-of-uninitialized-value'. | researched | `skills/sanitizers/sanitizer-report-reading` |

## How to use

- Read the `SKILL.md` of a skill for its triggers, reasoning rules, and verification commands.
- `references/` hold the deep knowledge; `examples/good|bad` are verified compile-and-run fixtures;
  `evals/` define how the skill is tested.
- Load only the skill you need (see `skills/_meta/meta-routing`); references load on demand.

## Related

- [Back to repository root](../../README.md)
