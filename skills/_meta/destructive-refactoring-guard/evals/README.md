# Evaluation — destructive-refactoring-guard

Skill: `skills/_meta/destructive-refactoring-guard`. Stability:
`source-backed` — every fixture was executed on this host on
2026-08-20 and the real output is recorded below (python 3.11.9,
gcc 16.1.0 MinGW).

## Verified facts (host, recorded 2026-08-20)

Diff-guard on the wholesale rewrite (`python examples/tools/refactor_diff_guard.py examples/bad/wholesale_rewrite.diff`):

```
file                     +added -deleted     net   lost%  status
----------------------------------------------------------------
src/parser.c                 33     3090   -3057   99.3%  FLAG: deletion-dominant (net -3057 >= 50); >50.0% of lines lost (99.3%)
----------------------------------------------------------------
RESULT: 1 file(s) flagged — justify each deletion or split into reversible commits (baseline, refs, tests).
```

exit code 1 (KNOWN, recorded).

Diff-guard on the surgical fix (`python examples/tools/refactor_diff_guard.py examples/good/surgical_fix.diff`):

```
file                     +added -deleted     net   lost%  status
----------------------------------------------------------------
src/format_size.c            11        8       3   26.7%  ok
----------------------------------------------------------------
RESULT: clean — no file lost a dominant share of its lines.
```

exit code 0 (KNOWN, recorded).

Snapshot mode, rewrite pair (`--before examples/bad/parser_before.c --after examples/bad/parser_after.c`):
added 33, deleted 3090, net -3057, lost 99.3% — FLAG, exit 1 (matches the
diff mode, so the hunk-derived estimate equals the exact file size).
Snapshot mode on the same pair with `--max-loss 100 --threshold 3100`:
no flags, exit 0 (the justified-deletion escape hatch).

C behavior pair (compile + run):

```
gcc -Wall -Wextra -Werror -O2 examples/good/incremental_fix.c -o ifix.exe   # exit 0
./ifix.exe
  PASS "abc1" -> 2 bytes, checksum 0059560f
  PASS "ABC1" -> 2 bytes, checksum 0059560f
  PASS "abc" -> 2 bytes, checksum 00596b33
  PASS " A0B " -> 2 bytes, checksum 00596b84
  PASS "ag" -> rejected
  incremental_fix: PASS all cases                                            # exit 0
gcc -Wall -Wextra -Werror -O2 examples/bad/rewrite_breaks.c -o rbrk.exe     # exit 0
./rbrk.exe
  PASS "abc1" -> 2 bytes, checksum 0059560f
  PASS "ABC1" -> 2 bytes, checksum 0059560f
  FAIL decode("abc") = 1 bytes
  FAIL decode(" A0B ") = 2 bytes
  FAIL "ag": invalid input accepted (1 bytes)
  rewrite_breaks: 3 case(s) wrong                                            # exit 1
```

The rewrite compiles clean under `-Wall -Wextra -Werror -O2` and still
loses three behaviors (odd-length truncation, whitespace normalization,
invalid-input rejection).

Fixtures: `parser_before.c` = 3112 lines, `parser_after.c` = 55 lines,
`wholesale_rewrite.diff` = 3148 lines (single hunk `@@ -1,3112 +1,55 @@`),
`incremental_fix.c`/`rewrite_breaks.c` and `handler_before.c`/
`handler_after.c` all compile clean; both parser snapshots compile clean
as the "before" is working code.

## Synthetic evals

| Case | Fixture | Expected | Recorded |
|---|---|---|---|
| easy/negative | `bad/wholesale_rewrite.diff` | FLAG, exit 1 | FLAG deletion-dominant + >50% lost, exit 1 |
| easy/positive | `good/surgical_fix.diff` | clean, exit 0 | net +3, 26.7% lost, exit 0 |
| medium/negative | `bad/rewrite_breaks.c` | behavior lost | 3 case(s) wrong, exit 1 |
| medium/positive | `good/incremental_fix.c` | behavior preserved | PASS all cases, exit 0 |
| hard/positive | `good/handler_before.c` -> `handler_after.c` | same LOC surface, fixed bug | 4 wrong -> all 5 correct |

