# Evaluation — misra-c-compliance

Skill: `skills/safety/misra-c-compliance`.
Stability: `source-backed` — the scanner and the gcc commands below were
actually executed on this host on 2026-08-20 and the output was recorded.
Toolchain: gcc (Rev5, Built by MSYS2 project) 16.1.0, Python 3.11.9, Windows.

## Verified facts (host, recorded 2026-08-20)

All commands below were executed from the repository root.

### 1. Heuristic scanner on the bad fixtures (expect findings, exit 1)

```
python skills/safety/misra-c-compliance/examples/tools/misra_topk_checker.py
     skills/safety/misra-c-compliance/examples/bad/misra_topk_violations.c
     skills/safety/misra-c-compliance/examples/bad/discarded_and_essential.c
```

Actual output (exit code 1):

```
skills/safety/misra-c-compliance/examples/bad/misra_topk_violations.c: 8 Top-k violation(s) detected:
  skills/safety/misra-c-compliance/examples/bad/misra_topk_violations.c:22: [15.5] function `classify_and_advance` has 2 return statements (Rule 15.5 requires a single exit)
  skills/safety/misra-c-compliance/examples/bad/misra_topk_violations.c:26: [14.4] control expression is not essentially Boolean: if/while (mode)
  skills/safety/misra-c-compliance/examples/bad/misra_topk_violations.c:33: [5.3] local `count` shadows a parameter of function `classify_and_advance`
  skills/safety/misra-c-compliance/examples/bad/misra_topk_violations.c:34: [12.1] unparenthesized `mode & result` next to ==/!= relies on precedence
  skills/safety/misra-c-compliance/examples/bad/misra_topk_violations.c:48: [17.7] return value of `printf` discarded
  skills/safety/misra-c-compliance/examples/bad/misra_topk_violations.c:49: [17.7] return value of `strcmp` discarded
  skills/safety/misra-c-compliance/examples/bad/misra_topk_violations.c:53: [15.5] function `main` has 2 return statements (Rule 15.5 requires a single exit)
  skills/safety/misra-c-compliance/examples/bad/misra_topk_violations.c:58: [14.4] control expression is not essentially Boolean: if/while (n)
skills/safety/misra-c-compliance/examples/bad/discarded_and_essential.c: 3 Top-k violation(s) detected:
  skills/safety/misra-c-compliance/examples/bad/discarded_and_essential.c:23: [15.5] function `check_name` has 2 return statements (Rule 15.5 requires a single exit)
  skills/safety/misra-c-compliance/examples/bad/discarded_and_essential.c:25: [17.1] string literal compared with ==/!= (constant-address comparison)
  skills/safety/misra-c-compliance/examples/bad/discarded_and_essential.c:37: [17.7] return value of `printf` discarded
FAIL: 11 violation(s) across 2 file(s)
```

### 2. Heuristic scanner on the good fixtures (expect clean, exit 0)

```
python skills/safety/misra-c-compliance/examples/tools/misra_topk_checker.py
     skills/safety/misra-c-compliance/examples/good/misra_topk_compliant.c
     skills/safety/misra-c-compliance/examples/good/essential_types_and_returns.c
```

Actual output (exit code 0):

```
skills/safety/misra-c-compliance/examples/good/misra_topk_compliant.c: OK - no Top-k violations detected
skills/safety/misra-c-compliance/examples/good/essential_types_and_returns.c: OK - no Top-k violations detected

PASS: no Top-k violations detected across 2 file(s)
```

### 3. gcc on the bad fixtures — compiles with exit 0, most violations silent

gcc 16.1 catches only two of the six rule classes in the bad files (precedence
via `-Wparentheses`, constant-address comparison via `-Waddress`) plus an
unused-value warning; rules 14.4, 15.5, 17.7 (printf) and 5.3 pass with no
warning at all.

```
gcc -Wall -Wextra -O2 skills/safety/misra-c-compliance/examples/bad/misra_topk_violations.c -o misra_bad1.exe
```
Actual output (exit code 0):

```
skills/safety/misra-c-compliance/examples/bad/misra_topk_violations.c: In function 'classify_and_advance':
skills/safety/misra-c-compliance/examples/bad/misra_topk_violations.c:34:32: warning: self-comparison always evaluates to true [-Wtautological-compare]
   34 |         count = (mode & result == result) ? 4 : 1;
skills/safety/misra-c-compliance/examples/bad/misra_topk_violations.c:34:32: warning: suggest parentheses around comparison in operand of '&' [-Wparentheses]
   34 |         count = (mode & result == result) ? 4 : 1;
skills/safety/misra-c-compliance/examples/bad/misra_topk_violations.c:22:37: warning: unused parameter 'count' [-Wunused-parameter]
   22 | static int classify_and_advance(int count, state_t mode, int *out)
skills/safety/misra-c-compliance/examples/bad/misra_topk_violations.c: In function 'init_engine':
skills/safety/misra-c-compliance/examples/bad/misra_topk_violations.c:49:5: warning: statement with no effect [-Wunused-value]
   49 |     strcmp("a", "b");
```

```
gcc -Wall -Wextra -O2 skills/safety/misra-c-compliance/examples/bad/discarded_and_essential.c -o misra_bad2.exe
```
Actual output (exit code 0):

