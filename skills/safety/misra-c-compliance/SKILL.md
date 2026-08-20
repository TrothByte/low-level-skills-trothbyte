---
name: misra-c-compliance
description: Use when writing or reviewing C/C++ for automotive, aerospace, medical or other safety-critical systems that must comply with MISRA C:2012 or MISRA C++:2023. Covers the Top-k most-violated rules LLMs break, essential-type casts, boolean control expressions, and verification with static analyzers.
---

# MISRA C/C++ Compliance — Top-k Rules

## When to use

- Writing or reviewing C or C++ for automotive, aerospace, medical, rail or
  industrial functional-safety development where MISRA C:2012 or MISRA
  C++:2023 applies.
- Adding code to a project with an enforced MISRA profile: an analyzer gate, a
  deviation list, or a certification evidence trail.
- Triaging a MISRA analyzer report and choosing between a cast, a redesign, or
  a documented deviation.
- Instructing an LLM to generate safety-critical C/C++: a concise Top-k rule
  pack (k = 3, 5, 10) cuts MISRA violations by up to 83% versus an unprompted
  model.
- Estimating how many violations a fresh LLM-written file will introduce
  before it reaches a static analyzer.

## When not to use

- General-purpose C/C++ with no MISRA requirement: the rule set is stricter
  than ordinary "good C", and wholesale application adds noise.
- Projects that pin a different standard (AUTOSAR, JSF, C Secure Coding /
  CWE-only): route to the applicable rule set instead.
- Certification work: this skill gates with a heuristic checker and documents
  analyzer commands, but a compliance claim requires the project's official
  toolchain and a safety-team deviation review.
- C++ features the MISRA C++:2023 profile restricts (the C library, `goto`,
  `setjmp`, `reinterpret_cast`, `const_cast`): those need the C++ rule book,
  not the C one.

## What the agent often gets wrong

- Writes `if (flags)` and `if (ptr)`: Rule 14.4 requires control expressions to
  be essentially Boolean (`if (flags != 0)`, `if (ptr != NULL)`).
- Mixes essential types without explicit casts (Rule 10.x): enum arithmetic,
  signed/unsigned comparisons, character data treated as int.
- Relies on operator precedence instead of parentheses (Rule 12.1):
  `a & b == c` parses as `a & (b == c)`.
- Leaves multiple `return` statements per function (Rule 15.5) and shadowed
  identifiers (Rule 5.3).
- Calls `strcmp`, `printf`, `chmod` and drops the result (Rule 17.7).
- Ships dead code and unused declarations (Rule 2.2); in C++:2023 keeps using
  C library functions and unsafe casts.
- "Fixes" a violation with a blind cast that creates an essential-type
  conversion of its own, or disables the checker, instead of redesigning.
- Claims compliance from inspection without ever running a static analyzer.
- Empirically: LLMs emit 23-29 MISRA C++ violations per KLOC and no model
  produces fully compliant code unaided; a Top-k instruction pack reduces
  violations by up to 83% (sources in "Where the knowledge comes from").

## How to reason correctly

1. Confirm the exact edition and profile in force — MISRA C:2012 versus MISRA
   C++:2023, amendment level, and the project's permitted deviation list —
   before claiming or promising anything.
2. Treat the static analyzer as the authority and hard gate: cppcheck with the
   misra.py addon, SonarQube with a MISRA profile, or the project's certified
   tool. Never certify by inspection.
3. Classify every finding as "cast needed" or "redesign needed". Prefer
   redesign: a cast that only silences the diagnostic obscures the type model
   and usually feeds another Rule 10.x violation.
4. Work the Top-k priority queue: fix the highest-frequency rules first — the
   majority of an LLM's violations cluster in a small set of rules.
5. Enforce the zero-new-violation constraint: each fix must not introduce a
   different rule violation; a cast that creates a conversion violation is the
   classic slip.
6. Report honestly: compliance is a property of the analyzed build, not of a
   source file passing a code review.

## What to verify

- Every function has exactly one exit point (Rule 15.5); early returns are
  staged into a result variable and returned once.
- Every `if`, `while`, `for` and `?:` control expression is essentially Boolean
  (Rule 14.4).
- No implicit signed/unsigned or essential-type mixing; every conversion is an
  explicit cast to the essential type (Rule 10.x).
- No return value of a non-void function is discarded (Rule 17.7).
- No identifier shadowing and no namespace pollution (Rule 5.3).
- Precedence is made explicit with parentheses wherever `&`, `|`, `^`, `<<`,
  `>>` meet comparisons or arithmetic (Rule 12.1).
- The static analyzer reports zero violations of the chosen rule set, or every
  remaining item carries a deviation approved by the safety team.
- No dead code, unreachable statements, or unused declarations (Rule 2.2).

## How to verify

Run the bundled heuristic scanner on both example sets (host-verifiable here;
real output is recorded in `evals/README.md`):

```
python examples/tools/misra_topk_checker.py examples/bad/*.c
python examples/tools/misra_topk_checker.py examples/good/*.c
```

Compile both sets with gcc and run the good fixtures:

```
gcc -Wall -Wextra -Werror -O2 examples/good/misra_topk_compliant.c -o good1.exe
./good1.exe
gcc -Wall -Wextra -Werror -O2 examples/good/essential_types_and_returns.c -o good2.exe
./good2.exe
```

Target gate (documented, not run on this host — cppcheck with the misra.py
addon is not installed here):

```
cppcheck --addon=misra.py --std=c11 --enable=all examples/bad/
cppcheck --addon=misra.py --std=c11 --enable=all examples/good/
```

The python checker is a fast feedback loop for Top-k patterns, never proof of
compliance; the analyzer gate decides.

## Where the knowledge comes from

- MISRA C:2012 / MISRA C++:2023 standards (https://misra.org.uk)
- Reducing MISRA violations in LLM-generated code by 83% — Research Square rs-8123173 (https://www.researchsquare.com/article/rs-8123173/v1)
- Comparative Analysis of LLMs for MISRA C++ Compliance — arXiv 2506.23535 (https://arxiv.org/abs/2506.23535)
- cppcheck MISRA addon (https://github.com/danmar/cppcheck)
- GCC documentation (https://gcc.gnu.org/onlinedocs)

## Related skills

- `c-undefined-behavior`
- `c-integer-promotion-and-conversion`
- `c-string-and-buffer-safety`
- `meta-verification`
- `compiler-ub-assumptions`

## Evaluation

- Synthetic: the scanner must flag every intentional violation in
  `examples/bad/` (rules 14.4, 12.1, 15.5, 17.7, 5.3, 17.1) and approve every
  file in `examples/good/`; gcc must compile the good files with `-Werror` and
  run.
- False-positive: the good fixtures use `if (mode == RUNNING)`, `(n != 0)`,
  captured `strcmp` results, and single exits — none may be flagged.
- Historical: the 83% reduction study (Research Square rs-8123173) — an agent
  must be able to explain why a Top-k pack works and which rules it targets.
- Adversarial: a "compliant" file that hides a violation behind a blind cast or
  a discarded result must be caught; a file that merely compiles clean under
  `-Wall -Werror` must not be called compliant.
- Host runs with real output: `evals/README.md`.
