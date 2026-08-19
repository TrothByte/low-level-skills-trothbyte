# Pre-flight analysis report (TrothByte v3.0)

Date: 2026-08-19. Lead-agent, Phase 0.

## Current state (what already exists)

1. **163 skills** registered & implemented (`registry/skills.yaml`, 1339 lines): 85 source-backed,
   78 researched, 34 domains, 228 sources, 141 claims, ~233 cross-links. All have SKILL.md.
2. **SKILL.md standard** (9 sections, enforced by `tools/lint/skill_lint.py`): frontmatter
   name+description, When to use, When not to use, What the agent often gets wrong, How to reason
   correctly, What to verify, How to verify, Where the knowledge comes from, Related skills, Evaluation.
3. **Validators** (all implemented): `tools/validate.py` orchestrates skill_lint, token gate,
   registry_check, source_check, claim_extractor, prose_lint. CI: `.github/workflows/ci.yml`
   (single validate job on ubuntu-latest) + stale.yml.
4. **Token tooling**: `tools/tokens/token_measure.py --check 2000` exists; AGENTS.md currently
   ~477 words (~700-900 tokens), already below 2000 but not cache-restructured.
5. **Registry files**: skills.yaml, sources.yaml, claims.yaml, cross-links.yaml, evals.yaml, tools.yaml
   (tools.yaml: 27 tools, 6 implemented, 21 registered — incl. `eval-runner` @ tools/eval/eval_runner.py
   and `sanitizer-run` @ tools/eval/sanitizer_run.sh as *registered-but-missing*).
6. **Meta skills** live in `skills/_meta/` (NOT `skills/meta/` as the task prompt states):
   meta-routing, agent-tool-whitelist exist and are enhanceable; meta-compaction,
   meta-self-consistency, meta-silent-failure-detection do NOT exist.
7. **Evals**: 157/163 skills have `evals/README.md`. Missing: the 6 original PHASE-10 meta-skills
   (meta-assumptions, meta-completion, meta-evidence, meta-rationalizations, meta-routing,
   meta-verification). Relevant for Phase B3 "evals/README.md exists for ALL skills".
8. **Toolchain on this host (win32)**: Python 3.11.9, gcc 16.1.0 (MSYS2), rustc/cargo 1.97.1,
   gdb (MSYS2). MISSING: clang, semgrep, docker, nasm, qemu. Semgrep/CodeQL/Docker jobs are
   CI-only (ubuntu). Host cannot run semgrep or docker locally.
9. **Branches**: main (working), remote gh-pages exists (landing at
   trothbyte.github.io/low-level-skills-trothbyte). docs/ has index.html (JS landing) + index.md (Jekyll).

## What is missing (per v3.0 task)

- `registry/skills.min.yaml` (token/trigger index) + generator script.
- `registry/triggers.yaml` (reverse keyword index).
- Cache-friendly AGENTS.md rewrite (<2000 tokens, static-first).
- `.agentignore` (token economy ignore list).
- `tools/eval_runner.py` (semantic + security gate) and `tools/sanitizer_run.sh` (ASan/UBSan/TSan/Miri).
- `tools/tool_schema.yaml` (machine-readable schemas).
- `tools/restructure_skills.py` (SKILL.md section reorder + CRITICAL REMINDER).
- CI matrix (ubuntu+windows), docker/Dockerfile.ubuntu-eval, Makefile.
- 3 new meta-skills (meta-compaction, meta-self-consistency, meta-silent-failure-detection).
- README/docs/CHANGELOG v3.0 updates; landing search over skills.min.yaml.

## Discrepancies (task prompt vs repo reality)

- **D1**: prompt path `skills/meta/...` — real dir is `skills/_meta/`. Using real paths.
- **Rule 5 vs Phases D2/D3/E3**: rule says "no new skills, work only with existing 163", but
  phases D2/D3/E3 explicitly create 3 new meta-skills. ASKED the user (lead decision pending).
- **B3**: "evals/README.md exists for ALL skills" would fail on 6 meta-skills — plan: create
  minimal evals/README.md for those 6 (does not modify SKILL.md).
- **B1/B2 host limits**: semgrep/docker unavailable on win32 host — security job is CI-only
  (ubuntu); eval_runner must degrade gracefully (SKIP if toolchain absent, marked in report).

## Token budget check

- AGENTS.md: 477 words ≈ 700-900 tokens (target < 2000) — PASS already; restructure still needed.
- SKILL.md activation: gate 2000 (validate.py Level 1b); worst skill historically 1689 tokens.
