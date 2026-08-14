# Zeroization & Constant-Time Code — Reference

Source: ISO C11 N1570 §5.1.2.3 / §6.7.3, C23 N3096 (`memset_explicit`), GCC 16.1 asm
(empirical, x86-64 MinGW), Carruth GIGO, Godbolt talks, Rust Reference / Rustonomicon.
Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE → VERIFICATION → SOURCE.

## 1. The optimizer removes "dead" zeroization

- **RULE**: `memset(secret, 0, n)` is removable when the optimizer can prove the buffer
  is dead after the last read. Zeroizing is a store whose result is never observed, so the
  abstract machine does not force it (N1570 §5.1.2.3 — a store is observable only if it
  reaches a volatile object or changes a value read later; §6.2.4 — lifetime of automatic
  objects ends at scope exit).
- **WHY AI GETS IT WRONG**: "memset writes to memory, of course it stays in the binary."
- **CORRECT REASONING**: the optimizer performs dead-store elimination. For an automatic
  object that never escapes (no `&secret` passed out, never read after the wipe), the store
  of zeros has no observable effect and is deleted. Verified empirically with GCC 16.1
  (`gcc -O2 -S`): `wipe_stack_buf` compiles to a body of exactly `ret` — the fill loop AND
  the memset are both gone. With `-fno-lifetime-dse` the local-variable elision is
  unaffected when the whole object is dead; the flag only helps for the object-lifetime
  analysis, not for provably-unused stores.
- **EXAMPLE** (bad): see `examples/bad/zeroize_bad.c` `wipe_stack_buf` —
  `void wipe_stack_buf(size_t n) { unsigned char s[64]; ...; memset(s, 0, 64); }`
  at `-O2` → asm body is `ret` only.
- **COUNTEREXAMPLE** (good): `secure_zero_memory` writes through a `volatile` pointer
  (`while (n--) *p++ = 0;`) — volatile stores are observable side effects, never elided.
  Verified: `wipe_volatile_sink` emits a real `movb $0, (%rax)` store loop at `-O2`.
- **VERIFICATION**: `gcc -O2 -S bad.c` and grep for `memset`/`movb $0`/`stosb` in the
  function body; compare against the volatile version.
- **SOURCE**: N1570 §5.1.2.3, §6.2.4, §6.7.3; gcc-manual (Optimize Options, inline asm);
  empirical GCC 16.1.

## 2. The volatile-sink / secure_zero_memory pattern

- **RULE**: zeroization must be an observable side effect. Writes through a
  `volatile unsigned char *` qualify (N1570 §6.7.3p7: accesses to volatile objects are
  evaluated strictly per the abstract machine, so the optimizer cannot delete them).
  `explicit_bzero`/`secure_zero_memory` wrap this loop; a compiler barrier
  (`__asm__ __volatile__("" ::: "memory")`) also anchors a preceding `memset`.
- **WHY AI GETS IT WRONG**: "I declared the array volatile" — declaring the OBJECT
  volatile is not enough if you then cast it to `unsigned char *` for the wipe loop; the
  cast drops the qualifier and the loop becomes ordinary (removable) stores. Verified:
  `login_check_good` (volatile array + cast to `unsigned char *` for the wipe) lost the
  wipe at `-O2`.
- **CORRECT REASONING**: the qualifier that matters is on the POINTER USED FOR THE STORE.
  `volatile unsigned char *p = (volatile unsigned char *)ptr; *p = 0;` is a volatile store.
  A store loop through that pointer cannot be removed or reordered. `memset` + an empty
  volatile asm with "memory" clobber is the C-compatible alternative; `explicit_bzero`
  (POSIX/glibc/bionic) and C23 `memset_explicit` are standard-named wrappers with the same
  contract.
- **EXAMPLE** (bad): `void wipe(char *p, size_t n) { memset(p, 0, n); }` — the call is
  kept when `p` may be observed by the caller (verified: GCC emits `jmp memset`), but the
  guarantee comes from "may be observed", which is fragile: once the buffer provably dies
  (local, or LTO sees no later use) the call is elided. Never rely on the callee's luck.
