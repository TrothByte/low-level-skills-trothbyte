# Constant-Time Code — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE
→ VERIFICATION → SOURCE. Source ids refer to registry/sources.yaml.

## 1. No secret-dependent branches

- **RULE**: control flow (branches, conditional returns, early exits) must never
  depend on secret data. The set of executed instructions and the path length
  must be identical for every secret value.
- **WHY AI GETS IT WRONG**: the model optimizes for "correct and readable" and
  writes the natural `if (secret > 0) ...` or a byte-wise early-exit `memcmp`,
  forgetting that path length is observable by an attacker; LLM-generated crypto
  shows this leak in roughly 40% of cases (Copilot study, CWE-1254).
- **CORRECT REASONING**: a branch whose condition is a secret makes execution
  time, branch-predictor state, and cache state depend on the secret. Even a
  "single byte" leak compounds: the attacker recovers the secret one bit at a
  time. The fix is to make the work identical for all inputs and derive the
  result arithmetically (XOR-fold, select).
- **EXAMPLE** (bad):
  ```c
  for (i = 0; i < n; i++)
      if (a[i] != b[i]) return 0;  /* leaks first-diff index */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  uint8_t acc = 0;
  for (i = 0; i < n; i++) acc |= a[i] ^ b[i];
  return acc == 0;
  ```
- **VERIFICATION**: fixed work per input — count executed iterations in a
  harness; or dudect (differential) / ctgrind (Valgrind taint, BearSSL).
- **SOURCE**: cwe-1254; tob-constant-time; dudect; ctgrind.

## 2. No secret-indexed memory access

- **RULE**: memory addresses (array indices, pointer arithmetic) must never be
  derived from secret data. Every secret value must touch the same cache lines.
- **WHY AI GETS IT WRONG**: S-box / lookup-table patterns are idiomatic crypto,
  so models generate `table[secret]` without realizing the cache line touched is
  itself a signal; on a shared CPU the attacker measures which line is hot.
- **CORRECT REASONING**: a load at address `base + secret*k` warms exactly one
  cache line, leaking `floor(secret*k/64)` through reload timing. Constant-time
  replacements read ALL candidates and select arithmetically (`mask & (a) | ~mask & (b)`),
  or use CPU hardware AES instructions that are designed to be data-independent.
- **EXAMPLE** (bad):
  ```c
  out ^= sbox[data[i] ^ key];   /* index depends on key */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  uint8_t sel(uint8_t bit, uint8_t a, uint8_t b) {
      uint8_t m = (uint8_t)(0u - bit);
      return (a & m) | (b & ~m);
  }
  ```
  then fold over all 256 table entries.
- **VERIFICATION**: ctgrind marks the index tainted and flags it; a
  cache-reload microbenchmark shows the leak (recorded numbers in this skill's
  evals).
- **SOURCE**: cwe-1254; ctgrind; dudect.

## 3. Integer division can be value-dependent

- **RULE**: some implementations of integer division (UDIV/SDIV on certain ARM
  and x86 parts) take a number of cycles that depends on the operand values;
  code that divides by a secret must not assume constant latency.
- **WHY AI GETS IT WRONG**: C compilers and ISAs "abstract" division as a single
  operator with unspecified latency, and models assume `x / y` is one fixed-cost
  instruction; the ML-DSA CVE-2026-22705 disclosure demonstrated real UDIV/SDIV
  timing dependence in a signing path.
- **CORRECT REASONING**: when a secret feeds a divisor or dividend, replace the
  division with a constant-latency sequence: multiply-by-reciprocal with shift
  (compiler-generated `mul` + `shr`), or table-free Montgomery/reduction math
  that avoids division entirely. Never treat `div` as data-independent.
- **EXAMPLE** (bad):
  ```c
  uint32_t s = 1000000000u / (1u + (secret & 7u)); /* latency varies */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  return (uint32_t)(((uint64_t)x * 2863311531ull) >> 33); /* fixed mul+shr */
  ```
- **VERIFICATION**: disassemble (`objdump -d`) — `div`/`idiv` present = suspect;
  count cycles for extreme operand pairs, or run a dudect-style measurement.
- **SOURCE**: CVE-2026-22705; tob-constant-time; dudect.

## 4. Early-exit and string-equality shortcuts are leaks

- **RULE**: `memcmp`, `strcmp`, `==` on strings, `strncmp` and their hand-rolled
  equivalents are allowed only on NON-secret data. On secret data they must be
  replaced with a constant-time compare.
- **WHY AI GETS IT WRONG**: the model uses the standard library "because it is
  correct and fast"; it does not ask whether the data is secret. CWE-1254
  quantifies that ~40% of Copilot-generated crypto contains such leaks.
- **CORRECT REASONING**: `memcmp`/`strcmp` implementations exit at the first
  mismatch. `==` on strings compares pointers or length first — all
  secret-dependent. The check must be: *who can observe the timing?* If anyone
  else on the CPU/network can, the compare must be constant-time.
- **EXAMPLE** (bad):
  ```c
  if (strcmp(user_token, stored_token) == 0) grant();
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  if (ct_tag_equals(stored_token, user_token, TOKEN_LEN) == 0) grant();
  ```
  with `ct_tag_equals` folding all bytes into one accumulator before branching.
- **VERIFICATION**: benchmark both on this host (recorded in evals/README.md):
  early-exit shows a ~0.05 s delta between first-byte and last-byte mismatch
  over 500k iterations; the constant-time version shows ~0.
- **SOURCE**: cwe-1254; dudect; ctgrind.

## 5. "Optimizer-friendly" code silently reintroduces leaks

- **RULE**: constant-time intent is fragile under optimization: compilers can
  hoist/eliminate loads, convert `acc |= a[i]^b[i]` into vectorized
  short-circuit logic, or turn a select into a conditional move. Always check
  the generated assembly at `-O2`.
- **WHY AI GETS IT WRONG**: the model verifies the C source, not the object
  code; it assumes the compiler preserves the loop and the reads. GCC can
  transform a constant-time fold into `memcmp`-like early exit when it can
  prove a byte difference short-circuits the result.
- **CORRECT REASONING**: the constant-time property lives in the machine code.
  Inspect `objdump -d` for: no data-dependent branch/jump, no data-derived
  address, no `rep movs`/vectorized compare feeding a branch on secret bytes.
  Use volatile reads or intrinsics only when the compiler provably cooperates.
- **EXAMPLE** (bad): writing the constant-time fold, then shipping it without
  ever disassembling at `-O2`.
- **COUNTEREXAMPLE** (good): `gcc -O2 -c ct_compare.c && objdump -d` and confirm
  the XOR-fold loop is intact and the final branch is on the accumulator only.
- **VERIFICATION**: `objdump -d` on the `-O2` object; run dudect/ctgrind on the
  compiled binary, not the source.
- **SOURCE**: tob-constant-time; cwe-1254.

## Quick reference table

| Topic | Rule in one line |
|---|---|
| Branches | never branch on a secret — path length is a signal |
| Indices | never derive an address from a secret — cache lines are a signal |
| Division | `div`/`idiv` latency can depend on operands (ML-DSA CVE) |
| Compare | `memcmp`/`strcmp`/`==` exit early — constant-time fold instead |
| Optimization | verify the `-O2` assembly, not the C source |
| Tooling | dudect (differential timing), ctgrind (taint), `objdump -d` |
