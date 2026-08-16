# Evaluation — kernel-patch-review-commit-log-independence

Skill: `skills/kernel/kernel-patch-review-commit-log-independence`.
Stability: `researched` (source-backed grounding: lwn-1073583, lwn-1075067 —
both LWN articles fetched and verified 2026-08-17). The Python simulations and
the gcc C-sanity compiles were actually executed on this host; real output is
recorded below. No Linux kernel tree or git-review pipeline is needed for the
core claim (the diff is the object of review).

## Synthetic evals

| Case | Fixture | Expected | Status |
|---|---|---|---|
| negative (no-op diff) | `bad/bad_plausible_commit_log.c` | commit log claims a bounds fix; diff only renames — must be REFUSED | review-time flag; C compiles (exit 0) |
| negative (message-only review) | `bad/bad_commit_log_review.py` | message-credulous reviewer ACCEPTS a no-op patch — the bias must be flagged | RUN on host |
| positive (real fix) | `good/good_review_fixture.c` | bound check added; diff-first review ACCEPTS | RUN (compile + review sim) |
| positive (protocol) | `good/good_diff_first_review.py` | fix ACCEPT on diff evidence; no-op REFUSED; refactor -> NO_CLAIM | RUN on host |

## False-positive evals (correct code must NOT be flagged)

- `good/good_review_fixture.c` — genuine bounds check `if (n > dst_cap) return;`
  before the copy; must be ACCEPTED with diff evidence.
- A refactor with no fix claim must be marked NO_CLAIM and reviewed on its own
  merits, not refused for lacking a fix.
- A genuine fix that also touches an unrelated line must NOT be refused: the
  required property is that the vulnerable line changed, not that the diff is
  minimal.

## Historical evals

- Sashiko commit-log bias (lwn-1073583, LSFMM+BPF 2026): the LLM reviewer
  "can also be biased by the commit log"; a patch set whose log said it fixed
  real bugs was accepted at face value. This skill's failure mode is that
  incident.
- Maintainer countermeasure (lwn-1075067, LSFMM+BPF 2026): Starovoitov
  ignores commit logs "on the assumption that it is written by LLMs" and
  reads the patches directly, because the subsystem is "swamped with
  low-quality patches that look, on first impression, really good".
- Sashiko calibration data from the same session: ~10% false-positive rate
  over 1500 email threads, ~85% true positives, ~97% on critical/high
  severity — context for why review verdicts need diff evidence, not prose.

## Adversarial evals

- A commit message that claims a fix and whose diff changes only a comment or
  a variable rename must be caught (the `bad_plausible_commit_log.c` fixture).
- A message with no fix claim but a real security fix hidden in the diff must
  still be reviewed and the fix recognized (NO_CLAIM does not mean SKIP).
- An off-by-one "guard" (`n < cap` instead of `n > cap`) must be caught by
  the boundary probe (`n == cap`), not accepted because a check "exists".
- A fix that adds an early return but skips resource cleanup must be flagged
  as introducing a new bug (rule 4 in `references/diff-first-review.md`).

## Verification commands

```
python examples/good/good_diff_first_review.py
python examples/bad/bad_commit_log_review.py
gcc -Wall -Wextra -Werror -O2 -c examples/good/good_review_fixture.c
gcc -Wall -Wextra -Werror -O2 -c examples/bad/bad_plausible_commit_log.c
```

Target (Linux kernel tree; documented-as-target, not executed here):
`scripts/checkpatch.pl` and `git show` the patch; grep the vulnerable
expression in pre/post image to confirm the guard dominates the copy.

## Verified facts

| Fact | Status | Evidence |
|---|---|---|
| python 3.11.9 runs `good_diff_first_review.py`: real fix ACCEPT, no-op REFUSE, refactor NO_CLAIM | VERIFIED (executed 2026-08-17) | output below |
| `bad_commit_log_review.py` accepts a no-op-diff patch on the message alone | VERIFIED (executed) | output below |
| both C fixtures compile with `gcc -Wall -Wextra -Werror -O2 -c`, exit 0 | VERIFIED (executed) | exit codes below |
| Sashiko is "biased by the commit log" and accepted log claims at face value | KNOWN (article fetched) | lwn-1073583 |
| Starovoitov ignores commit logs as LLM-written and reviews patches directly | KNOWN (article fetched) | lwn-1075067 |
| BPF subsystem flooded with plausible-looking low-quality patches | KNOWN (article fetched) | lwn-1075067 |
| Sashiko FP ~10% / TP ~85% / 97% critical-high severity (1500 threads) | KNOWN (article fetched) | lwn-1073583 |
| gcc version on host | VERIFIED | gcc 16.1.0 (MSYS2 ucrt64) |

### Host run (executed 2026-08-17)

`python examples/good/good_diff_first_review.py`:

```
diff-first review (commit log untrusted)

  A real fix + honest message            ACCEPT
    diff evidence: 'n > c' added; probe (32, 32) now safe (was unsafe)
  No-op diff + fix claim (Sashiko failure) REFUSE
    claimed fix does not touch the vulnerable decision line
  Refactor, no fix claim                 NO_CLAIM
    message claims no fix; review the diff on its own merits

RESULT: real fix accepted on diff evidence; no-op-diff 'fix'
refused because the diff never touches the vulnerable line.
```

`python examples/bad/bad_commit_log_review.py`:

```
message: 'parse_name: fix out-of-bounds write (CVE-2026-XXXX)'
  diff actually fixes anything? True
  verdict (message-only) ............ ACCEPT
message: 'net: prevent double-free on error path'
  diff actually fixes anything? True
  verdict (message-only) ............ ACCEPT
message: 'buffer overflow in parse_name now guarded (bounds check added)'
  diff actually fixes anything? False
  verdict (message-only) ............ ACCEPT
  >>> WRONG: accepted a no-op patch purely on the message
```

`gcc -Wall -Wextra -Werror -O2 -c` -> exit 0 for both fixtures
(`good_review_fixture.c`, `bad_plausible_commit_log.c`). A compile is NOT a
review: the bad fixture compiles cleanly while still overflowing — the defect
is only visible to the diff-first process.

## Scoring (for routing eval)

- recall: no-op-diff "fix" and message-credulous reviewer detected.
- precision: real fix accepted; refactor without a claim not refused.
- FP-rate: no false flags on the good fixture or the NO_CLAIM path.
