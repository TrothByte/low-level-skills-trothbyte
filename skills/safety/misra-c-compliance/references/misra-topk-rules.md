# MISRA C/C++ Top-k Rules — Reference for LLM Agents

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE →
COUNTEREXAMPLE → VERIFICATION → SOURCE. Sources are listed at the end of
SKILL.md; ids here refer to the skill's source list.

## 1. Essential types (MISRA C:2012 Rule 10.x)

- **RULE**: every operand must be of an appropriate essential type (Boolean,
  signed, unsigned, enum, character, or floating); conversions between
  essential types must be explicit, and enum-typed operands may not feed
  arithmetic or bitwise operators.
- **WHY AI GETS IT WRONG**: LLMs write natural C — `level + 1` on an enum,
  `if (unsigned_count < -1)`, `c - '0'` stored into a signed int — and never
  think about the essential-type model, because the compiler does not reject
  it. C's implicit conversions hide the issue until the analyzer runs.
- **CORRECT REASONING**: separate the *underlying* type from the *essential*
  type. A cast to an essential type documents intent and stops the analyzer
  complaining; a cast that merely widens (e.g. `(int)enum_value + 1`) is
  acceptable, a cast that hides a sign mismatch is a second violation.
- **EXAMPLE** (bad):
  ```c
  typedef enum { LOW = 0, HIGH = 1 } level_t;
  int scaled = level + 1;   /* enum arithmetic, Rule 10.1 */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  int scaled = (int)level + 1;   /* explicit cast to essential type */
  ```
- **VERIFICATION**: static analyzer essential-type checker (cppcheck
  misra.py, SonarQube MISRA profile). The bundled heuristic checker cannot see
  types and does not flag enum arithmetic — this is analyzer territory.
- **SOURCE**: MISRA C:2012; cppcheck MISRA addon.

## 2. Boolean control expressions (Rule 14.4)

- **RULE**: the controlling expression of `if`, `while`, `for` and the
  condition of `?:` must be essentially Boolean; testing a non-Boolean value
  directly is a violation.
- **WHY AI GETS IT WRONG**: `if (flags)`, `if (ptr)`, `if (count)` is the
  most idiomatic C an LLM has seen; the model does not know MISRA demands
  `if (flags != 0)`, `if (ptr != NULL)`, `if (count > 0)`.
- **CORRECT REASONING**: an essentially-Boolean expression is one whose type
  is Boolean or a comparison / logical operator. A bare integer, pointer, or
  enum is not Boolean. Rewrite the test, do not cast: `if ((int)x != 0)` is
  still ugly; `if (x != 0)` is clean and compliant.
- **EXAMPLE** (bad):
  ```c
  if (result) {            /* result is int */
  while (p) {              /* p is a pointer */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  if (result != 0) {
  while (p != NULL) {
  ```
- **VERIFICATION**: scanner heuristic (bare identifier / bare bitwise
  expression in `if`/`while`); full check by analyzer.
- **SOURCE**: MISRA C:2012; GCC documentation (warnings do not catch this).

## 3. Operator precedence (Rule 12.1)

- **RULE**: the intended precedence of an expression must be stated
  unambiguously with parentheses whenever it is not obvious; in particular
  mixing bitwise operators with comparisons without parentheses is a
  violation.
- **WHY AI GETS IT WRONG**: the model relies on C's precedence table:
  `mode & result == result` is emitted meaning `(mode & result) == result`
  but parses as `mode & (result == result)`. C's own `-Wparentheses` warning
  flags exactly this shape.
- **CORRECT REASONING**: comparison operators bind tighter than `&`, `|`,
  `^`, and `<<`/`>>`. Whenever a bitwise operator is near a comparison, the
  parens are not optional — they are the compliance evidence.
- **EXAMPLE** (bad):
  ```c
  int ok = mode & kind == KIND_A;   /* parsed as mode & (kind == KIND_A) */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  int ok = ((mode & kind) == KIND_A);
  ```
- **VERIFICATION**: `gcc -Wall` flags the bad shape; scanner regex flags the
  `&`/`|`/`^` + `==` pattern; analyzer confirms.
- **SOURCE**: MISRA C:2012; GCC documentation.

## 4. Identifier shadowing (Rule 5.3)

- **RULE**: an inner declaration shall not hide (shadow) a declaration in an
  outer scope; each identifier must be unique within its scope chain.
- **WHY AI GETS IT WRONG**: LLMs reuse parameter names as loop counters or
  locals (`int count` inside a function whose parameter is `count`), which is
  common and legal in C but hides the outer meaning and is a certification
  blocker.
- **CORRECT REASONING**: rename the inner variable; the shadowed name is a
  maintenance and review hazard. gcc needs `-Wshadow` to notice, so a
  `-Wall -Werror` build stays silent.
- **EXAMPLE** (bad):
  ```c
  int f(int count) {
      { int count = 0; count++; }   /* shadows parameter */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  int f(int count) {
      { int local_count = 0; local_count++; }
  ```
- **VERIFICATION**: `gcc -Wshadow`, scanner heuristic (parameter name reused
  by a local declaration), analyzer.
- **SOURCE**: MISRA C:2012; GCC documentation.

## 5. Single point of exit (Rule 15.5)

