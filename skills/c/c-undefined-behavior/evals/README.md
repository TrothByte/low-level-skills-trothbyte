# Evaluation — c-undefined-behavior

Skill: `skills/c/c-undefined-behavior`. Stability target: `evaluated`.

## Historical CVE evals

| CVE | Class | Fixture | Detect | Fix | Verify |
|---|---|---|---|---|---|
| CVE-2022-3602 | off-by-one (punycode) | OpenSSL fe3b639 | `written_out > max_out` should be `>=` | 1-line | ASan + regression |
| CVE-2018-16890 | int overflow → OOB read (NTLM) | curl b780b30 | `offset + len > size` not guarded | 1 condition | ASan |
| CVE-2016-8617 | int overflow before alloc (base64) | curl efd24d5 | `malloc(insize*4/3+4)` on 32-bit | +5 lines guard | UBSan 32-bit |

Each eval: DETECT (find the UB) → EXPLAIN (name the J.2 class) → FIX (remove UB at source)
→ VERIFY (sanitizer clean + asm preserved).

## Synthetic evals

- **easy/positive**: correct `unsigned` wrap check — must NOT flag (see examples/good).
- **easy/negative**: `1u << n` with unguarded `n` — must detect shift UB.
- **medium/negative**: signed overflow check `x + 1 > x` — must detect signed overflow.
- **hard/negative**: multi-layer `memcpy` overlap after integer conversion — must detect overlap.
- **adversarial**: code that passes tests on x86 but is UB (e.g. signed overflow in a
  bounded input that the test never exercises at the boundary).

## False-positive evals (correct code must not be flagged)

- `int f(int x) { return x + 1; }` — no UB if callers guarantee no overflow; do NOT flag.
- Correct signed arithmetic within documented bounds — do NOT flag signed overflow.
- Correct `shift` with a verified in-range count — do NOT flag.

## Verification commands

```
clang -O2 -g -fsanitize=undefined -fno-sanitize-recover=undefined examples/bad/ub_snippets.c -o /tmp/ub_bad
/tmp/ub_bad   # expect UBSan reports
clang -O2 -g -fsanitize=undefined -fno-sanitize-recover=undefined examples/good/ub_snippets.c -o /tmp/ub_good
/tmp/ub_good  # expect clean
```

## Scoring (for routing eval)

- precision: UB reports must map to a real J.2 class.
- recall: each bad snippet must be detected.
- FP-rate: good snippets must produce zero reports.
