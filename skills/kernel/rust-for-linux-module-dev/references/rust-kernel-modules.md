# Rust kernel modules — the `kernel` crate contract (deep reference)

Facts below are KNOWN when grounded in the kernel docs/source fetched on
2026-08-20 (docs.kernel.org 7.2.0; rust.docs.kernel.org kernel 7.2); items
that drift between releases are marked DRIFT, and anything I could not
verify is UNVERIFIED. Always re-check against the `kernel` crate in the
target kernel tree (`rust/kernel/*.rs`) — that is the skill's core lesson.

## 1. Toolchain and no_std (KNOWN)

- The kernel's Rust support can link only `core`, not `std`; crates opt in
  with `#![no_std]` (docs.kernel.org/rust/general-information.html).
- Build with `make LLVM=1`; `make LLVM=1 rustavailable` checks the
  toolchain requirements; `CONFIG_RUST` enables the support (quick-start).
- The kernel pins the toolchain via `rust-toolchain.toml`; you need
  `rust-src` (core is cross-compiled by Kbuild) and `bindgen` (generates
  the C bindings from `rust/bindings/bindings_helper.h`). Docs recommend a
  prebuilt LLVM+Rust from kernel.org/pub/tools/llvm/rust/.
- Extra tools: `make LLVM=1 rustdoc`, `make LLVM=1 CLIPPY=1`,
  `make LLVM=1 rustfmt`, `make LLVM=1 rust-analyzer` (quick-start,
  general-information). Clippy may change codegen — not for production.

## 2. Abstractions vs bindings (KNOWN)

- `bindings` = auto-generated (bindgen) Rust declarations of C kernel
  functions/types. `abstractions` = safe Rust wrappers in `rust/kernel/`.
- Design rule (general-information): leaf modules (drivers) should NOT use
  the C bindings directly; subsystems provide as-safe-as-possible
  abstractions. Raw `unsafe` against bindings belongs in `rust/kernel/`,
  reviewed once, not sprinkled through drivers.
- FFI types: use the kernel prelude's `c_int`/`c_char` aliases, not
  `core::ffi` (coding-guidelines) — they can differ.

## 3. Module declaration — `kernel::module!` (KNOWN shape, DRIFT in detail)

- The macro lives in `rust/kernel/module.rs` and expands into module
  metadata (MODULE_INFO: name, author, description, license) plus the
  init/exit glue. Parameters are declared inside the macro:
  `params: { name: ty { default = expr }, ... }`.
- Parameter handling lives in `rust/kernel/param.rs`; the set of supported
  parameter types and the descriptors have been redesigned repeatedly since
  the 6.1 merge. DRIFT: check `param.rs`/`module.rs` in your kernel.
- Entry/exit: `#[unsafe(no_mangle)] pub extern "C" fn init_module() ->
  kernel::error::Result<...>` and the matching `cleanup_module`. The
  `#[unsafe(no_mangle)]` form is the current one; older kernels/tutorials
  use `#[no_mangle]`. UNVERIFIED exact 7.2 macro keyword set — verify in
  source. Reference samples: `samples/rust/rust_minimal.rs`,
  `samples/rust/rust_parameters.rs` (quick-start points at them).
- Historical redesigns (for the Historical evals):
  - 6.1 initial merge: `kernel::module!` with `impl_module!` internals;
    params via `kernel::param::ParamSpec` machinery.
  - Later: parameter spec rework, `[unsafe(...)]` attribute migration
    (edition 2024), lock-class/key parameter support.
  - KNOWN direction, exact per-release details UNVERIFIED without a tree.

## 4. Error handling (KNOWN, from rust.docs.kernel.org 7.2)

- `kernel::error::Error` wraps a valid errno: invariant is
  `>= -MAX_ERRNO && < 0` (negative). `Error::from_errno(c_int)` constructs
  one; passing an out-of-range value is a bug and yields `EINVAL`.
- Methods: `to_errno()`, `to_ptr()`, `name()`. Named codes live under
  `kernel::error::code::*` (e.g. `code::EINVAL`).
- `From` impls for `AllocError`, `LayoutError`, `TryFromIntError`,
  `Utf8Error`, `kvec::PushError` etc. mean `?` composes across the kernel
  crate's fallible APIs.
