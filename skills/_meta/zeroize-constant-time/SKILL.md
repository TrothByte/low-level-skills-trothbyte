---
name: zeroize-constant-time
description: Use when writing or reviewing code handling secrets (keys, passwords, nonces) that must be zeroized or compared in constant time. Triggers on memset-before-return, secret-dependent branches or indexing, memcmp on secrets, and claims that a secret is cleared. Teaches volatile-sink zeroization, explicit_bzero, ct_memcmp, and asm verification.
---

# Zeroization & Constant-Time Code

## When to use

- Secrets (keys, passwords, nonces) are copied into memory and must be wiped after use.
- Secrets are compared (MAC, password, key check) where timing must not depend on the value.
- Reviewing any `memset(secret, 0, n)` — ask "will the compiler keep this?".
- Any claim that "the secret is cleared" — distrust until asm proves the wipe.

## When not to use

- The value is public, not a secret — plain `memset`/`free`/`memcmp` is fine.
- Wiping/constant-time is handled by a hardware token or OS primitive you do not control.

## What the agent often gets wrong

- "`memset(secret, 0, n)` clears the secret." The optimizer can prove the buffer dead after
  the last use and delete the memset; GCC 16.1 `-O2` elides a stack-local wipe entirely
  (verified: body compiles to just `ret`).
- "`volatile` fixes it." Only stores THROUGH a volatile lvalue are observable; a volatile
  array cast to `unsigned char *` for the wipe drops the qualifier and is elided again.
- "Source `if (x == secret)` leaks / is constant-time." The truth is in asm: GCC 16.1 folds
  trivial `secret == guess` to `sete`/`cmov` but keeps branches in early-exit loops.
- "`memcmp` on secrets is fine." Early exit returns at the first differing byte; runtime
  leaks the position of the first difference.
- "`free()` clears the memory." The block keeps the secret; the allocator may reuse it.
- "Wiping the buffer is enough." Secrets also survive in registers and other copies.

## How to reason correctly

1. Find every copy of the secret and its lifetime.
2. Wipe each copy that must die with an OBSERVABLE side effect: write through a `volatile`
   pointer (`secure_zero_memory`), `memset` + compiler barrier / `explicit_bzero`, or C23
   `memset_explicit`.
3. Treat "source says wiped" as unverified until `-O2 -S` asm shows the stores.
4. Design constant-time from the source: no early exit, no secret-dependent branch or
   index; XOR-accumulate bytes, then one final compare on the aggregate.
5. Verify at the asm level. A compiler may convert a branch to `cmov`/`sete`, but you may
   not rely on it — the source must be constant-time even if the branch is kept.

## What to verify

- The wipe appears in `-O2` asm: `memset`/`explicit_bzero` call or byte/vector stores to
  the secret's stack slot or heap range.
- Constant-time loops have no early-exit branch on secret bytes and no secret-indexed load.
- No `memcmp`/`strcmp` on secrets without a constant-time replacement.
- Rust: `zeroize()` runs for every copy (no `Drop`-skipping path).

## How to verify

```
gcc -O2 -S examples/bad/zeroize_bad.c -o bad.s
grep -E "memset|stosb|movb \$0" bad.s      # wipe gone in B1 (body is just ret)
gcc -O2 -S examples/good/zeroize_good.c -o good.s
grep -E "memset|stosb|movb \$0" good.s     # wipe present in G1/G2/G3
grep -E "je|jne" good.s                    # only final result test, no per-byte branch
```

## Where the knowledge comes from

- `iso-c11-n1570` §5.1.2.3, §6.7.3; `iso-c23-n3096` (`memset_explicit`)
- `gcc-manual`, `clang-docs` (optimizer/asm), `carruth-gigo`, `godbolt-compiler`
- `rust-reference`, `rustonomicon` (Zeroize trait concepts)
- `cwe` CWE-14, CWE-208; `cert-c` MSC13-C

## Related skills

- `compiler-ub-assumptions` — require of: the optimizer removes "dead" code the same way
  it exploits UB
- `c-undefined-behavior` — require of: "it compiles" proves nothing
- `meta-verification` — require of: verify by asm, not by reading source
- `rust-unsafe-reasoning` — Rust-side zeroization and `Drop`

## Evaluation

Synthetic: easy (wiped-local memset that `-O2` removes), medium (early-exit memcmp on a
secret), hard (volatile-cast-drops-qualifier wipe silently elided), adversarial (code that
passes a functional test but leaks via timing).
False-positive: plain memset/memcmp on public data, correct accumulator compare, correct
wipe-then-free — must NOT be flagged.
Verification commands and verified facts: see `evals/README.md`.
