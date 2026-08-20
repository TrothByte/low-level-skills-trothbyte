# Refactoring Guard — reference

Deep reference for `destructive-refactoring-guard`. The SKILL.md is the
operational summary; this file carries the reasoning and the tooling
details.

## The documented failure class

Coding agents, asked to fix a bug or refactor a module, frequently delete
entire files or thousands of lines of working functionality and replace
them with a fresh rewrite. The rewritten version often:

- does not compile (the agent reported success without compiling);
- compiles but changes behavior (drops edge cases);
- is smaller and "cleaner" but silently deletes the accumulated value of
  the original: error handling, range checks, tests embedded as comments,
  configuration surface, performance tuning, ABI constraints.

The damage is silent because the diff is big-bang and the pre-change
baseline was never recorded. Nothing demonstrates the deletion was safe;
nothing proves the rewrite is equivalent.

The failure is documented as an AI-agent behavior class (SWE-bench error
analysis; repeated community reports of LLM agents deleting files on
failed refactors). KNOWN (documented class), primary source grounding in
SKILL.md "Where the knowledge comes from".

## Diff-first review

Review the diff, not the final state. A rewritten module can look fine in
isolation and still have destroyed behavior the reviewer never sees. The
unit of review is the change:

1. Get the diff between old and new at file granularity.
2. Read it zero-context (`git diff -U0`) so nothing is hidden in context.
3. Ask: does the diff fix the reported problem, or replace the file?
4. Every removed line that is not directly connected to the fix must be
   justified or restored.

## LOC accounting

`git diff --numstat` prints `added<TAB>deleted<TAB>path` per file. It is
the canonical count. The diff-guard tool
(`examples/tools/refactor_diff_guard.py`) reproduces the same per-file
added/deleted counts from a unified diff or two snapshots, and adds two
checks:

- `deleted - added >= threshold` (default 50): deletion-dominant change;
- `deleted > 50%` of the original file: more than half the file is gone.

Use `git diff --stat` for a quick human read, `--numstat` for the numbers.
For a real guard in CI, `git diff --numstat` piped to an awk check is the
canonical primitive; the python tool exists so the same check runs without
git (e.g., on a raw PR diff).

Heuristic used by this skill: deleting more than ~20% of a file, or a net
negative delta beyond 50 lines, turns "refactor" into "rewrite", which is
a decision that needs a named defect or requirement.

## Deleted-symbol proof

For every function, type, macro or export a change deletes, prove no
remaining reference exists:

- whole-tree grep: `grep -rn "symbol" --include=*.c --include=*.h .`
  (adjust for the language and build layout);
- build-time proof: the full project must still compile and link;
- hidden reference classes to check explicitly:
  - reflection (name strings in tables/registries);
  - weak symbols and aliases (`__attribute__((weak))`, linker
    `--undefined`, `.symver`);
  - macro invocation (the symbol is text in a macro expansion);
  - FFI/exports (shared-library exports, JNI, COM vtable, wasm exports);
  - generated code (the symbol appears in a build step, not in source);
  - configuration files that name the symbol (XML/JSON/yaml registries).

If the symbol is only "not referenced in the files I looked at", it is not
proven dead. The build linker (`build-linker-error-diagnostics`) is the
cheapest whole-project oracle: deleted symbol -> undefined reference.

## Baseline discipline

Never delete before you know what "working" means for the old code:

1. Run the pre-existing test suite / build on the OLD code. Record the
   result (pass/fail, which tests).
2. Only then make the change.
3. Run the identical suite on the NEW code.
4. New behavior must equal old behavior everywhere except the named defect.

A change without a recorded baseline cannot claim "same behavior" — it can
only claim "looks similar". The failure mode "reports success on a compile
that never happened" is exactly a missing baseline plus a missing gate.

## Compile-before-delete gates

Two gates, both executed, both recorded:

- Before: full build + test suite on the old tree (baseline).
- After: full build + test suite on the new tree.

Gates must be the same commands and the same toolchain. A build that
"passes" on a different compiler version, or on a stale object, or that
skipped the deleted module, is not the same build — see
`build-toolchain-version-drift`. The deleted module must be exercised: if
the build system dropped it from the target, deletion "compiles fine"
while the code is already gone.

## Reversibility and commit discipline

- One commit per decision: delete, then rewrite, then feature work.
- Never mix deletion with unrelated feature changes — a rollback of the
  feature must not resurrect the deletion.
- A big-bang diff that mixes rewrite and feature is unreviewable and
  unrevertable per-function.
- If the rewrite must be abandoned, the original must be recoverable by
  reverting a single small commit.

## When a rewrite IS justified

The guard flags files; it does not block them. A justified large deletion
passes review when it can produce:

- a reproducible defect or requirement that motivated the deletion (a
  failing test on the old code, a spec change);
- a recorded baseline (old test suite run);
- the deleted surface is behaviorally covered by the replacement
  (same-or-better test suite on the new code);
- deleted symbols proven reference-free, including the hidden classes
  above;
- the rewrite is separated from unrelated work.

Example: replacing a hand-rolled parser with a standards library parser
that has its own comprehensive test suite, with the old code's edge-case
tests ported over, can justify deleting thousands of lines. That is a
decision with evidence; "the old code was bad" is not.

## Guard tool semantics

`examples/tools/refactor_diff_guard.py` parses unified diffs (git-style
`diff --git` or difflib-style `---`/`+++`) and snapshot pairs. Per file it
reports added, deleted, net and lost-percent, and flags on:

- `deleted > added` and `(deleted - added) >= --threshold`;
- `deleted > --max-loss` percent of the original lines.

The `lost%` uses the hunk ranges from the diff (for snapshot mode it is
the exact old file size). The tool mirrors `git diff --numstat`; recorded
outputs are in `evals/README.md`.

## Known / inferred status

- The failure class (agents deleting working code on failed refactors) is
  KNOWN and documented; community reports are anecdotal, SWE-bench
  analysis is published. Marked accordingly in `evals/README.md`.
- The 20% / 50% thresholds are heuristics (INFERRED, tunable per project);
  the guards that must not be relaxed are baseline, numstat accounting,
  deleted-symbol proof and the after-build gate.
- Rewrite-equals-destructive unless justified is the operational default;
  the justified-deletion path above is the escape hatch.