- **COUNTEREXAMPLE** (good):
  ```c
  void secure_zero_memory(void *p, size_t n) {
      volatile unsigned char *v = (volatile unsigned char *)p;
      while (n--) *v++ = 0;
  }
  ```
  Verified: emits an inlined `movb $0, (%rax)` loop at `-O2`, present after the
  `verify_key` call in `login_check_good`.
- **VERIFICATION**: `gcc -O2 -S examples/good/zeroize_good.c` — the wipe loop survives;
  objdump/`-S` shows store instructions, not an empty body.
- **SOURCE**: N1570 §6.7.3, §5.1.2.3; gcc-manual (volatile, inline asm constraints);
  cppreference-c-behavior (volatile); C23 N3096 (`memset_explicit`, §7.24).

## 3. asm-level verification that secrets do not survive

- **RULE**: "cleared" is a claim about the BINARY, not the source. Verify by inspecting
  `-O2 -S` asm: the secret's stack slot or heap range must be overwritten by stores or a
  wipe call, and no register/stack copy of the secret may outlive the wipe.
- **WHY AI GETS IT WRONG**: "I wrote `memset`, and my test passed, so it's wiped" — a
  functional test cannot observe residual memory contents.
- **CORRECT REASONING**: the optimizer moves values into registers and can keep a copy in
  a register even after the stack slot is wiped; with `-O2` the secret may be spilled,
  folded, or duplicated. The asm is the ground truth. Known residual-surface categories:
  the stack slot (unwiped or re-zeroed by an ABI-visible store), callee-saved registers
  used for the secret, and the red-zone/local copies. Full guarantee also requires wiping
  copies and not leaking via debug info (DWARF locations) or `-g`.
- **EXAMPLE** (bad): `wipe_param_kept` loads the secret byte into `%ebx` (callee-saved),
  wipes, then returns `%eax` derived from it — fine, but if the wipe were removed the
  stack slot would retain it; an unwiped local plus a spilled value demonstrates the
  register/stack residual issue (see `examples/bad/zeroize_bad.c`).
- **COUNTEREXAMPLE** (good): inspect the asm for the wipe function and confirm every store
  of the secret is followed by a zero-store or that the value is never materialized in a
  callee-saved register that survives. `check_rust_asm`-style scripts (trailofbits
  zeroize-audit) automate this on Rust/C asm.
- **VERIFICATION**:
  ```
  gcc -O2 -S file.c -o file.s
  rg -n "memset|movb \$0|stos|pxor|xor" file.s   # wipe instructions present
  # and confirm no branch/call between last secret use and the wipe that could be skipped
  ```
- **SOURCE**: empirical GCC 16.1; sysv-amd64-abi (callee-saved registers); godbolt-compiler
  (asm as ground truth); dwarf-v5 (debug info residual, §debug_line/location lists).

## 4. Constant-time: no secret-dependent branches, indexes, or early exits

- **RULE**: secret-dependent control flow and secret-dependent memory addressing create
  timing side channels. Timing must be independent of the secret VALUE. The three banned
  constructs: (a) a branch whose condition is derived from secret bytes, (b) a memory load
  whose address is derived from secret bytes (cache timing), (c) an early-exit loop that
  returns at the first differing byte.
- **WHY AI GETS IT WRONG**: "my `memcmp` loops over all bytes anyway" — a naive
  `for (i...) if (a[i] != b[i]) return 0;` returns as soon as a difference is found; the
  RUNTIME leaks the position of the first difference. Also "the compiler turns `==` into
  `sete`, so it's constant-time" — that is GCC's choice for a trivial case; other compilers
  or more complex bodies keep the branch. You must not RELY on the compiler's conversion.
