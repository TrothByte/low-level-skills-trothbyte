# Evaluation — memory-ordering-reasoning

Skill: `skills/concurrency/memory-ordering-reasoning`. Stability target: `evaluated`.

## Adversarial evals (core)

- **AD-01 (compiles but wrong ordering)**: "flag + data" publish/consume protocol with
  `Relaxed`. It compiles and passes naive tests. Agent must: identify the missing
  synchronizes-with edge, classify as data race (UB), fix to `Release`/`Acquire`, and
  explain the asm difference (x86: `mov` vs `xchg`).
- **AD-07 (sanitizer passes but concurrency bug)**: ASan-clean lock-free code with a wrong
  ordering — agent must know ASan does NOT detect races; TSan/Miri is required.

## Synthetic evals

- **easy/negative**: stats counter with `Relaxed` — must NOT be flagged as a bug (legitimate use).
- **medium/negative**: publish protocol missing the acquire on the reader — must flag.
- **hard/negative**: CAS loop (SPSC ring) with wrong order — must reason through the edges.
- **ambiguous**: `SeqCst` where `Release`/`Acquire` suffices — must suggest weakening but mark
  as OPTIONAL (not a bug).

## False-positive evals

- Correct `Release`/`Acquire` pair — must NOT be "strengthened" to `SeqCst`.
- A `Relaxed` atomic counter — must NOT be flagged.
- `volatile` used for MMIO (single-consumer hardware register) — must NOT be flagged as
  inter-thread sync misuse (different context; refer to `embedded-volatile-and-memory-ordering`).

## Verification fixtures

- `examples/bad/ordering_snippets.c` (relaxed flag, non-atomic counter, volatile sync)
- `examples/good/ordering_snippets.c` (release/acquire protocol, atomic counter)
- Rust asm verification:
  ```
  rustc --crate-type=lib --emit=asm -C opt-level=2 atomics.rs
  ```
  Confirmed with rustc 1.97.1 x86-64: `store_relaxed` → `mov`; `store_seqcst` → `xchg`;
  `load_acquire` → `mov`; `fetch_add` (Relaxed and SeqCst) → `lock xadd`.

## Scoring

- detection: names the missing edge and the UB class (data race).
- reasoning: explains synchronizes-with pairing, not just "use stronger ordering".
- fix: minimal correct ordering change; does not over-strengthen to SeqCst blindly.
- verification: cites/uses TSan/Miri or asm evidence.
