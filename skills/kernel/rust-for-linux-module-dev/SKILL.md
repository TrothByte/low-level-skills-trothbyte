---
name: rust-for-linux-module-dev
description: Use when writing, reviewing, or debugging Rust modules for the Linux kernel using the kernel crate — module parameters, kernel::module, unsafe wrappers around C APIs, pinning, and error handling. Teaches in-kernel Rust that compiles against the kernel tree, not user-space Rust.
---

# Rust for Linux — module development with the kernel crate

## When to use

- Writing a new Rust module/driver for the kernel (CONFIG_RUST enabled):
  `kernel::module!` declaration, module parameters, init/exit functions.
- Reviewing an existing Rust module for contract violations: std types,
  sleeping locks in atomic context, panics, or fabricated SAFETY comments.
- Wrapping a C kernel API in safe Rust and getting the ownership, locking,
  and GFP/allocation contract right.
- Deciding whether to use a `kernel` crate abstraction or drop to raw
  bindings, and debugging why a module does not build or crashes.

## When not to use

- User-space Rust that talks to the kernel through syscalls/libc — use
  `rust-ffi-boundary` (different FFI rules: std is available there).
- General unsafe-Rust semantics (aliasing, provenance, validity) — use
  `rust-unsafe-reasoning` and `rust-unsafe-safety-contract-verification`.
- C kernel modules — use `kernel-ub-patterns` / `kernel-uaccess-safety`.
- Kbuild Makefile / vermagic / modinfo mechanics of building a module
  out-of-tree — use `kernel-module-build-out-of-tree`.
- Core locking/GFP context rules that apply to C code equally — those live
  in `kernel-atomic-context`; this skill applies them to the kernel crate.

## What the agent often gets wrong

1. Uses std types (`Vec`, `String`, `Box` from std). The kernel links only
   `core` — std is absent (`#![no_std]` required). The `kernel` crate
   re-exports the allocator types as `kernel::alloc::{Box, Vec, KBox, KVec,
   KString, ...}` (modules `kbox`/`kvec`), so a bare `use Vec;` resolves to
   std and fails to compile in the kernel.
2. Uses `std::sync::Mutex`. The kernel equivalents are `kernel::sync::Mutex`
   (wraps the C `struct mutex`, sleepable, process context only) and
   `kernel::sync::SpinLock` (atomic/IRQ context). `std::sync::Mutex` is
   user-space futex code that cannot exist in the kernel. Note the drift:
   the 6.1-era `Mutex` was spinlock-based; newer crates split it into
   sleepable `Mutex` + new `SpinLock`. Check your kernel's version.
3. Gets the module signature wrong. The entry point is `#[unsafe(no_mangle)]
   pub extern "C" fn init_module() -> kernel::error::Result<...>` plus the
   `kernel::module! { type: ..., name: ..., author: ..., license: ...,
   params: {...} }` declaration. Module parameters are declared INSIDE the
   `module!` block, not as global statics.
4. Reaches for panics. Panicking in module code is a kernel oops; there is no
   unwind recovery for other drivers. Errors propagate as
   `kernel::error::Error` (errno-backed) with `?`. `unwrap()`/`expect()` in
   module paths are bugs.
5. Fabricates SAFETY comments when wrapping C APIs. Every unsafe block needs
   a `// SAFETY:` comment naming the REAL contract: GFP context, pointer
   validity and lifetime, ownership transfer, lock state. A comment that
   names nothing is a lie the compiler cannot see.
6. Ignores allocation flags. Kernel allocations carry flags:
   `GFP_KERNEL` (sleepable, process context) vs `GFP_ATOMIC` (atomic/IRQ).
   The kernel-crate allocation API is fallible and flag-aware (see
   `kernel::alloc::Flags`); a bare allocator-style `Box::new` cannot express
   the context, so an "it works in Rust" habit breaks here.
7. Forgets pinning. Self-referential structures (e.g. an embedded linked-list
   head pointing into the object itself) require `Pin`; using `&mut` where a
   pinned pointer is required silently breaks the invariant. Newer kernels
   build such objects with the pin-init machinery (`PinInit`/`Init`).
