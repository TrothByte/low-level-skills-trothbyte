---
name: destructive-refactoring-guard
description: Use when an agent proposes large refactors or deletions, replacing functions, rewriting modules, or removing dead code, to prevent destroying thousands of lines of working code and replacing them with broken equivalents. Teaches diff-first review, LOC accounting, and compile-before-delete gates.
---

# Destructive Refactoring Guard

## When to use

- An agent proposes replacing, deleting, or rewriting a substantial chunk
  of an existing, working file (a "big-bang" diff).
- A bug fix is being implemented as a rewrite: the module is deleted and a
  fresh version written instead of changing the failing lines.
- "Dead code" removal is proposed and the symbol's callers were not
  mapped first.
- The change deletes more than ~20% of a file's lines, or mixes deletion
  with unrelated feature work in one commit.
- The motivation is an aesthetic claim ("the old code was bad") with no
  failing test or requirement attached.

## When not to use

- Greenfield files with no history, no callers, and no tests.
- A proven-dead file: grep shows zero references across the whole tree and
  the full build and tests pass with it deleted.
- Mechanical, behavior-preserving renames and moves (git tracks these).
- Small surgical fixes that change under ~20% of a file and are pinned by
  a failing test.

## What the agent often gets wrong

- Wholesale rewrite instead of a surgical fix: a big-bang diff that
  replaces the module rather than changing the failing lines.
- Deleting code whose callers/behavior it never mapped — symbols that look
  unused but are reached through reflection, weak symbols, macro
  invocation, FFI exports, or generated code.
- "The old code was bad" asserted without evidence: no failing test or
  requirement the new code must satisfy.
- Reporting success on a compile that never happened, or on a build that
  skipped the deleted module (stale objects, wrong target).
- No rollback story: a single giant commit mixing deletion and rewrite, so
  the deleted code cannot be recovered per decision.
- No baseline: the pre-existing test suite was never run against the OLD
  code, so there is nothing to compare the new result against.

## How to reason correctly

1. Baseline first: run the existing build and test suite on the OLD code
   and record the result before touching anything.
2. Diff before/after at file granularity and measure LOC: added, deleted,
   net (`git diff --numstat`).
3. A rewrite is a decision, not a default. Deleting more than ~20% of a
   file requires a concrete, reproducible defect or requirement; name it.
4. Keep deletions reversible: separate commits for delete vs rewrite;
   never mix deletion with unrelated feature work.
5. Verify compilation of the whole project, not just the edited file, and
   run the test suite after — the same gates that ran before.
6. For every deleted symbol, prove no remaining reference exists: grep the
   whole tree, and check weak symbols, reflection, macros, and FFI.

## What to verify

- Added/deleted/net LOC per file (`git diff --numstat`) and the total
  across the change.
- Every deleted function, type, macro, or export has no remaining
  references.
- The old-code baseline result is recorded before the change.
- Full build plus test suite pass after; the identical gates ran before.

## How to verify

Run the diff-guard on the proposed diff (host, python 3.11):

```
python examples/tools/refactor_diff_guard.py examples/bad/wholesale_rewrite.diff
# expect: per-file FLAG lines and exit 1 (deletion-dominant, >50% lost)
python examples/tools/refactor_diff_guard.py examples/good/surgical_fix.diff
# expect: clean per-file report and exit 0
```

Snapshot mode for a before/after pair:

```
python examples/tools/refactor_diff_guard.py --before a.c --after b.c
```

C examples prove behavior preservation with a real compiler (gcc 16.1):

```
gcc -Wall -Wextra -Werror -O2 examples/good/incremental_fix.c -o ifix.exe
./ifix.exe                  # expect: PASS all cases, exit 0
gcc -Wall -Wextra -Werror -O2 examples/bad/rewrite_breaks.c -o rbrk.exe
./rbrk.exe                  # expect: FAIL odd-length + invalid input, exit 1
```

Real repo commands:

```
git diff --numstat          # LOC accounting per file
git diff -U0                # zero-context diff; nothing hidden
grep -rn deleted_symbol --include=*.c --include=*.h .   # prove no refs
```

## Where the knowledge comes from

- Documented AI-agent failure class — wholesale rewrites deleting working
  code (SWE-bench, https://github.com/princeton-nlp/SWE-bench; community
  reports of LLM agents deleting files on failed refactors)
- git documentation — diff and numstat semantics
  (https://git-scm.com/docs/git-diff, https://git-scm.com/docs/gitdiffcore)
- Claude Code best practices — surgical edits over rewrites
  (https://www.anthropic.com/engineering/claude-code-best-practices)
- Google engineering practices — review the diff, not the final state
  (https://google.github.io/eng-practices/review/reviewer/looking-for/)
- Martin Fowler — refactoring in small steps, keeping the system green
  after each step (https://refactoring.com/)

## Related skills

- `agent-scope-management` (recommend) — a destructive refactor is scope
  creep; the unit of work is the fix, not the rewrite
- `meta-verification` (recommend) — "it compiles" must be an executed
  command with recorded output, not a claim
- `meta-completion` (recommend) — a rewrite is not complete until the
  baseline and post-change gates both have records
- `meta-evidence` (recommend) — the failing test is the only evidence that
  justifies a deletion
- `build-toolchain-version-drift` (recommend) — a build that "passes" on a
  different compiler version is not the same build
- `debugging-instrumentation-over-reasoning` (recommend) — instrument old
  and new code and compare, instead of reasoning about which is "better"
- `build-linker-error-diagnostics` (recommend) — undefined references from
  deleted symbols are how the destruction announces itself

## Evaluation

- Synthetic: `bad/wholesale_rewrite.diff` must be flagged (exit 1);
  `good/surgical_fix.diff` must be clean (exit 0); `rewrite_breaks.c`
  must fail its self-test while `incremental_fix.c` passes.
- False-positive: a large deletion backed by a failing test, a recorded
  baseline, and zero remaining references must NOT be flagged (the guard
  asks for justification; it does not forbid deletion).
- Historical: SWE-bench failure-mode analysis and community reports of LLM
  agents deleting whole files on failed refactors.
- Adversarial: a rewrite that "looks cleaner" but silently drops an
  odd-length edge case — reproduced by `rewrite_breaks.c`.
- Commands recorded on this host (python 3.11.9, gcc 16.1.0):
  `evals/README.md`.