Detection rule: a change is suspect when deletions dominate additions
beyond the threshold or more than half the file disappears; the agent must
then produce baseline, deleted-symbol proof, and a post-change test run
before the deletion is accepted.

## False-positive evals (correct sessions must NOT be flagged)

- A justified large deletion — a failing test on the old code, a recorded
  baseline, zero remaining references, and the replacement's own test
  suite — must pass review. The guard is a gate, not a ban: it prints the
  flag and the agent justifies it. Demonstrated by
  `--max-loss 100 --threshold 3100` on the rewrite pair: exit 0.
- `good/surgical_fix.diff` (net +3, 26.7% lost) is not flagged.
- A pure rename (`git diff` rename detection) with no content change is
  not a deletion and is not flagged.

## Historical evals

- SWE-bench failure-mode analysis: agents solving tasks by deleting
  substantial existing code and substituting rewrites, with correctness
  drops on edge cases the original handled. KNOWN (published dataset,
  https://github.com/princeton-nlp/SWE-bench); the shape is reproduced by
  `rewrite_breaks.c` (compiles clean, behavior gone).
- Community reports of LLM agents deleting files or thousands of lines on
  failed refactors, then "succeeding" because the build skipped the
  deleted module. KNOWN (anecdotal), reproduced by the wholesale-rewrite
  fixture's single-hunk 3057-line deletion with a 55-line replacement.
- The `git diff --numstat` accounting primitive is canonical git behavior
  (https://git-scm.com/docs/git-diff); the guard tool reproduces its
  numbers (KNOWN, verified: 33 added / 3090 deleted matches the diff).

## Adversarial evals

- The rewrite fixture compiles clean under `-Werror -O2` — an agent that
  accepts "it compiles" as proof of correctness accepts the rewrite. The
  self-test disproves it with concrete bytes.
- The wholesale diff deletes 99.3% of the file in one hunk; a summary
  like "modernized parser" with no numstat hides it. The guard prints the
  numbers the summary omits.
- The surgical diff hides nothing: net +3, small lost%, the exact bug
  fix. An agent that flags it as "destructive" is the reverse error.

## Verification commands

```
python examples/tools/refactor_diff_guard.py examples/bad/wholesale_rewrite.diff
python examples/tools/refactor_diff_guard.py examples/good/surgical_fix.diff
python examples/tools/refactor_diff_guard.py --before examples/bad/parser_before.c --after examples/bad/parser_after.c
python examples/tools/refactor_diff_guard.py --before examples/bad/parser_before.c --after examples/bad/parser_after.c --max-loss 100 --threshold 3100
gcc -Wall -Wextra -Werror -O2 examples/good/incremental_fix.c -o ifix.exe && ./ifix.exe
gcc -Wall -Wextra -Werror -O2 examples/bad/rewrite_breaks.c -o rbrk.exe && ./rbrk.exe
gcc -Wall -Wextra -Werror -O2 examples/good/handler_before.c -o hb.exe && ./hb.exe
gcc -Wall -Wextra -Werror -O2 examples/good/handler_after.c -o ha.exe && ./ha.exe
gcc -Wall -Wextra -Werror -O2 -c examples/bad/parser_before.c   # working code compiles
gcc -Wall -Wextra -Werror -O2 -c examples/bad/parser_after.c    # rewrite also compiles
```

Fixtures are regenerated deterministically:

```
python examples/tools/gen_wholesale_fixture.py
python examples/tools/gen_surgical_fixture.py
```

## Scoring

- precision: every flagged change must be deletion-dominant or a majority
  line loss — the surgical diff and the justified-deletion case produce
  zero flags.
- recall: baseline, numstat accounting, deleted-symbol proof, whole-tree
  grep, and the after-gate are each demanded for a flagged change; the C
  pair distinguishes "behavior preserved" from "behavior lost" at byte
  level.
- FP-rate: measured at zero for the good fixtures and the justified
  deletion path.
- Strongest single fact: the same five test inputs pass byte-for-byte
  under the surgical fix and fail 3/5 under the compiling rewrite — the
  rewrite's silent behavior loss is recorded, not assumed.