- Panics: the coding guidelines say panicking should be very rare; prefer a
  fallible approach returning `Result`. In the kernel a panic is an oops —
  there is no process to kill, the whole system faults.

## 5. Allocation (KNOWN shape, 7.2 rustdoc)

- `kernel::alloc` re-exports `Box`/`KBox` (kbox), `Vec`/`KVec` (kvec),
  plus `VBox`/`KVBox` and `VVec`/`KVVec` for virtual-memory-resident
  buffers. Allocation is fallible and flag-aware (`kernel::alloc::Flags`,
  `flags` module, `AllocError`).
- Context rules (kernel-atomic-context): `GFP_KERNEL` sleeps — legal in
  process context only; `GFP_ATOMIC` is for atomic/IRQ context. DRIFT: the
  flag type was `GfpFlags` in earlier kernels; now `Flags` in
  `kernel::alloc`. Never allocate with a sleepable flag where preemption is
  disabled.
- The host model in `examples/good` mirrors the fallible,
  flag-taking shape so the contract is exercised without a kernel.

## 6. Synchronization (KNOWN shape, 7.2 rustdoc; DRIFT vs 6.1)

- `kernel::sync::Mutex` is an alias in `lock::mutex` wrapping the C
  `struct mutex` — sleepable, process context only. `kernel::sync::SpinLock`
  (`lock::spinlock`) wraps a spinlock — atomic context.
- 6.1-era kernels had a single `Mutex` that was spinlock-based; the lock
  redesign introduced the `Lock`/`Guard`/`LockInInterrupt` traits and split
  `Mutex` (sleepable) from `SpinLock`. DRIFT: code written against one
  kernel release may deadlock or fail to compile on another.
- Construction now goes through init macros/classes
  (`kernel::new_mutex!`, `new_spinlock!`, `static_lock_class!`,
  `global_lock!`) rather than a plain `Mutex::new`. Also available:
  `Arc`, `UniqueArc`, `Refcount`, `CondVar`, `SetOnce`, `LockedBy`,
  `GlobalLock` (7.2 rustdoc).
- `std::sync::Mutex` is doubly wrong: it requires std (absent) and its
  futex-based sleeping + poisoning model does not exist in the kernel.

## 7. Pinning and init (KNOWN direction, details DRIFT)

- Self-referential objects (e.g. an embedded list head pointing into the
  object's own fields) need the object pinned before pointers to it are
  taken; `&mut` lets the value move and invalidates the self-reference.
- Newer kernels build such objects through the pin-init machinery
  (`kernel::init`, `PinInit`/`Init` traits, `Pin::new_unchecked` under the
  hood, `UniqueArc::try_pin` etc.). 7.2 rustdoc shows blanket
  `Init<T>`/`PinInit<T>` impls — the pattern is core to current drivers.
- Rule of thumb: if the C side keeps a pointer INTO the Rust object (list
  heads, `wait_queue_head`, timer lists), the object must be pinned and
  never moved after init.

## 8. Building and loading (target; documented, not run here)

- In-tree or `M=` out-of-tree module build needs the target kernel's
  prepared tree: `make LLVM=1 modules_prepare`, then
  `make M=<moddir> LLVM=1`. The module is a `.ko` with the same vermagic /
  modversions binding as C modules (kernel-module-build-out-of-tree).
- `modinfo <mod>.ko` and `insmod`/`rmmod` behave like C modules; dmesg shows
  init/exit messages and panics. A panic in a Rust module produces the same
  kernel oops machinery (lockup detection, panic message from the Rust
  panic handler).

## 9. Drift checklist (what to verify against the target tree)

- `rust/kernel/module.rs`: macro keyword set, param descriptors.
- `rust/kernel/param.rs`: supported param types and their traits.
- `rust/kernel/alloc/`: `Flags` vs `GfpFlags`, `Box`/`KBox` vs bare names.
- `rust/kernel/sync/`: `Mutex` sleepability, `SpinLock` presence,
  `new_mutex!`/`global_lock!` macros, guard trait names.
- `rust/kernel/error.rs`: `from_errno` vs `from_kernel_errno` vs
  `from_posix_errno`, `code::*` constants.
- Attribute form: `#[unsafe(no_mangle)]` vs `#[no_mangle]`.