- **RULE**: a function shall have a single point of exit at the end of its
  body (with the single allowed exception of `main`'s `return`).
- **WHY AI GETS IT WRONG**: early-return guard clauses are the natural LLM
  style for error handling; the model emits `if (bad) return -1;` several
  times per function without knowing the rule forbids it.
- **CORRECT REASONING**: stage the result in a local and return it once.
  Early returns require a documented deviation; do not convert the early
  return into a `goto`, which C++:2023 also restricts.
- **EXAMPLE** (bad):
  ```c
  int f(void) {
      if (a) return 1;   /* exit 1 */
      return 2;          /* exit 2 */
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  int f(void) {
      int result = (a != 0) ? 1 : 2;
      return result;     /* single exit */
  }
  ```
- **VERIFICATION**: scanner counts `return` statements per function body;
  analyzer.
- **SOURCE**: MISRA C:2012; MISRA C++:2023 (C++ variant retains the rule).

## 6. Discarded return values (Rule 17.7)

- **RULE**: the value returned by a function with a non-void return type
  shall be used; ignoring it is a violation. Explicitly casting to `(void)` is
  the accepted way to state "deliberately ignored".
- **WHY AI GETS IT WRONG**: `strcmp(a, b);` and `printf("...");` as bare
  statements are pervasive in LLM output; the model treats the return as
  optional because most compilers do not warn (no `warn_unused_result`
  attribute on these functions).
- **CORRECT REASONING**: capture the result (`int rc = strcmp(a, b);` and
  test it), or cast to `(void)` when the call is genuinely fire-and-forget
  (a deviation that is audit-friendly).
- **EXAMPLE** (bad):
  ```c
  strcmp(a, b);          /* result dropped */
  printf("hello\n");     /* int result dropped */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  int rc = strcmp(a, b);
  ...
  (void)printf("hello\n");
  ```
- **VERIFICATION**: scanner flags statement-position calls to known
  non-void functions; `-Wunused-result` for attribute-marked functions;
  analyzer.
- **SOURCE**: MISRA C:2012; GCC documentation.

## 7. Dead code and unused declarations (Rule 2.2)

- **RULE**: code shall not be unreachable; every declaration shall be used.
- **WHY AI GETS IT WRONG**: LLMs leave stub branches, unused locals, and
  statements after `return` while iterating on a solution; the leftover code
  is dead but still carries latent defects.
- **CORRECT REASONING**: delete unused locals and unreachable statements.
  gcc's `-Wunused-variable`/`-Wunreachable-code` catch the obvious cases; the
  analyzer catches the rest.
- **EXAMPLE** (bad):
  ```c
  int f(void) {
      int unused = 0;      /* never used */
      return 1;
      return 0;            /* unreachable */
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  int f(void) {
      return 1;
  }
  ```
- **VERIFICATION**: `gcc -Wall -Wunreachable-code`, analyzer.
- **SOURCE**: MISRA C:2012; GCC documentation.

## 8. Unsafe casts and C++:2023 restrictions (Rule 11.x / MISRA C++:2023)

- **RULE**: pointer/integer casts must be explicit and necessary (C:2012
  Rule 11.x); MISRA C++:2023 further restricts use of the C library, `goto`,
  `setjmp`, `reinterpret_cast` and `const_cast`.
- **WHY AI GETS IT WRONG**: the model reaches for `(int)ptr` or
  `reinterpret_cast<T*>` as a quick fix for type errors and uses `memcpy`,
  `strcpy`, `printf` from the C library out of habit.
- **CORRECT REASONING**: each cast must carry a justification comment and
  a deviation if it cannot be avoided; prefer typed alternatives. For C++,
  use the C++ standard library and static casts with defined semantics.
- **EXAMPLE** (bad):
  ```c
  uintptr_t addr = (uintptr_t)ptr;    /* implicit-free but unmotivated */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  /* deviation: hardware register access requires the conversion */
  uintptr_t addr = (uintptr_t)ptr;
  ```
- **VERIFICATION**: analyzer cast rules; review for justification.
- **SOURCE**: MISRA C:2012; MISRA C++:2023.

## Quick reference — Top-10 rules for LLM agents

Ordering follows the frequency with which LLM-generated code violates each
rule (research in the SKILL.md source list); confirm against your analyzer.

| # | Rule | One-line guidance |
|---|---|---|
| 1 | 10.x Essential types | explicit casts to essential types; no enum arithmetic, no implicit sign mixing |
| 2 | 14.4 Boolean control expressions | `if (x != 0)`, `if (ptr != NULL)`, never `if (x)` |
| 3 | 12.1 Precedence | parenthesize `&`/`|`/`^`/`<<`/`>>` next to comparisons |
| 4 | 15.5 Single point of exit | stage a result variable, one `return` per function |
| 5 | 17.7 Discarded return values | capture or `(void)` every non-void call result |
| 6 | 5.3 Identifier shadowing | never redeclare a parameter name in an inner scope |
| 7 | 2.2 Dead code | delete unreachable statements and unused declarations |
| 8 | 10.1/10.4 Signed/unsigned and casts | compare and convert same essential type; no narrowing without cast |
| 9 | 13.5 Short-circuit operands | right operand of `&&`/`||` must be side-effect free |
| 10 | 11.x / C++:2023 Casts, `goto`, C library | no `reinterpret_cast`/`const_cast`/`goto`/`setjmp`; C++ standard library only |
