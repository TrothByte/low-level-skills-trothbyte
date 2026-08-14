# C Integer Promotions & Conversions — Reference

Source: ISO C11 N1570 §6.3.1.1 (integer promotions), §6.3.1.8 (usual arithmetic conversions).
Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE → VERIFICATION → SOURCE.

## 1. Integer promotion (§6.3.1.1)

- **RULE**: Any type with rank lower than `int` (`char`, `short`, bit-fields, `_Bool`) is
  promoted to `int` if `int` can represent all its values; otherwise to `unsigned int`.
- **WHY AI GETS IT WRONG**: assumes `char + char` is done in `char`.
- **CORRECT REASONING**: `char` and `short` are widened to `int` before arithmetic. The
  result of `a + b` for two `char`s is `int`, which you then narrow back.
- **EXAMPLE** (bad): `char a = 100, b = 100; char c = a + b;` — `a+b` is `int` 200, then
  narrowing to `char` is implementation-defined (or UB if not representable).
- **COUNTEREXAMPLE** (good): `int c = a + b;` (keep the promoted type) or explicitly cast.
- **VERIFICATION**: `-Wconversion` flags the narrowing.
- **SOURCE**: N1570 §6.3.1.1p2.

## 2. Usual arithmetic conversions (§6.3.1.8)

- **RULE**: For a binary operator, operands convert to a common type. If one is unsigned
  and the other signed, and the unsigned type can represent all values of the signed type,
  the signed operand converts to the unsigned type. Otherwise both convert to the unsigned
  counterpart of the signed type.
- **WHY AI GETS IT WRONG**: assumes "the larger type wins" and that signed/unsigned mixing is benign.
- **CORRECT REASONING**: the signed value is converted to unsigned. `-1` becomes
  `UINT_MAX`. Comparisons and arithmetic then happen in unsigned.
- **EXAMPLE** (bad):
  ```c
  int i = -1;
  unsigned u = 1;
  if (i < u) { /* never true: i converts to UINT_MAX */ }
  ```
- **COUNTEREXAMPLE** (good): cast explicitly and compare in a domain you understand:
  ```c
  if (i < 0 || (unsigned)i < u) { /* handles negative first */ }
  ```
- **VERIFICATION**: `-Wsign-compare` warns on the mixed comparison.
- **SOURCE**: N1570 §6.3.1.8p1.

## 3. Signed → unsigned conversion is not value-preserving

- **RULE**: Converting a negative signed value to an unsigned type of the same or smaller
  rank wraps modulo 2^N (well-defined), but the result is a huge positive number.
- **WHY AI GETS IT WRONG**: "`(unsigned)-1` is `-1`" or "just a bit pattern".
- **CORRECT REASONING**: `(unsigned)-1 == UINT_MAX`. This is the mechanism behind the
  signed/unsigned comparison surprise and behind `size_t` sentinel bugs.
- **EXAMPLE** (bad): `size_t n = strlen(s); if (n - 1 > 10) ...` when `n == 0` → `n-1` is SIZE_MAX.
- **COUNTEREXAMPLE** (good): check `if (n == 0 || n - 1 > 10)`.
- **VERIFICATION**: `-Wconversion`; UBSan `-fsanitize=unsigned-integer-overflow` won't flag (well-defined), but review logic.
- **SOURCE**: N1570 §6.3.1.3p2.

## 4. size_t vs int (narrowing, 32-bit hazard)

- **RULE**: `size_t` is unsigned and at least 32 bits; converting it to `int` when the value
  exceeds `INT_MAX` is implementation-defined (or traps). On 32-bit, `size_t` is 32-bit and
  arithmetic can overflow where 64-bit would not.
- **WHY AI GETS IT WRONG**: assumes `int len = strlen(s)` is safe, and that code tested on
  64-bit behaves the same on 32-bit.
- **CORRECT REASONING**: keep lengths in `size_t`; only convert to a signed type with an
  explicit range check. CVE-2021-33909: a `size_t` buffer length passed to a function taking
  `int` went negative for >2 GB, causing an OOB write.
- **EXAMPLE** (bad): `int buflen = len; prepend(buf, buflen);` (buflen can be negative).
- **COUNTEREXAMPLE** (good): `if (len > INT_MAX) return -E2BIG; int buflen = (int)len;`.
- **VERIFICATION**: `-Wconversion`; 32-bit build (`-m32`) with UBSan.
- **SOURCE**: N1570 §6.3.1.3; CVE-2021-33909 (Sequoia); CERT INT31-C.

## 5. Multiplication before allocation (integer overflow)

- **RULE**: `malloc(n * sizeof(T))` overflows the multiplication silently (unsigned wrap),
  yielding a too-small buffer, which the caller then overflows. This is the classic
  "integer overflow before allocation" (CWE-190).
- **WHY AI GETS IT WRONG**: "`n * sizeof(T)` is fine; malloc handles the rest."
- **CORRECT REASONING**: compute the size in a checked way (e.g. `__builtin_mul_overflow` or
  explicit `if (n > SIZE_MAX / sizeof(T))`), then allocate. CVE-2016-8617: `malloc(insize*4/3+4)`
  overflowed on 32-bit for `insize >= 1 GB`.
- **EXAMPLE** (bad):
  ```c
  char *buf = malloc(n * 4 / 3 + 4);
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  if (n > (SIZE_MAX - 4) / 4 * 3) return -1;
  char *buf = malloc(n * 4 / 3 + 4);
  ```
- **VERIFICATION**: `-fsanitize=undefined` (won't flag unsigned wrap), so use explicit
  check + 32-bit build; `gcc -ftrapv` or `__builtin_mul_overflow`.
- **SOURCE**: N1570 §7.22.3; CVE-2016-8617; CWE-190.

## 6. Integer promotion in bitwise/shift contexts

- **RULE**: `char`/`short` promote to `int` before `&`, `|`, `^`, `~`, `<<`. The result is
  `int`, so `~c` for `char c` is sign-extended and can surprise in masks.
- **WHY AI GETS IT WRONG**: "`~c` gives the inverted `char` byte."
- **CORRECT REASONING**: `c` promotes to `int`, `~c` is an `int` with high bits set; when
  you then `& 0xff` it is fine, but assigning to `char` or comparing needs care.
- **EXAMPLE** (bad): `unsigned char x = ~c;` (already sign-extended, then truncated — usually fine but fragile).
- **COUNTEREXAMPLE** (good): `unsigned char x = (unsigned char)~c;` — make intent explicit.
- **VERIFICATION**: `-Wconversion`; UBSan `-fsanitize=implicit-conversion`.
- **SOURCE**: N1570 §6.3.1.1, §6.5.3.3.

## Quick reference table

| Situation | What happens | Tool |
|---|---|---|
| `char`/`short` in arithmetic | promote to `int` | `-Wconversion` |
| signed vs unsigned comparison | signed → unsigned (wrap) | `-Wsign-compare` |
| negative → `size_t` | huge positive | logic review |
| `size_t` → `int` (> INT_MAX) | impl-defined/negative | `-Wconversion`, `-m32` |
| `malloc(n * k)` | unsigned overflow | explicit check / `__builtin_mul_overflow` |
| `~c` on `char` | sign-extend to `int` | `-Wconversion` |
