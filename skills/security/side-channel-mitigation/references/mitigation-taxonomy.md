# Side-Channel Mitigation — Reference

Sources: `dudect`, `ctgrind`, `tob-constant-time`, `cwe-1254`, `cwe`
(CWE-203/385), `intel-sdm`. The timing demo in examples was compiled and run
with gcc 16.1 on this host (x86-64); the dudect/ctgrind commands are target
toolchains.

## 1. Channel-to-countermeasure mapping

- **RULE**: choose the countermeasure from the attacker's observation, not
  from the crypto algorithm: timing → constant-time ops; cache (address) →
  read-all-then-select or cache-line masking; power/EM → masking/blinding/
  shuffling; speculative execution → fences + bounds checks (or documented
  hardware mitigation).
- **WHY AI GETS IT WRONG**: assuming one universal fix ("use constant-time")
  for every channel (A10).
- **CORRECT REASONING**: a table lookup leaks via the cache even if the
  control flow is constant-time; an S-box load must be converted to
  bitslicing or a masked full-table read.
- **EXAMPLE**: AES T-table → bitsliced or fixed-window lookup.
- **COUNTEREXAMPLE**: "I use constant-time AES with a T-table" — the address
  index is secret-dependent.
- **VERIFICATION**: `objdump -d` shows the table load indexed by the secret;
  dudect t-test on the target.
- **SOURCE**: `dudect`; `ctgrind`; `cwe` CWE-203/CWE-385.

## 2. Constant-time primitives and their compiler fate

- **RULE**: a constant-time comparison must not early-exit on a secret byte;
  the XOR-fold then final compare idiom (`acc |= a[i]^b[i]`, return `acc==0`)
  is the canonical pattern. Compilers may transform it — verify in assembly.
- **WHY AI GETS IT WRONG**: reviewing only the C source (B7); gcc can turn a
  fold into a `memcmp`-like early exit, or an `if` on a secret into a branch.
- **CORRECT REASONING**: inspect `objdump -d`/`-S` output: no conditional
  jump on a secret byte, no early return. Use `__attribute__((noinline))`
  and memory clobbers where the transform is hostile.
- **EXAMPLE**: `examples/good/ct_compare_demo.c` — the XOR-fold version.
- **COUNTEREXAMPLE**: `if (a[i] != b[i]) return 0;` on secret data — leaks
  the first-difference position (CWE-1254).
- **VERIFICATION**: `gcc -O2 -S` + `objdump -d` on this host; run the demo.
- **SOURCE**: `cwe-1254`; `tob-constant-time`.

## 3. Masking and blinding

- **RULE**: masking splits a secret into shares; every intermediate stays
  independent of the secret only if the mask is fresh, uniform, and
  independent per share; blinding randomizes a public value so the attacker's
  statistical link breaks. Both must be combined with constant-time
  implementation.
- **WHY AI GETS IT WRONG**: a fixed "mask" constant (e.g., `0xFF`) is not a
  mask; reusing the same mask across operations leaks through
  higher-order effects (B2).
- **CORRECT REASONING**: mask = fresh random per operation; blinding value =
  fresh random per exponentiation; verify the masked path still has no
  secret-dependent branch (masking doesn't fix branches).
- **EXAMPLE**: masking an S-box index: `idx_masked = idx ^ m; real =
  sbox[idx_masked ^ m]` with fresh `m`.
- **COUNTEREXAMPLE**: `idx_masked = idx ^ 0x00` — no masking at all.
- **VERIFICATION**: source review + dudect/ctgrind on target.
- **SOURCE**: `dudect` (methodology); research references in `cwe`.

## 4. Speculative-execution leaks

- **RULE**: Spectre (CVE-2017-5753) reads out-of-bounds data into a cache
  line via speculative execution after a missed bounds check; Meltdown
  (CVE-2017-5754) reads kernel memory through speculative loads. Software
  mitigation: bounds check + `lfence`/`ISB` before the deref, or rely on
  microcode/OS mitigation with the assumption stated.
- **WHY AI GETS IT WRONG**: treating constant-time code as Spectre-proof
  (the leak is about speculation, not data-dependent timing) (A10).
- **CORRECT REASONING**: a secret-dependent *address* formed after a bounds
  check is a Spectre gadget; place the fence after the check and before the
  dereference, or document the hardware mitigation that covers it.
- **EXAMPLE**: `if (idx < len) { _mm_lfence(); x = table[idx]; }`.
- **COUNTEREXAMPLE**: `x = table[idx];` with `idx` derived from a secret and
  a prior (speculative) bounds check.
- **VERIFICATION**: Spectre-test harness on the target (documented);
  `objdump -d` for the fence placement.
- **SOURCE**: `intel-sdm` (speculative-execution chapters); research refs.

## 5. Verification discipline: measure, don't assert

- **RULE**: a mitigation claim requires evidence: (a) assembly has no secret
  branch/index/div; (b) dudect t-test shows no class separation; (c) the
  compiler version is pinned and re-audited on upgrade.
- **WHY AI GETS IT WRONG**: "I reviewed the code, it's fine" without any
  measurement (B7).
- **CORRECT REASONING**: run the differential test; inspect the emitted
  instructions; record the tool output in the audit.
- **EXAMPLE**: `dudect -n 10000000` on the target with the fixed-vs-random
  classes.
- **COUNTEREXAMPLE**: a PR that says "constant-time" with no test artifact.
- **VERIFICATION**: `examples/good/ct_compare_demo.c` timing run (host);
  target dudect/ctgrind.
- **SOURCE**: `dudect`; `ctgrind`; `tob-constant-time`.