- **CORRECT REASONING**: build constant-time from the source: XOR-accumulate all bytes
  into a single `diff` (`diff |= a[i] ^ b[i];`) with a fixed trip count, then a SINGLE
  comparison on `diff` after the loop. The per-byte work is data-independent; the only
  secret-free result test is `diff == 0`. For selection, use bitwise arithmetic
  (`mask = 0u - (cond != 0); (a & mask) | (b & ~mask)`) or `cmov`-style idioms — no branch
  on the secret. For indexing, never index a table with a secret (this is why S-box lookups
  in crypto are a classic cache-timing channel); use precomputed constant-time selection.
- **EXAMPLE** (bad): `ct_bad` early-exit compare at `-O2`:
  ```
  movzbl  (%rdx,%rax), %r9d
  cmpb    %r9b, (%rcx,%rax)   # secret bytes compared
  je      .L10                # BRANCH on secret data — timing leaks byte position
  xorl    %eax, %eax
  ret
  ```
- **COUNTEREXAMPLE** (good): `ct_good` accumulator compare at `-O2` vectorizes to
  `pxor`/`por` accumulation and a single final `testb %r9b,%r9b; sete %al` — no per-byte
  branch on secret data.
- **VERIFICATION**: `gcc -O2 -S` and inspect: the constant-time loop body must contain no
  conditional jump that depends on loaded secret bytes; the only secret-derived branch (if
  any) must be the single final result test. `ctgrind`/`dudect` are the standard statistical
  validators.
- **SOURCE**: carruth-gigo (optimizer freedom); godbolt-compiler (asm inspection); CWE-208
  (observable timing discrepancy); cert-c MSC13-C (sensitive data clearing).

## 5. Compiler-induced timing side channels (bitwise vs branch)

- **RULE**: even branchless-looking C can produce a branch, and branchy C can become
  branchless — the side channel is decided by the GENERATED CODE, so verify per compiler
  and per optimization level. Comparisons (`==`, `!=`) on secret values are the classic
  source: `secret == guess` compiled by GCC 16.1 at `-O2` to `cmpl` + `sete` (branchless),
  while the same comparison inside a loop with early exit compiled to `cmpl` + `jne`
  (a branch). Compilers also emit secret-dependent branches when the body is complex, when
  the value feeds a jump table, or under different targets (e.g. AArch64 may differ).
- **WHY AI GETS IT WRONG**: "GCC 16.1 made my `if (x == secret)` into `sete`, so it's
  constant-time on all compilers" — false; that is one compiler's optimization choice for
  one shape, not a language guarantee.
- **CORRECT REASONING**: write code that is constant-time WITHOUT the compiler's help:
  no secret-dependent `if`/`?:`/loop-exit in the source, and no secret-dependent indexing.
  Then, regardless of whether the compiler keeps or removes branches, the source already
  guarantees data-independence. Use bitwise arithmetic for selection and accumulation for
  comparison.
- **EXAMPLE** (bad): `if (secret == guess) return 0xdeadbeef; return guess & 0xff;`
  compiled to `cmove` here — but the identical pattern with a more complex then-branch
  keeps a real `je`. The source is not constant-time by construction.
- **COUNTEREXAMPLE** (good): `ct_select`: `mask = 0u - (cond != 0); return (a & mask) |
  (b & ~mask);` → `cmovne` at `-O2`, and even if the compiler emitted branches, no secret
  feeds a control-flow decision with data-dependent timing in the intended sense.
- **VERIFICATION**: compile the same source at `-O0/-O1/-O2/-O3` and with
  `gcc` vs `clang`, diff the asm for branches; also run under `ctgrind` (Valgrind-based
  cache/timing instrumentation).
- **SOURCE**: empirical GCC 16.1 (this reference); clang-docs (optimizer behavior); CWE-208;
  carruth-gigo.

## 6. Rust `zeroize` crate concepts (Zeroize trait)

- **RULE**: Rust does not automatically wipe memory on drop; `Box<T>`/`Vec<T>` leave the
  heap contents in place after free, and the stack copy of a `Copy` value survives. The
  `zeroize` crate provides a `Zeroize` trait (an `fn zeroize(&mut self)` that overwrites
  the value in place) and a `ZeroizeOnDrop` marker so that dropping the value also wipes it
  — an explicit wipe wrapped in a `Drop` implementation, with the volatile-write technique
  under the hood.