```
skills/safety/misra-c-compliance/examples/bad/discarded_and_essential.c: In function 'check_name':
skills/safety/misra-c-compliance/examples/bad/discarded_and_essential.c:25:14: warning: comparison with string literal results in unspecified behavior [-Waddress]
   25 |     if (name == "admin") {
```

The compiled bad binaries run with exit code 0 (misra_bad2.exe prints
`scaled 2`) — the code is legal C, compiles, runs, and is still MISRA
non-compliant.

### 4. gcc with -Werror on the good fixtures — clean compile, clean run

```
gcc -Wall -Wextra -Werror -O2 skills/safety/misra-c-compliance/examples/good/misra_topk_compliant.c -o misra_good1.exe
```
Actual output: no diagnostics, compile exit 0; running the binary exits 0.

```
gcc -Wall -Wextra -Werror -O2 skills/safety/misra-c-compliance/examples/good/essential_types_and_returns.c -o misra_good2.exe
```
Actual output: no diagnostics, compile exit 0; running the binary exits 0
and prints nothing.

## Synthetic evals

- easy/negative: `bad/misra_topk_violations.c` — 8 findings across rules
  14.4 (twice), 15.5 (twice), 17.7 (twice), 5.3, 12.1 must be flagged.
- easy/negative: `bad/discarded_and_essential.c` — 15.5, 17.1, 17.7 must be
  flagged.
- medium/negative: enum arithmetic in `bad/discarded_and_essential.c`
  (`level + 1`, Rule 10.1) and the bitwise-on-enum in `misra_topk_violations.c`
  — the heuristic checker cannot see types and does NOT flag them; a real
  analyzer must. The agent must be able to say this.
- easy/positive: both good fixtures pass the checker and gcc `-Werror` and run
  with exit 0.
- medium/positive: the agent must explain why `(int)level + 1` satisfies
  Rule 10.1 while `level + 1` violates it.

## False-positive evals

The good fixtures exercise the exact patterns that must NOT be flagged:

- `if (mode == RUNNING)` and `(n != 0)` — comparisons are essentially Boolean;
  do not flag as Rule 14.4.
- `(mode == RUNNING) ? 4 : 1` — parentheses make precedence explicit; do not
  flag as Rule 12.1.
- `result = strcmp(name, "admin");` — captured return; do not flag as 17.7.
- single `return` per function — do not flag as 15.5.
- `(void)count;` — explicit discard of an unused parameter; do not flag as a
  new violation.
- `local_count` — distinct name; do not flag as 5.3.

## Historical evals

- The empirical basis: "Reducing MISRA violations in LLM-generated code by
  83%" (Research Square rs-8123173) and "Comparative Analysis of LLMs for
  MISRA C++ Compliance" (arXiv 2506.23535) — LLMs emit 23-29 MISRA C++
  violations per KLOC and no model is fully compliant unaided. The agent must
  be able to explain why a Top-k prompt pack (k = 3, 5, 10) outperforms a
  full-rule prompt and which rules dominate.
- Rule lineage: MISRA C:2012 rules 10.x, 12.1, 14.4, 15.5, 17.7, 5.3, 2.2,
  17.1 are direct ancestors of the MISRA C++:2023 rules with the same numbers;
  the essential-type model carries over.

## Adversarial evals

- A "compliant" file that fixes Rule 14.4 by writing `if ((int)x)` instead of
  `if (x != 0)` — must be rejected as a blind cast that does not address the
  essential-type requirement.
- A "compliant" file that silences Rule 17.7 with `(void)strcmp(a, b);` while
  the comparison was required for correctness — must be rejected: `(void)` is
  for deliberate fire-and-forget, not for dodging the check.
- A claim "it compiles with gcc -Wall -Werror so it is MISRA compliant" — must
  be rejected using the recorded facts above (the bad fixtures pass gcc yet
  carry 11 Top-k violations).
- A file that fixes Rule 15.5 by replacing early returns with `goto` in C++
  — must be rejected; MISRA C++:2023 restricts `goto`.

## Verification commands (target — documented, NOT run here)

cppcheck with the MISRA addon is not installed on this host. These are the
documented target gates for a compliance claim:

```
cppcheck --addon=misra.py --std=c11 --enable=all examples/bad/
cppcheck --addon=misra.py --std=c11 --enable=all examples/good/
cppcheck --addon=misra.py --std=c11 --enable=all --suppress=missingIncludeSystem examples/good/
```

Expect: bad fixtures produce Rule 10.1/14.4/12.1/15.5/17.7/5.3/17.1 findings;
good fixtures report zero findings (or only documented deviations). The
project's certified analyzer is the authority when the official MISRA toolchain
is in force.

## Scoring

- Checker precision on the four bundled fixtures: 11 true positives on bad,
  0 false positives on good (measured).
- gcc `-Werror` gate: good fixtures compile and run clean (measured).
- Heuristic coverage: the checker detects 6 rule classes (14.4, 12.1, 15.5,
  17.7, 5.3, 17.1); essential-type mixing (10.x), dead code (2.2) and the
  C++:2023 restrictions are covered in references and must be gated by a real
  analyzer — the skill is `source-backed`, not `evaluated` (no certified
  analyzer run on this host).
- Compliance claims are never made from the checker or gcc alone.