8. Assumes the module compiles with rustc alone. A real module needs the
   kernel tree, Kbuild, and the `kernel` crate — the toolchain is
   `make LLVM=1` (rustup override pinned by the tree's rust-toolchain.toml).
## How to reason correctly

1. Everything comes from the `kernel` crate in YOUR kernel tree. Check the
   crate version that ships with the target kernel first — the API drifts
   fast (see `kernel-api-drift-migration`); rust.docs.kernel.org documents
   each released kernel separately.
2. Write the `module!` declaration with correct params and types first, then
   implement init/exit. The params in the block and the struct fields must
   match; the macro expands into metadata + param descriptors.
3. Treat every unsafe block around a C API as an FFI contract: GFP context,
   pointer validity/lifetime, ownership transfer, lock state — all encoded
   in the SAFETY comment. The kernel crate exists to make this contract
   type-enforced where possible; prefer its abstractions over raw bindings.
4. Prefer safe abstractions (`kernel::sync`, `kernel::alloc`,
   `kernel::str::CStr`, ...) over raw C calls; only wrap what has no safe
   equivalent, in a subsystem abstraction.
5. Use kernel error types with `?`. `Error` is a valid errno in
   `[-MAX_ERRNO, -1]`; never panic, never unwrap, in module code.
6. Verify every API name against the kernel crate source
   (`rust/kernel/*.rs`) before writing code — names change between releases
   (e.g. `GfpFlags` -> `Flags`, `Mutex` semantics, param macros).

## What to verify

- Module entry/exit symbols are correct (`init_module`/`cleanup_module` with
  `#[unsafe(no_mangle)]` and `extern "C"`); `module!` params match the
  declared fields and types.
- No std imports; no `unwrap()`/`expect(` in module paths; every unsafe
  block is preceded by a real `// SAFETY:` comment that names a mechanism.
- Locking and allocation discipline matches the execution context: sleepable
  (`Mutex`, `GFP_KERNEL`) only in process context; `SpinLock`/`GFP_ATOMIC`
  in atomic/IRQ context.
- The module compiles against the target kernel tree (`make LLVM=1`) with
  the toolchain the tree pins (rust-toolchain.toml), and the C FFI types
  come from the kernel prelude (`c_int` etc.), not `core::ffi`.

## How to verify

Host-side checks (this machine; no kernel tree needed):

```
python examples/tools/module_contract_check.py examples/bad/params_misuse.rs examples/bad/unsafe_safety_contract.rs examples/good/standalone_contract.rs
rustc --edition 2021 --crate-type lib --crate-name standalone_contract examples/good/standalone_contract.rs -o build/standalone_contract.rlib
rustc --edition 2021 examples/bad/params_misuse.rs -o build/params_misuse.exe && build/params_misuse.exe
rustc --edition 2021 examples/bad/unsafe_safety_contract.rs -o build/unsafe_safety_contract.exe && build/unsafe_safety_contract.exe
python examples/good/api_contract_model.py
python examples/bad/alloc_context_misuse.py
```

The bad models compile AND run on the host — that is exactly the trap: host
runnable Rust is not kernel-runnable Rust. The static checker still flags
them; the real kernel build would reject them.

Target-side verification (documented; Linux host with the kernel tree — not
run on this machine):

```
make LLVM=1 modules_prepare
make M=drivers/my_mod LLVM=1
modinfo drivers/my_mod/my_mod.ko
sudo insmod drivers/my_mod/my_mod.ko
dmesg | tail
sudo rmmod my_mod
```

## Where the knowledge comes from

- Rust for Linux — kernel documentation (https://docs.kernel.org/rust/rust-for-linux.html, https://rust-for-linux.github.io/)
- Linux kernel rust coding guidelines (https://docs.kernel.org/rust/general-information.html)
- Linux kernel source: rust/kernel/ (module.rs, param.rs)
- Rust Reference (https://doc.rust-lang.org/reference/)
- Linux kernel module documentation (https://docs.kernel.org/kbuild/modules.html)

## Related skills

- `rust-ffi-boundary` — FFI layout/contract patterns across the C edge
  (recommend)
- `rust-unsafe-reasoning` — the unsafe semantics SAFETY comments must cite
  (recommend)
- `rust-unsafe-safety-contract-verification` — checking that a SAFETY
  contract is real, not fabricated (recommend)
- `kernel-module-build-out-of-tree` — Kbuild/vermagic/insmod of the built
  module (cross-link)
- `kernel-uaccess-safety` — copying data to/from user space from module code
  (cross-link)
- `kernel-atomic-context` — GFP and sleeping rules that decide Mutex vs
  SpinLock (cross-link)
- `kernel-api-drift-migration` — kernel-crate API drift across releases
  (cross-link)

## Evaluation

- Synthetic: the checker must flag `std::` usage, `unwrap(`/`expect(`, an
  unsafe block without a SAFETY comment, and a missing `module!`
  declaration; the bad rust models must compile and run on the host and be
  flagged; the good model must compile and be clean.
- False-positive: a correct module-shaped file (core/kernel paths only,
  SAFETY comment on every unsafe block, `module!` present) must produce zero
  flags; GFP-context and sleepability reasoning must not reject valid code.
- Historical: the Rust-for-Linux 6.1 initial merge vs later redesigns of
  `kernel::module`, `kernel::sync::Mutex` (spinlock -> sleepable + new
  `SpinLock`), and the alloc flags rename (`GfpFlags` -> `Flags`).
- Adversarial: a fabricated SAFETY comment that passes the static checker
  (comment present, contract false) — the checker cannot disprove it;
  routing must hand off to `rust-unsafe-safety-contract-verification`.
- Commands and recorded results: `evals/README.md`.
