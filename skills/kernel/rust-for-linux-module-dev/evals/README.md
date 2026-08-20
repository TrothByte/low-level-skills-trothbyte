# Evaluation — rust-for-linux-module-dev

Skill: `skills/kernel/rust-for-linux-module-dev`. Stability: `researched`
(in-tree module build needs a Linux kernel tree; host-verified core is the
python checker + rustc models recorded below). Toolchain: rustc 1.97.1
(8bab26f4f 2026-07-14), Python 3.11.9, Windows x86_64-msvc. Host evidence
was produced on 2026-08-20 by running the examples in this skill.

## Verified facts (host, recorded 2026-08-20)

Real rustc + python checker output:

```
rustc --edition 2021 --crate-type lib --crate-name standalone_contract examples/good/standalone_contract.rs -o build/standalone_contract.rlib
  exit 0, zero warnings — the no_std kernel-style contract model compiles on host

rustc --edition 2021 examples/bad/params_misuse.rs -o build/params_misuse.exe && build/params_misuse.exe
  exit 0, prints "host run OK: pushed byte 0xAA into my_mod ring buffer"
  — the model of the mistakes RUNS on the host; that is the trap

rustc --edition 2021 examples/bad/unsafe_safety_contract.rs -o build/unsafe_safety_contract.exe && build/unsafe_safety_contract.exe
  exit 0, prints "read 42 from raw pointer" — a missing-SAFETY raw deref is
  accepted by rustc AND would be accepted by a real kernel build

python examples/tools/module_contract_check.py examples/bad/params_misuse.rs examples/bad/unsafe_safety_contract.rs examples/good/standalone_contract.rs
  exit 1 (flags expected):
  FLAG  bad/params_misuse.rs:19 std:: usage - kernel links only core; use core:: or kernel::alloc
  FLAG  bad/params_misuse.rs:20 std:: usage - kernel links only core; use core:: or kernel::alloc
  FLAG  bad/params_misuse.rs:21 std:: usage - kernel links only core; use core:: or kernel::alloc
  FLAG  bad/params_misuse.rs:53 unwrap( would panic in module code (kernel oops)
  FLAG  bad/unsafe_safety_contract.rs:19 unsafe block without a preceding // SAFETY: comment
  FLAG  bad/unsafe_safety_contract.rs: missing module declaration (kernel::module!)
  CLEAN good/standalone_contract.rs

python examples/good/api_contract_model.py
  exit 0, prints "PASS: params-from-module, GFP context, errno errors, lock context"

python examples/bad/alloc_context_misuse.py
  exit 1, prints two FAIL lines (GFP_KERNEL in interrupt context;
  panic-style abort instead of Error propagation)
```

Source grounding fetched 2026-08-20: docs.kernel.org Rust docs
(general-information, coding-guidelines, quick-start; doc version 7.2.0) and
rust.docs.kernel.org kernel 7.2 (`kernel::alloc` re-exports Box/KBox/Vec/KVec,
`Flags` + flags module; `kernel::sync` re-exports sleepable `Mutex` and
`SpinLock`; `kernel::error::Error` is errno-backed with `from_errno` and
`code::` constants). Note: `https://docs.kernel.org/rust/rust-for-linux.html`
returned 404 on 2026-08-20 (the overview moved); the canonical URL is kept in
SKILL.md per the repository template, and the current entry point is
docs.kernel.org/rust/.

## Synthetic evals

| Case | Fixture | Expected | Recorded |
|---|---|---|---|
| easy/negative | `bad/params_misuse.rs` | std types + unwrap + global params flagged | 4 flags (3x std::, 1x unwrap) |
| easy/negative | `bad/unsafe_safety_contract.rs` | missing SAFETY + missing module! flagged | 2 flags |
| easy/positive | `good/standalone_contract.rs` | compiles clean, checker CLEAN | rustc exit 0, 0 flags |
| medium/positive | `good/api_contract_model.py` | contract model holds | PASS, exit 0 |
| medium/negative | `bad/alloc_context_misuse.py` | GFP-in-atomic + panic-style errors | 2 FAIL lines, exit 1 |

