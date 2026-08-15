# Evidence — what was actually executed

This page aggregates the recorded, reproducible verification outputs behind the
"source-backed" claim. Every fact below was produced by running real toolchains on a
Windows/MSYS2 host: **GCC 16.1 (as/ld/objdump 2.46, GDB 17.2), rustc/cargo 1.97.1,
Python 3.11, CMake 4.4, Ninja 1.13**. Exact reproduction commands are in each skill's
`evals/README.md`.

## Assembly verification (asm-verification-hallucination-gate)

- `movqad` (an invented instruction) → assembler **exit 1**, `Error: no such instruction`.
- `imul eax, eax, 38` assembles to `69 c0 00 00 00 00` — the immediate `38` is **dropped**
  by the parser (the bug class behind BBoeOS PR#584).
- `mov (%rax), %eax` assembles to `8b 00`; `movqad` vs real `movq %rax, %rax` (`48 89 c0`)
  confirmed by `objdump -d` byte comparison.
- GAS vs Intel: `-masm=intel` inverts operand order (`mov eax, [rax]`); mixing dialects
  silently changes semantics.

## Concurrency (concurrency-actual-parallelism-detection)

- Fake-parallelism demo: thread-safe-looking program ran on **1 thread**,
  wall-clock **1.206 s**, `max_working = 1`.
- Real `pthread` split: **4 threads**, wall-clock **0.304 s**, `max_working = 4`.
  Gate: count live threads + measure wall-clock scaling, not syntax.

## Rust (rust-*)

- `rust-api-evolution-and-drift`: stale API usage → real `E0614` / `E0133` errors on
  rustc 1.97.1; `#[deprecated]` warning reproduced.
- `rust-dependency-supply-chain`: `cargo info <hallucinated-crate>` → **exit 101**
  (not found); Levenshtein comparison flags near-miss names (`serde-json` vs `serde_json`).
- `rust-crypto-primitives-safety`: pure-Rust ChaCha20 block function **passes** RFC 8439
  §2.3.2 known-answer vectors; nonce-reuse detector flags the reuse.
- `rust-unsafe-safety-contract-verification`: fake SAFETY comment (no PhantomData) →
  real borrow-check **E0597**; a type-enforced invariant compiles.

## Build systems (build-*)

- `build-system-cmake-diagnostics`: a broken `find_package` project fails `cmake -G Ninja`;
  the target-graph diagnosis (`cmake --graphviz`) identifies the missing link line.
- `build-toolchain-version-drift`: same source compiled with different `-std=` and `-O`
  levels produces **different binaries** (`cmp` differs); `gcc --print-file-name`
  resolves the toolchain paths.
- `build-linker-error-diagnostics`: undefined reference reproduced and traced with
  `nm`/`readelf -s` (missing vs wrong-mangled symbol distinction).

## Debugging (debugging-*)

- `debugging-crash-triage-discipline`: gdb backtrace pinpoints the real caller chain:
  `ucrtbase!strlen <- print_owner(0x0) <- show <- main` — crash site ≠ root cause.
- `debugging-instrumentation-over-reasoning`: a file log reveals corruption at
  **iteration 11** that pure reasoning missed; the fix verified by re-run.

## Side channels (side-channel-constant-time-verification)

- `gcc -O2`, 500 000 iterations × 256-byte buffers: **early-exit memcmp 0.054 s** vs
  **constant-time compare ~0 s** (not statistically distinguishable) — the timing leak
  is real and measurable on this host.
- Counterexample argument for CVE-2026-22705 (ML-DSA UDIV/SDIV timing) documented.

## OpenMP (hpc-openmp-parallel-programming)

- `gcc -fopenmp` reduction correct at **1/8/12 threads**; a `shared`-variable race
  produces a wrong sum (`174763/131072` vs `1048576`) — caught by the race gate.

## Meta (meta-verification-harness-validity)

- Ablation gate: a harness returning `0` unconditionally masks the broken function
  (**exit 0**); an assert-based harness aborts (**0xC0000409**) on the same input.
  "The harness must fail when the target is broken."

## Toolchain and caveats

- 59 skills are honestly marked `researched`: the toolchain is absent on this host
  (zig, nasm, clang-cross, qemu, nvcc, mpicc, valgrind, verilator, jadx/frida,
  frama-c/cbmc/kani, z3, Linux kernel). Their `evals/README.md` lists the exact
  command that would elevate them to source-backed on a suitable host.
- Reproduce everything with `python tools/validate.py` (structure gates) and the
  per-skill commands in each `evals/README.md`.
