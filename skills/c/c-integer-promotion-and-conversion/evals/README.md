# Evaluation — c-integer-promotion-and-conversion

Skill: `skills/c/c-integer-promotion-and-conversion`. Stability target: `evaluated`.

## Historical CVE evals

| CVE | Class | Fixture | Detect | Fix |
|---|---|---|---|---|
| CVE-2022-0185 | unsigned arithmetic underflow → heap overflow | linux 722d948 | `len > PAGE_SIZE - 2 - size` computes in unsigned | `size + len + 2 > PAGE_SIZE` |
| CVE-2016-8617 | int overflow before allocation | curl efd24d5 | `malloc(insize*4/3+4)` | `#if SIZEOF_SIZE_T == 4` guard |
| CVE-2021-33909 | size_t → int truncation → OOB write | linux 8cae8cd | `size_t` passed to `int` param | check `> INT_MAX` first |

## Synthetic evals

- **easy/negative**: `int i = -1; unsigned u = 0; if (i < u)` — detect signed→unsigned surprise.
- **medium/negative**: `int len = strlen(s)` — detect narrowing.
- **hard/negative**: `malloc(n * 4 / 3 + 4)` — detect allocation overflow.
- **adversarial**: code correct on 64-bit, broken on 32-bit (size_t arithmetic) — must reason about platform width.

## False-positive evals

- Correct unsigned-only wrap arithmetic (e.g. a hash mixing) — must NOT be flagged.
- A guarded `size_t → int` conversion with an explicit range check — must NOT be flagged.

## Verification commands

```
gcc -Wall -Wextra -Wconversion -Wsign-compare -c examples/bad/integer_snippets.c  # expect warnings
gcc -Wall -Wextra -Wconversion -Wsign-compare -c examples/good/integer_snippets.c # expect clean
```