Detection rules: `std::`/`unwrap(`/`expect(` in non-comment code, unsafe
blocks without a `// SAFETY:` marker in the comment block above, and a
missing `module!` declaration. The good model keeps every unsafe block under
a multi-line SAFETY comment (kernel style puts `SAFETY:` on the first line),
so the checker's comment-block scan is exercised.

## False-positive evals (correct code must NOT be flagged)

- `good/standalone_contract.rs` — no_std, `core::`/`kernel::` paths only,
  SAFETY comments on every unsafe block, `module!` present: zero flags.
  The file's comments name `std::sync::Mutex` and `unwrap()` as anti-patterns;
  comment/string stripping keeps that documentation from producing flags.
- Code that documents the real kernel pattern in comments (`kernel::module!`,
  `#[unsafe(no_mangle)]`) must not be flagged merely for containing those
  tokens — only the module! MISSING check may reference the raw text.
- A valid SAFETY comment on a plain `ptr::read`-style block must not be
  flagged just because it is unsafe.

## Historical evals

Rust-for-Linux upstream API changes since the 6.1 initial merge (directions
KNOWN from kernel docs/source; exact per-release versions UNVERIFIED without
a checkout — verify against `rust/kernel/*.rs` in the target tree):

- `kernel::module!` redesigns: parameter descriptors in `param.rs` and the
  `module.rs` macro internals were reworked repeatedly after 6.1.
- `kernel::sync::Mutex`: spinlock-backed at the 6.1 merge; the lock redesign
  split it into a sleepable `Mutex` (wrapping C `struct mutex`) plus a new
  `SpinLock` for atomic context. Code written for one era misbehaves or
  fails to compile on another — this is a core drift lesson of the skill.
- Allocation flags: `GfpFlags` -> `kernel::alloc::Flags`; `Box`/`Vec`
  re-exported under `kernel::alloc` (kbox/kvec) with fallible, flag-taking
  constructors.
- Attributes: `#[no_mangle]` -> `#[unsafe(no_mangle)]` (edition-2024 unsafe
  attributes). Lock construction moved to init macros/classes
  (`new_mutex!`, `new_spinlock!`, `global_lock!`).

## Adversarial evals

- A fabricated SAFETY comment that names no real mechanism (comment present,
  contract false) PASSES the static checker — the checker verifies presence,
  not truth. Routing must hand this off to
  `rust-unsafe-safety-contract-verification` (which audits the contract
  itself) instead of trusting the flag count.
- The bad rust models compile and run on the host with exit 0: an agent that
  treats "compiles and runs on my machine" as a kernel build passes
  host-runnable code. The eval: explain that `make LLVM=1` against the
  target tree is the only authoritative build, and that the bad files would
  be rejected there (std unavailable; global params emit no descriptors;
  panic paths are oopses).
- `alloc_context_misuse.py` models the "GFP_KERNEL in IRQ handler while
  spinlock held" class: host output is just printed FAIL lines, but the
  kernel failure is a "scheduling while atomic" oops — an agent must connect
  the model to the real failure mode.

## Verification commands (target — Linux kernel build)

Documented, NOT run on this Windows host; requires a Linux host with the
target kernel tree:

```
make LLVM=1 modules_prepare
make M=drivers/my_mod LLVM=1
modinfo drivers/my_mod/my_mod.ko
sudo insmod drivers/my_mod/my_mod.ko
dmesg | tail
sudo rmmod my_mod
```

Toolchain checks (target): `make LLVM=1 rustavailable`, `rustup override set
stable` inside the tree, `rustup component add rust-src`, bindgen installed
(quick-start).

## Scoring

- precision: every flag maps to a stated rule (std::, panic paths, SAFETY
  presence, module! presence) — no rule fires on the good model.
- recall: std imports, unwrap/expect, missing SAFETY, missing module!
  declaration all detected on the bad fixtures.
- FP-rate: the good model and its comment-documented anti-patterns produce
  zero flags; multi-line SAFETY comment blocks are handled by the scan.
- Gap (documented): fabricated-but-present SAFETY contracts are out of scope
  for the static checker — routed to `rust-unsafe-safety-contract-verification`;
  a real module build and runtime behavior are target-only.