- **WHY AI GETS IT WRONG**: "Rust is memory safe, so secrets are handled for me" — safety
  is about correctness of memory access, not about erasing secret contents; `Drop` for
  ordinary types frees memory without clearing it, and `Copy` duplicates the value freely.
- **CORRECT REASONING**: think of `zeroize()` as the explicit wipe primitive, and
  `ZeroizeOnDrop` as RAII for the wipe. Where Rust C allows `memset`, Rust requires the
  crate (or a manual volatile loop) because the language has no standard wipe builtin; the
  trait simply makes the wipe discoverable and compositional (arrays, `String`, slices
  implement it). Like the C case, the guarantee holds only if the wipe survives
  optimization — the crate's implementation is the volatile-sink pattern, and `-O`/LTO
  must not remove it. Described per the crate's public API documentation; this reference
  does not copy crate source.
- **EXAMPLE** (bad): a key struct with a `String`/`Vec<u8>` field that is simply dropped —
  the heap buffer holding the key is returned to the allocator without being cleared.
- **COUNTEREXAMPLE** (good): derive/implement `Zeroize` (+ `ZeroizeOnDrop` where drop
  should wipe) and call `secret.zeroize()` explicitly after each use; for FFI buffers,
  wipe through a volatile pointer (as §2) before `free`.
- **VERIFICATION**: run the code under Miri/TSan for the write pattern; inspect the release
  asm (`cargo asm` or `rustc -O --emit asm`) for the wipe stores; the crate's own audit
  (`zeroize-audit`) scripts check x86/aarch64 asm.
- **SOURCE**: rust-reference (traits, Drop, unsafe); rustonomicon (Drop/RAII, FFI);
  carruth-gigo (optimizer freedom applies equally).

## 7. Clearing heap memory and whole-object coverage

- **RULE**: `free()`/`Box::drop`/`Vec` deallocation does NOT clear; heap memory must be
  wiped before release. Wiping must cover the whole object (padding, adjacent copies),
  not just the field the code "uses".
- **WHY AI GETS IT WRONG**: "after free the allocator owns it, so it's gone" — the
  allocator hands the block to the next `malloc`; a later attacker or another process
  (after `fork`, or via allocator reuse) can read it.
- **CORRECT REASONING**: wipe exactly the allocated range before `free` (C), or rely on
  `ZeroizeOnDrop` for owned heap values (Rust). For C structs with padding, wiping
  `sizeof(struct)` covers padding; for multiple copies, wipe each.
- **EXAMPLE** (bad): `free(secret_ptr);` without a preceding wipe; or
  `memset(&secret, 0, sizeof secret->key);` leaving the rest of the struct untouched.
- **COUNTEREXAMPLE** (good): `secure_zero_memory(secret_ptr, secret_len); free(secret_ptr);`
  with `secret_len` = allocated size.
- **VERIFICATION**: after free, read the block via a fresh `malloc` in a test (or check
  with a debug allocator that fills freed blocks); for stack, inspect asm.
- **SOURCE**: N1570 §7.22.3 (allocation/free), §5.1.2.3; CWE-244/212 (improper clearing of
  sensitive information before storage/release); rustonomicon (ownership/drop); cppreference-c-behavior.

## Quick reference table

| Problem | Bad | Good |
|---|---|---|
| wipe a local buffer | `memset(s,0,n)` before return | `secure_zero_memory`/volatile loop |
| wipe a heap buffer | `free(p)` only | wipe then `free(p)` |
| compare secrets | early-exit `memcmp` loop | XOR-accumulate, single final test |
| select on secret | `if (secret) return a; return b;` | bitwise mask / `cmov` idiom |
| index with secret | `table[secret]` | no secret-dependent addressing |
| Rust | drop a `String`/`Vec` with a key | `Zeroize` + `ZeroizeOnDrop` |
