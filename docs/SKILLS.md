# Low-level skills TrothByte — Skill Catalog

All **163 skills** in one place. 85 are source-backed (verified with real toolchains); the rest are researched and honestly marked. For orientation by domain, see the domain READMEs under `skills/`; for triggers and rules, open each skill's `SKILL.md`.

**Stability:** `source-backed` = claims verified by execution (compilers, sanitizers, asm, debuggers); `researched` = content grounded in primary sources but requiring a toolchain not available in this repository's dev environment.

## Languages & semantics

### c

C's sharp edges: undefined behavior, integer promotion, strings and buffers, errno, signal handlers.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `c-errno-and-syscall-returns` | Use when writing or reviewing C code that calls libc or system calls which report errors through errno and negative return values — read, write, accept, connect, open, close, strtol. Covers errno discipline, EINTR retry loops, partial read/write handling, EOF detection, and fd validation. | common | source-backed | `skills/c/c-errno-and-syscall-returns` |
| `c-integer-promotion-and-conversion` | Use when writing or reviewing C arithmetic where signed/unsigned mixing, integer promotion, narrowing, or size_t vs int conversions can cause wrong results or overflow — comparisons, array sizes, allocation sizes, and length calculations. Teaches the usual arithmetic conversions and the classic wrap surprises. | improved | source-backed | `skills/c/c-integer-promotion-and-conversion` |
| `c-signal-handler-safety` | Use when writing or reviewing C code that installs or runs signal handlers — SIGINT/SIGTERM shutdown, async flag set, EINTR handling, crash handlers. Covers async-signal-safe functions, volatile sig_atomic_t, sigaction vs signal, self-pipe trick, and signal behavior in multithreaded programs. | common | source-backed | `skills/c/c-signal-handler-safety` |
| `c-string-and-buffer-safety` | Use when writing or reviewing C code that copies strings or fills buffers — strncpy, snprintf, strcpy, strcat, memcpy/memmove, sizeof on arrays vs pointers, _FORTIFY_SOURCE, or anything that can overrun or fail to NUL-terminate a buffer. | common | source-backed | `skills/c/c-string-and-buffer-safety` |
| `c-undefined-behavior` | Use when writing, reviewing, or debugging C code where undefined behavior (UB) may be present — signed overflow, out-of-bounds access, uninitialized reads, invalid pointers, shift/aliasing violations, or when a program behaves differently across optimization levels or compilers. Teaches the J.2 UB taxonomy and how to detect each class. | improved | source-backed | `skills/c/c-undefined-behavior` |

### cpp

Object lifetimes, move semantics, and positive RAII/descriptor-type API design.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `cpp-move-semantics` | Use when writing, reviewing, or debugging C++ code involving rvalue references, std::move and std::forward, move constructors and assignment, moved-from state and use-after-move bugs, copy elision and NRVO, the rule of five, or returning objects by value. | common | source-backed | `skills/cpp/cpp-move-semantics` |
| `cpp-object-lifecycle` | Use when writing, reviewing, or debugging C++ object lifetime issues: constructor/destructor order, virtual calls during construction, static initialization order fiasco, copy vs move semantics, use-after-move, destructor exceptions, dangling references and pointers, and basic.life violations. | common | source-backed | `skills/cpp/cpp-object-lifecycle` |
| `raii-descriptor-types-api-design` | Use when designing NEW C/C++/Rust APIs that wrap resources — file descriptors, sockets, buffers, handles, locks. Teaches positive design patterns: descriptor/newtype types, RAII ownership, typed errors, builders, and debug-asserted preconditions, so the type system makes misuse hard. | unique | source-backed | `skills/cpp/raii-descriptor-types-api-design` |

### rust

Unsafe semantics, FFI boundaries, and panic safety — the three places Rust can still be wrong.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `rust-api-evolution-and-drift` | Use when writing Rust code that must compile against a specific toolchain: API signatures drift between Rust versions and editions, methods get deprecated or change meaning. Prevents generated code that references removed or renamed APIs, edition-2024 unsafe changes, and stale deprecations. | unique | source-backed | `skills/rust/rust-api-evolution-and-drift` |
| `rust-crypto-primitives-safety` | Use when writing, reviewing, or auditing Rust cryptography: selecting AEAD primitives, nonces, key handling, or hand-rolled ciphers. Prevents nonce reuse, invented algorithms, and API hallucination — 57% of LLM-compiled crypto is vulnerable. | unique | source-backed | `skills/rust/rust-crypto-primitives-safety` |
| `rust-dependency-supply-chain` | Use when choosing or adding a dependency: crate names are hallucinated at 5.2-21.7%, typosquats and near-misses abound. Teaches exact-name verification (cargo info, crates.io API), Levenshtein near-miss checks, cargo-deny/audit, and minimal version pinning. | unique | source-backed | `skills/rust/rust-dependency-supply-chain` |
| `rust-ffi-boundary` | Use when writing or reviewing Rust FFI code — repr(C) layout, enum discriminants, CString/CStr and Box::into_raw ownership, extern "C" callbacks, opaque handles, or panic/unwind at the boundary. Teaches Rust-specific rules for safe C interop and how to verify layout and ownership on both sides. | improved | source-backed | `skills/rust/rust-ffi-boundary` |
| `rust-panic-safety` | Use when writing, reviewing, or debugging Rust code where a panic may be reachable — unwrap/expect on untrusted input, unwinding through extern "C" exports, catch_unwind boundaries, RefCell/Mutex poisoning, Drop during unwind, or panic=abort vs panic=unwind. Teaches panic reachability and unwind discipline as a single reasoning model. | improved | source-backed | `skills/rust/rust-panic-safety` |
| `rust-unsafe-reasoning` | Use when writing, reviewing, or debugging Rust code that uses unsafe blocks — raw pointers, transmute, MaybeUninit, Box::from_raw/into_raw, unsafe impl Send/Sync, or FFI-adjacent code — to reason about validity invariants, aliasing, and pointer provenance, and to detect undefined behavior that compiles and runs but is wrong. | improved | source-backed | `skills/rust/rust-unsafe-reasoning` |
| `rust-unsafe-safety-contract-verification` | Use when auditing unsafe Rust: every unsafe block must carry a SAFETY comment whose preconditions are real and encoded in the type system. Prevents fabricated contracts — like Bun PathString.rs (2026) — that claim a 'caller guarantees' invariant no type enforces, silently permitting use-after-free. | improved | source-backed | `skills/rust/rust-unsafe-safety-contract-verification` |

### concurrency

Memory ordering, the atomics API, and lock/condvar discipline.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `atomics-c11-cpp11-rust` | Use when writing, reviewing, or porting atomic code in C11, C++20, or Rust — API lookup for _Atomic / std::atomic / std::sync::atomic, compare_exchange semantics (expected in-out, weak spurious failure), memory_order/Ordering validity, lock-free guarantees, and cross-language porting. | common | source-backed | `skills/concurrency/atomics-c11-cpp11-rust` |
| `concurrency-actual-parallelism-detection` | Use when verifying that concurrent code actually executes in parallel — distinguishing real parallelism from "fake" thread-safe code (CONCUR ST class) and catching concurrency-limit bypasses (codex#37653: 86 processes, kernel panic). Requires measuring wall-clock scaling and real overlap, not just counting threads or using primitives. | unique | source-backed | `skills/concurrency/concurrency-actual-parallelism-detection` |
| `concurrency-condvar-and-spurious-wakeup` | Use when writing, reviewing, or debugging condition-variable code (std::condition_variable, C11 cnd_*) — pairing wait with a predicate and mutex, handling spurious and lost wakeups (CON36-C), choosing notify_one vs notify_all, or fixing a program that occasionally hangs. | common | source-backed | `skills/concurrency/concurrency-condvar-and-spurious-wakeup` |
| `concurrency-deadlock-and-lock-ordering` | Use when writing or reviewing code that acquires two or more locks — detecting ABBA deadlock, enforcing consistent lock ordering (CON35-C), choosing std::lock/std::scoped_lock, avoiding recursive/try_lock hazards, or interpreting a TSan/helgrind lock-order report. | common | source-backed | `skills/concurrency/concurrency-deadlock-and-lock-ordering` |
| `memory-model-arm-x86-riscv` | Use when reasoning about cross-thread ordering on ARM, x86, or RISC-V: lock-free code, message passing, publication, atomic orderings, memory barriers, seq_cst vs relaxed, and porting TSO-only code to weak-memory machines. Teaches each architecture's memory model and how to map C11/Rust atomics to hardware. | cross-layer | source-backed | `skills/concurrency/memory-model-arm-x86-riscv` |
| `memory-ordering-reasoning` | Use when reasoning about atomic operations, memory ordering, happens-before, or lock-free synchronization in C11/C++11/Rust — choosing Relaxed vs Acquire/Release vs SeqCst, diagnosing "compiles but races at runtime", or explaining why ordering matters across architectures (x86 vs ARM). Teaches the ordering model and how to verify it. | improved | source-backed | `skills/concurrency/memory-ordering-reasoning` |

### zig



| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `zig-allocators-and-memory-management` | Use when designing Zig allocation: choosing an allocator, passing Allocator through APIs, arena lifetime, leak/double-free detection with std.testing.allocator, and OutOfMemory handling. Prevents hidden allocation, wrong-lifetime frees, and container item-invalidation bugs. Version-pinned to Zig 0.15-0.17. | unique | researched | `skills/zig/zig-allocators-and-memory-management` |
| `zig-build-system-and-packages` | Use when creating or reviewing build.zig/build.zig.zon, wiring packages and dependencies, integrating C sources, or orchestrating test/run steps. Prevents 0.14-era API usage, hardcoded paths that break caching, missing install steps, and wrong dependency wiring. Version-pinned to Zig 0.15-0.17. | unique | researched | `skills/zig/zig-build-system-and-packages` |
| `zig-comptime-metaprogramming` | Use when writing or reviewing Zig generics, reflection, comptime evaluation, @compileError gates, or inline-for metaprogramming. Prevents mixing runtime and comptime values, exceeding the comptime branch budget, using 0.15-era @Type, or letting lazy analysis skip intended compile errors. Version-pinned to Zig 0.15-0.17. | unique | researched | `skills/zig/zig-comptime-metaprogramming` |
| `zig-concurrency-and-io-events` | Use when writing or reviewing Zig concurrency: std.Thread spawn/join, atomics and memory ordering, thread-local state, std.Io evented I/O and io_uring, and single-threaded correctness. Prevents data races, fake parallelism, and 0.16 Thread.Pool removals. Version-pinned to Zig 0.15-0.17. | unique | researched | `skills/zig/zig-concurrency-and-io-events` |
| `zig-cross-compilation-targets` | Use when building for a target other than the host: -Dtarget triples, std.Target.Query, zig cc as a cross-compiler, CPU features (-mcpu), bundled libc, and running foreign binaries. Prevents host-only builds, wrong ABI suffixes, and feature-guessing. Version-pinned to Zig 0.15-0.17. | unique | researched | `skills/zig/zig-cross-compilation-targets` |
| `zig-error-model-and-defers` | Use when writing or reviewing Zig error handling: error sets and unions, try/catch, defer/errdefer LIFO semantics, optionals, and Illegal Behavior rules. Prevents resource leaks on error paths, double frees from defer+errdefer, wrong defer order, and unwrap panics. Version-pinned to Zig 0.15-0.17. | unique | researched | `skills/zig/zig-error-model-and-defers` |
| `zig-ffi-c-interop` | Use when bridging Zig and C: extern struct layout, C type primitives, @cImport/translate-c, extern fn and callconv(.c), std.c, variadics, and linking C libraries. Prevents ABI-layout mistakes, wrong C type sizes, and pre-0.16 @cImport habits. Version-pinned to Zig 0.15-0.17. | unique | researched | `skills/zig/zig-ffi-c-interop` |
| `zig-fuzzer-and-testing` | Use when writing Zig tests and fuzz targets: test declarations, std.testing.allocator leak detection, the built-in fuzzer and std.testing.Smith interface, corpora, and crash reproduction. Prevents naive assertions that pass all inputs, leaked allocations, and wrong-version fuzz signatures. Version-pinned to Zig 0.15-0.17. | unique | researched | `skills/zig/zig-fuzzer-and-testing` |
| `zig-inline-asm-and-abi` | Use when writing or reviewing Zig inline assembly, global asm, calling conventions, @export/@extern, and ABI-sensitive syscall wrappers. Prevents missing or wrong clobbers (unchecked Illegal Behavior), stale stringly-typed clobbers, deleted volatile, and wrong callconv. Version-pinned to Zig 0.15-0.17. | unique | researched | `skills/zig/zig-inline-asm-and-abi` |
| `zig-simd-vector-intrinsics` | Use when writing or reviewing Zig SIMD: @Vector, element-wise ops, @splat/@shuffle/@select/@reduce, std.simd, and LLVM-vector lowering. Prevents runtime vector indexing, vector-array coercion mistakes, missing overflow guards, and scalar-vector mixing. Version-pinned to Zig 0.15-0.17. | unique | researched | `skills/zig/zig-simd-vector-intrinsics` |
| `zig-version-migration` | Use when migrating Zig code across 0.15-0.17: Writergate std.io-to-std.Io changes, Juicy Main and non-global environment, @Type split, unmanaged containers, @cImport deprecation, and version pinning with build.zig.zon and zigup. Prevents pasting stale APIs and mis-attributing migration errors. Version-pinned. | unique | researched | `skills/zig/zig-version-migration` |


## Compilers & IR

### compiler

How compilers interpret undefined behavior — the root of '-O0 works, -O2 breaks'.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `compiler-ub-assumptions` | Use when diagnosing why C/C++ code behaves differently across optimization levels or compilers, when a bounds/null check "disappears" at -O2, when the optimizer reorders or elides code, or when explaining assumption-based optimization. Teaches how compilers exploit undefined behavior and how to prove the behavior with disassembly. | improved | source-backed | `skills/compiler/compiler-ub-assumptions` |

### llvm

Reading LLVM IR and writing passes.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `llvm-ir-reading` | Use when reading, reviewing, or debugging LLVM IR (.ll files or opt output) — understanding SSA form, GEP offsets, phi nodes, poison/undef/freeze semantics, opaque pointers, function attributes, or explaining why an optimization pass changed the IR. Covers clang -S -emit-llvm and opt workflows. | common | researched | `skills/llvm/llvm-ir-reading` |
| `llvm-pass-writing` | Use when writing, reviewing, or testing LLVM optimization passes in C++ — New Pass Manager structure, PassInfoMixin run methods, PreservedAnalyses correctness, analysis invalidation, IRBuilder usage, SSA-safe IR mutation, opt -passes= integration, and lit/FileCheck testing. | common | researched | `skills/llvm/llvm-pass-writing` |


## Machine level

### assembly

x86-64 registers, calling conventions, inline asm constraints, signed/unsigned branches, optimizer artifacts.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `asm-aarch64-neon-simd-safety` | Use when writing or reviewing AArch64 NEON/SIMD loops — vector intrinsics or assembly. Covers per-lane overflow, saturation vs wrap semantics, element types, and tail handling. Prevents the documented SIMD failure where counters overflow every ~255 iterations because no horizontal-reduce guard exists. | unique | researched | `skills/assembly/asm-aarch64-neon-simd-safety` |
| `asm-arm-thumb-2-encoding` | Use when writing or reviewing Thumb-2 assembly for Cortex-M and other Arm A/R-profile cores. Covers CBZ/CBNZ r0-r7 constraint and range, 16/32-bit mixed encoding, IT blocks for conditional execution, and A32-vs-Thumb differences. Prevents invalid hi-register branches, far-range branch bugs, and hand-encoded byte corruption. | unique | researched | `skills/assembly/asm-arm-thumb-2-encoding` |
| `asm-calling-conventions` | Use when writing or reviewing assembly, inline asm, or FFI code that calls or defines functions on x86-64 (SysV or Windows), AArch64, or RISC-V, or when reading disassembly and predicting argument registers, shadow space, stack alignment, callee-saved sets, and prologue shape. | common | source-backed | `skills/assembly/asm-calling-conventions` |
| `asm-inline-asm-constraints` | Use when writing, reviewing, or porting inline assembly in C, C++, or Rust — GCC extended asm constraints, memory and register clobbers, asm goto, and Rust asm! operand classes. Teaches declaring every asm side effect so the optimizer cannot miscompile around it. | common | source-backed | `skills/assembly/asm-inline-asm-constraints` |
| `asm-optimizer-artifacts` | Use when reading compiler-generated assembly (gcc/clang -O2 -S, objdump, Godbolt) and explaining why machine code diverges from C source — inlining, tail calls, dead-code elimination, constant folding, lea strength reduction, RIP-relative addressing. Teaches spotting optimizer artifacts without misreading them as missing code. | common | source-backed | `skills/assembly/asm-optimizer-artifacts` |
| `asm-risc-v-registers-and-calling-conventions` | Use when writing or reviewing RISC-V assembly — recursion, stack frames, and register roles. Covers callee-saved s0-s11 vs caller-saved a0-a7/t0-t6, RV64 8-byte slots and 16-byte alignment, leaf functions, and argument passing. Prevents the recorded garbage-sum failures from uninitialized s0 and 4-byte frames. | unique | researched | `skills/assembly/asm-risc-v-registers-and-calling-conventions` |
| `asm-signed-unsigned-branches` | Use when writing or reading x86-64 assembly, inline asm, or disassembly where signed vs unsigned semantics decide the instruction — jl/jg/jge/jle vs jb/ja/jae/jbe, sar vs shr, cdq/idiv vs xor/div, movsx vs movzx. Teaches flag semantics and how compilers select branch mnemonics from C types. | common | source-backed | `skills/assembly/asm-signed-unsigned-branches` |
| `asm-syntax-dialects-nasm-gas-att` | Use when writing or reviewing assembly where the dialect matters — NASM Intel-style versus GNU as AT&T versus GNU as Intel mode. Covers operand order, immediates, size hints, label case, and default rel. Prevents silent address-vs-content bugs, reversed operands, and the four documented NASM error classes. | unique | source-backed | `skills/assembly/asm-syntax-dialects-nasm-gas-att` |
| `asm-verification-hallucination-gate` | Use when an agent produces or reviews assembly and claims an instruction or encoding is valid. Gate every mnemonic, operand, width, and stack offset through assemble + disassemble + byte compare against an ISA manual, because generated mnemonics are often invented. Prevents fabricated instructions, AT&T reversals, and byte-blind claims. | unique | source-backed | `skills/assembly/asm-verification-hallucination-gate` |
| `asm-x86-64-registers-and-addressing` | Use when reading, writing, or reviewing x86-64 assembly: choosing register widths, operand-size suffixes, addressing modes, RIP-relative operands, REX encoding, canonical 48-bit addresses, or zero/sign extension. Prevents wrong-size moves, stale-flag branches, and non-canonical-address bugs in hand-written asm and inline asm. | common | source-backed | `skills/assembly/asm-x86-64-registers-and-addressing` |

### abi

Struct layout and argument passing for SysV AMD64, AAPCS64, RISC-V psABI — verified with the compiler.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `abi-layout-reasoning` | Use when writing or reviewing code that crosses a calling convention or binary interface — structs by value, FFI boundaries, inline asm, varargs, or layout-dependent code. Teaches how to compute struct layout and argument passing for SysV AMD64, AAPCS64, RISC-V psABI, and how to verify with the compiler. | cross-layer | source-backed | `skills/abi/abi-layout-reasoning` |

### ffi

Cross-language boundaries: layout, ownership, errors, and the no-unwind rule.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `ffi-boundary-cross-language` | Use when passing data or control across a language boundary — C to Rust, C++ to C, Zig to C, Rust to WASM — where layout, ownership, error translation, and unwind semantics must be pinned. Teaches the shared rules: repr(C) layout, who frees/drops, panic/unwind prohibition, and opaque handles. | cross-layer | source-backed | `skills/ffi/ffi-boundary-cross-language` |

### elf

The ELF pipeline: layout, relocations, GOT/PLT dynamic linking.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `elf-dynamic-linking-got-plt` | Use when explaining or debugging how a dynamically linked ELF program resolves external calls and data — GOT/PLT layout, lazy vs eager binding (LD_BIND_NOW), R_X86_64_GOTPCREL/PLT32 relocations, readelf -d and objdump -R output, symbol interposition, and -fPIC/-fPIE implications. | common | source-backed | `skills/elf/elf-dynamic-linking-got-plt` |
| `elf-layout-and-relocations` | Use when reading or debugging ELF object files and executables — ELF header fields, section vs program headers, .text/.data/.bss/.rodata/.dynsym/.got/.plt roles, symbol binding and visibility, R_X86_64_* relocation types, static vs dynamic linking, and the "recompile with -fPIC" error on x86-64 shared objects. | common | source-backed | `skills/elf/elf-layout-and-relocations` |
| `elf-linker-loader-debugger` | Use when diagnosing or building ELF binaries — symbol resolution, static vs dynamic linking, relocation failures, undefined symbols, missing -fPIC, PLT/GOT lazy binding, .init_array ordering, _start-to-main flow, dynamic loader errors, or mapping addresses to source lines in a debugger. Explains the compiler-to-linker-to-loader-to-debugger pipeline as one process. | cross-layer | source-backed | `skills/elf/elf-linker-loader-debugger` |

### dwarf

Debug info and debugging optimized builds.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `dwarf-debug-info` | Use when reading or generating DWARF debug info, mapping addresses to source lines, explaining "value optimized out" in optimized builds, writing debug-friendly code, or inspecting binaries with objdump/readelf/gdb. Teaches DWARF sections, DIEs, attributes, location lists, and optimized-code debugging strategies. | improved | source-backed | `skills/dwarf/dwarf-debug-info` |


## Systems engineering

### kernel

uaccess safety, RCU and memory barriers, atomic-context rules.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `data-race-kernel-detection` | Use when hunting or reviewing kernel data races, reading KCSAN reports, or adding READ_ONCE/WRITE_ONCE/locks. Teaches the KCSAN model (watchpoint sampling, plain vs marked accesses), race classes in kernel context, and distinguishing real races from false positives. | improved | source-backed | `skills/kernel/data-race-kernel-detection` |
| `deadlock-kernel-prevention` | Use when reviewing kernel locking, fixing lockdep splats, or designing lock ordering. Teaches AB-BA lock inversion, irq-context (irq-safe/unsafe) rules, sleeping-while-locked violations, nested locking, and how lockdep proves classes of deadlocks. | improved | source-backed | `skills/kernel/deadlock-kernel-prevention` |
| `dma-cache-coherency` | Use when writing or reviewing DMA code — dma_alloc_coherent vs streaming mappings, dma_map_single/sg, dma_sync_single_for_cpu/device, DMA direction, cache-coherent vs non-coherent devices, bounce buffers, and stale-data bugs. Teaches the Linux DMA-API contract and the cache-coherency rules that keep device and CPU views consistent. | unique | source-backed | `skills/kernel/dma-cache-coherency` |
| `framekernel-architecture` | Use when reviewing or designing framekernel OSes (e.g. Asterinas) or comparing kernel architectures. Teaches the framework/service split in one address space, Rust-only requirement, MMU-based service isolation, and Linux ABI compatibility trade-offs. | unique | researched | `skills/kernel/framekernel-architecture` |
| `fuzzing-harness-kernel` | Use when building syzkaller descriptions, kernel fuzz harnesses, or coverage-guided kernel fuzzing. Teaches syscall-program generation, KCOV coverage feedback, harness initialization and seed corpora, crash minimization, and reproducing/classifying reports. | unique | source-backed | `skills/kernel/fuzzing-harness-kernel` |
| `interrupt-controller-gic-apic` | Use when writing or reviewing interrupt handling — ARM GICv3/v4 routing (SPI/PPI/SGI, distributor, redistributor), x86 APIC (LAPIC, IO-APIC, MSI, spurious vectors), priority/nesting, EOI, edge vs level, affinity, and IRQ remapping. Teaches the two controller families' structure and the driver contract for correct IRQ delivery. | unique | researched | `skills/kernel/interrupt-controller-gic-apic` |
| `iommu-smmu-isolation` | Use when writing or reviewing IOMMU/SMMU code — DMA isolation, stream/context tables, stage-1/2 translation, iommu_domain, VT-d vs SMMU, passthrough vs translation, device TLB invalidation, and DMA faults. Teaches IOMMU architecture and how to keep devices from bypassing isolation. | unique | researched | `skills/kernel/iommu-smmu-isolation` |
| `kernel-api-drift-migration` | Use when porting or reviewing kernel code across kernel versions: sys_call_table unexport, DRM fbdev-to-client_setup, IIO migrations, symbol availability via kallsyms, and API signature changes. Prevents code that compiles but silently does nothing after an API disappears, and pins kernel versions before claiming compatibility. | unique | researched | `skills/kernel/kernel-api-drift-migration` |
| `kernel-atomic-context` | Use when writing, reviewing, or debugging Linux kernel code that runs in atomic context: interrupt handlers, bottom halves, spinlock-held or preemption-disabled regions. Covers what is forbidden there (sleeping kmalloc/mutex/schedule), GFP_ATOMIC, irqsave/bh lock variants, deferring to process context, and verifying with lockdep. | common | source-backed | `skills/kernel/kernel-atomic-context` |
| `kernel-container-internals` | Use when writing or reviewing container-adjacent kernel claims — namespaces, cgroups v2, overlayfs, OCI/runc bundles, seccomp, and capability semantics. Prevents v1-era cgroup knobs, userns-root myths, and seccomp-as-sandbox overstatements from passing as facts. | unique | researched | `skills/kernel/kernel-container-internals` |
| `kernel-debugging-ftrace-kprobes-kdump` | Use when debugging or reviewing Linux kernel problems that need instrumentation — ftrace function graphs, tracepoints, kprobes, dynamic debug, kgdb/kdb, or kdump analysis. Prevents guessed debug knobs and filesystem confusion by requiring the real tracefs/debugfs path and exact command before any claim. | unique | researched | `skills/kernel/kernel-debugging-ftrace-kprobes-kdump` |
| `kernel-driver-char-device-lifecycle` | Use when writing or reviewing Linux character device drivers: file_operations dispatch, copy_from_user/copy_to_user return handling, cdev/class/device creation and teardown symmetry, module unload cleanup, and reference counting. Prevents unchecked user copies, inverted read/write contracts, double class_destroy, and unloading while the device is open. | unique | researched | `skills/kernel/kernel-driver-char-device-lifecycle` |
| `kernel-exploitation-primitives` | Use when assessing kernel exploitability, reviewing security fixes, or reasoning about bug-to-primitive chains. Teaches primitive taxonomy (UAF, OOB, refcount overflow, info leak), SLUB freelist mechanics, and why a bug's reachability determines its weaponizability. | unique | researched | `skills/kernel/kernel-exploitation-primitives` |
| `kernel-loader-elf` | Use when a bootloader must load an ELF kernel image at boot time: parsing program headers, mapping virtual addresses and alignment, applying relocations for position-independent kernels, decompressing a compressed payload, and performing the entry-point handoff. Teaches the boot-time subset of ELF loading that differs from userspace loading. | unique | source-backed | `skills/kernel/kernel-loader-elf` |
| `kernel-module-build-out-of-tree` | Use when building, packaging, or fixing Linux kernel modules outside the kernel tree: Kbuild Makefiles, Kconfig, kernel-headers dependency and version binding, MODULE_LICENSE/EXPORT_SYMBOL placement, and mismatch between a module built against one kernel and loaded on another. Prevents "unknown symbol", "disagrees about version", and silently-wrong module builds. | unique | researched | `skills/kernel/kernel-module-build-out-of-tree` |
| `kernel-patch-review-commit-log-independence` | Use when reviewing kernel patches: treat the commit log as untrusted, review the diff itself, verify claimed fixes actually touch the vulnerable code, and reject commit-log claims without diff evidence. Teaches diff-first review to counter LLM reviewer bias. | unique | researched | `skills/kernel/kernel-patch-review-commit-log-independence` |
| `kernel-rcu-memory-barriers` | Use when writing or reviewing Linux kernel code that needs memory barriers, READ_ONCE/WRITE_ONCE, or RCU — publish-subscribe patterns, rcu_assign_pointer/rcu_dereference, synchronize_rcu, or atomic-context rules like no sleeping in spinlocks. Teaches the kernel memory model and why it differs from C11 atomics. | cross-layer | source-backed | `skills/kernel/kernel-rcu-memory-barriers` |
| `kernel-scheduler-mm-vfs-internals` | Use when writing or reviewing claims about Linux internals — the fair scheduler (CFS vs EEVDF), vruntime and lag, buddy/SLUB allocation, kmalloc vs vmalloc, and VFS dcache/inode behavior. Prevents stale pre-6.6 scheduler lore and invented /proc fields from passing as kernel knowledge. | unique | researched | `skills/kernel/kernel-scheduler-mm-vfs-internals` |
| `kernel-uaccess-safety` | Use when writing, reviewing, or fixing Linux kernel driver code that exchanges data with user space — read/write, ioctl, mmap, poll/fasync, copy_to_user/copy_from_user, get_user/put_user, access_ok, compat_ioctl. Teaches fault-safe copying, size validation, and the -EFAULT/-ENOTTY/SIGSEGV symptom classes. | common | source-backed | `skills/kernel/kernel-uaccess-safety` |
| `kernel-ub-patterns` | Use when reviewing or writing Linux-kernel-style C code for UB: strict aliasing in kernel code, container_of misuse, __user annotation violations, and integer overflow in kernel APIs. Teaches the kernel-specific instances of undefined behavior that differ from userspace, and how to reason about them against the C standard and kernel conventions. | improved | source-backed | `skills/kernel/kernel-ub-patterns` |
| `page-table-management` | Use when writing or reviewing virtual-memory code — x86-64 4/5-level paging, AArch64 VMSAv8-64, RISC-V Sv39/48, PTE flags, TLB invalidation, kernel vs user mappings, large pages, and page-fault debugging. Teaches page-table structure, walk logic, and the TLB-coherency rules that must hold after any PTE change. | unique | source-backed | `skills/kernel/page-table-management` |
| `property-based-testing-kernel` | Use when verifying kernel-adjacent code (parsers, checksums, boundary math, state machines) with property-based tests: designing universal properties, writing generators and shrinkers, running quickcheck/proptest-style loops in C and Rust. Teaches finding counterexamples instead of asserting specific cases, and shrinking failing inputs to minimal reproducers. | unique | source-backed | `skills/kernel/property-based-testing-kernel` |
| `sel4-sddf-driver-framework` | Use when writing or reviewing seL4 device drivers, SDDF components, or Microkit systems. Teaches capability-based driver isolation, virtio queue protocols for network/block/serial, DMA and shared-memory rules, and why drivers are user-level servers, not kernel modules. | unique | researched | `skills/kernel/sel4-sddf-driver-framework` |
| `toctou-kernel` | Use when writing or reviewing kernel/driver code that validates then uses a resource — file paths, user pointers, fd tables, pid/name lookups, mounts — where another thread can mutate between check and use. Teaches TOCTOU patterns, openat2/renameat2/AT_EMPTY_PATH idioms, refcount-and-lock fixes, and how to make check-then-use atomic. | unique | researched | `skills/kernel/toctou-kernel` |

### networking

The eBPF verifier model.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `ebpf-verifier-opaque-feedback-iteration` | Use when a BPF program fails the verifier with an opaque log: extract the failing instruction from the log tail, minimize the program, bisect register state, and iterate a minimal fix. Teaches persisting where LLMs give up. | improved | researched | `skills/networking/ebpf-verifier-opaque-feedback-iteration` |
| `ebpf-verifier-reasoning` | Use when writing, reviewing, or debugging eBPF C programs — map_lookup_elem null checks, bounded loops, pointer arithmetic on packet/map/stack pointers, helper argument constraints, and reading verifier rejection logs. Teaches how the kernel verifier proves safety and which patterns it rejects at load time. | improved | researched | `skills/networking/ebpf-verifier-reasoning` |
| `networking-hardware-rdma-nic-offload` | Use when writing or reviewing RDMA verbs code or NIC offload claims — QP/MR/CQ semantics, transport vs link layer (RC/RoCE/iWARP/InfiniBand), one-sided operations, and DPDK/eSwitch/rte_flow offload. Prevents invented verbs APIs and offload features misattributed to the wrong layer. | unique | researched | `skills/networking/networking-hardware-rdma-nic-offload` |

### embedded

Volatile/MMIO ordering, interrupts, linker scripts, MPU/TrustZone, RTOS ISR discipline.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `embedded-board-bringup-peripheral-init` | Use when bringing up a new embedded board or peripheral: clock-tree and init-order reasoning, GPIO alternate-function configuration, register guessing for unknown MCUs, and quadrature-encoder interpretation. Prevents "looks correct but wrong" init code and naive level-counting encoders. Requires datasheet-first reasoning and state-machine logic for gray-code signals. | unique | researched | `skills/embedded/embedded-board-bringup-peripheral-init` |
| `embedded-device-tree-and-kconfig` | Use when writing or reviewing Zephyr/DeviceTree overlays and Kconfig fragments — compatible strings, unit-addresses, reg, required binding properties, and driver Kconfig symbols. Prevents invented compatible strings and Kconfig symbols that build silently but leave hardware undriven. | unique | researched | `skills/embedded/embedded-device-tree-and-kconfig` |
| `embedded-flash-debug-cycle` | Use when flashing and debugging embedded targets: OpenOCD/GDB probe ownership, SWD probe contention, orphaned GDB sessions, DFU/USB preflight, lock files, and flash-verify steps. Prevents probe contention, "can't reliably flash" loops, and strand issues caused by leftover sessions. Requires single-probe discipline and clean session teardown. | unique | researched | `skills/embedded/embedded-flash-debug-cycle` |
| `embedded-hil-ci-testing` | Use when setting up or running embedded hardware-in-the-loop CI: device-ready gating, re-enumeration after flashing, flash-verify gates, and toolchain-use guardrails from ESP-IDF/Zephyr AI guidance. Prevents flaky HIL runs from device-ready races and treating compile as deploy approval. Requires verifying on hardware before declaring a change ready. | unique | researched | `skills/embedded/embedded-hil-ci-testing` |
| `embedded-hw-register-datasheet-verification` | Use when writing or reviewing embedded peripheral drivers that touch raw registers — GPIO, I2C, SPI, display controllers, clocks. Prevents hallucinated registers, swapped bit positions, wrong reset values, and cross-family register maps by encoding the datasheet layout as a compilable C model. | unique | source-backed | `skills/embedded/embedded-hw-register-datasheet-verification` |
| `embedded-interrupt-and-nested` | Use when writing, reviewing, or debugging bare-metal Cortex-M firmware that uses interrupts — NVIC enable/pending/priority, exception vector table, nesting and preemption, PRIMASK critical sections with __disable_irq/__enable_irq, interrupt latency, and sharing state between ISR and main context. Teaches Cortex-M interrupt discipline and host verification. | common | source-backed | `skills/embedded/embedded-interrupt-and-nested` |
| `embedded-linker-script` | Use when writing, reviewing, or debugging a bare-metal embedded GNU ld linker script — MEMORY regions, SECTIONS placement, FLASH/RAM mapping, KEEP() on the vector table, startup copy loops with __etext/__data_start/__bss_start, ALIGN(), the location counter, and why firmware fails to boot or starts with stale .data. | common | source-backed | `skills/embedded/embedded-linker-script` |
| `embedded-mpu-trustzone` | Use when writing, reviewing, or debugging Cortex-M firmware that configures the MPU (ARMv7-M RBAR/RASR or ARMv8-M RBAR/RLAR regions, PRIVDEFENA background region, AP privilege) or ARMv8-M TrustZone (SAU regions, NSC veneers, secure/non-secure transitions, flash/SRAM partitioning), or when secure software faults on non-secure calls. | unique | source-backed | `skills/embedded/embedded-mpu-trustzone` |
| `embedded-ota-bootloader-safety` | Use when designing or reviewing firmware over-the-air updates and bootloaders: A/B slots, staged rollouts, rollback, trial windows, power-loss-safe flash writes, and watchdog/bootstrap integrity. Prevents fleet bricking from bad OTA configs and task-fatal bugs. Requires staged rollout with telemetry and verified rollback before declaring an update safe. | unique | researched | `skills/embedded/embedded-ota-bootloader-safety` |
| `embedded-volatile-and-memory-ordering` | Use when writing, reviewing, or debugging embedded C that accesses memory-mapped I/O (MMIO) registers or shares flags with an interrupt handler — volatile vs non-volatile access, why -O2 changes behavior, why volatile is not atomic, device vs normal memory attributes, and barriers for ordering. Teaches volatile rules and verification. | common | source-backed | `skills/embedded/embedded-volatile-and-memory-ordering` |
| `hardware-register-bringup` | Use when bringing up unknown MCU/peripheral registers from a datasheet: power-on reset sequences, clock enabling and reset deassert order, register init-order reasoning, and datasheet-first validation before writing firmware. Teaches the power-on-to-peripheral-ready sequence and how to verify register values against the reference manual before flashing. | unique | source-backed | `skills/embedded/hardware-register-bringup` |
| `rtos-concurrency-and-isr` | Use when writing, reviewing, or debugging FreeRTOS/Zephyr firmware with tasks, queues, semaphores, mutexes, or interrupt handlers — blocking calls in ISRs, ISR-safe FromISR APIs, priority inversion and inheritance, task vs ISR context, periodic task timing, and stack sizing. Teaches context-correct RTOS usage and verification. | unique | source-backed | `skills/embedded/rtos-concurrency-and-isr` |

### bootloader

Boot stages and relocation.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `bootloader-stages` | Use when writing or debugging bootloaders and firmware — BIOS/UEFI stage-1 loading, MBR/GPT, x86 real to protected to long mode (A20, GDT, CR0/CR4/EFER, paging), AArch64 and RISC-V boot protocols, and link-address vs load-address relocation at stage-2 entry. | unique | source-backed | `skills/bootloader/bootloader-stages` |
| `bootloader-uefi-acpi-dtb` | Use when a UEFI bootloader must hand off system-description data: locating ACPI tables (RSDP/XSDT/MADT), SMBIOS structures, or a Flattened Device Tree (DTB), and deciding which interface the platform exposes. Teaches table parsing, checksum validation, and architecture-dependent firmware-handoff conventions. | improved | source-backed | `skills/bootloader/bootloader-uefi-acpi-dtb` |
| `bootloader-uefi-firmware` | Use when writing, reading, or debugging UEFI firmware and edk2 code: Boot vs Runtime Services, the PI spec boundary, ExitBootServices ordering, HII/VFR forms, ACPI/SMBIOS tables, and Secure Boot. Prevents boot-services-after-exit crashes, missing RUNTIME_ACCESS flags, and VFR bounds that the driver ignores. | improved | researched | `skills/bootloader/bootloader-uefi-firmware` |

### qemu

System emulation for kernel, firmware, and bare-metal verification.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `qemu-system-setup` | Use when setting up QEMU system emulation to boot a Linux kernel, firmware, or bare-metal ELF for x86-64, ARM Cortex-M, or AArch64 — machine model selection, -kernel/-nographic/-drive/netdev, serial console, and gdb remote debugging. | common | researched | `skills/qemu/qemu-system-setup` |

### virtualization

Hypervisor internals (VMX/SVM), KVM ioctls, paravirtualization.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `hypervisor-vmx-svm-internals` | Use when writing, reading, or reviewing hypervisor code: VT-x VMCS layout, VMXON/VMLAUNCH/VMEXIT state transitions, SVM VMCB/VMRUN, EPT/NPT two-level page tables, APICv and posted interrupts, and VMEXIT reason-code handling. Prevents wrong control fields, misconfigured exits, and guest-escape vulnerabilities. | unique | researched | `skills/virtualization/hypervisor-vmx-svm-internals` |

### riscv

RISC-V ISA, vector extensions (RVV), and CHERI capability-based safety.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `riscv-cheri-capability-safety` | Use when writing or reviewing capability-aware C/C++ for CHERI (Morello, CHERI-RISC-V): capability pointers, bounds and tags, purecap vs hybrid ABI, sealing/unsealing, CheriBSD, and CHERI-enabled sanitizers. Prevents out-of-bounds dereferences, tag loss, and false positives that flag correct bounded code. | unique | researched | `skills/riscv/riscv-cheri-capability-safety` |
| `riscv-isa-and-rvv-intrinsics` | Use when writing or reviewing RISC-V code with the vector extension: vsetvl/vsetvli AVL-VL semantics, LMUL/SEW ratios, VLEN dependence, tail and mask policies, strip-mining loops, strided and segmented loads, and RVV C intrinsics. Prevents wrong VL, policy misuse, and portability bugs across VLEN implementations. | unique | researched | `skills/riscv/riscv-isa-and-rvv-intrinsics` |


## Binary analysis & RE

### binary-analysis

Type recovery from disassembly.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `binary-analysis-type-recovery` | Use when recovering C types (char, short, int, long, float, double, structs, arrays, pointers, function/vtable signatures) from x86-64 disassembly via instruction-width and addressing patterns (movzx/movsx, movss/movsd, movl/movq, disp(%reg), scale indexing), validated with DWARF when present. | common | source-backed | `skills/binary-analysis/binary-analysis-type-recovery` |
| `binary-disassembly-decompilation-fidelity` | Use when decompiling or judging decompiled code from x86-64 binaries, or when a reconstructed C function "looks right". Apply re-executability and byte-round-trip gates because decompilation is plausible-but-wrong (DeGPT 37% CFR, SCDBench 7%, Meta LLM Compiler 14% exact, cliff at ~200 instructions). | unique | source-backed | `skills/binary-analysis/binary-disassembly-decompilation-fidelity` |
| `binary-memory-leak-vm-allocator-diagnosis` | Use when diagnosing high RAM/RSS/commit growth that heap profilers (ASan/leak checkers) do not explain — VM-level leaks: page pools that reuse mmap/mmap'd regions without munmap, unbounded pools, allocator reuse-without-release. Ghostty PageList.zig 37-130 GB; agent is the trigger, not the cause. | unique | researched | `skills/binary-analysis/binary-memory-leak-vm-allocator-diagnosis` |

### reverse-engineering

Go/Rust binaries and automated protocol RE.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `auto-re-protocols-beyond-can` | Use when reverse-engineering a binary protocol from captured bytes — UART/serial frames, industrial fieldbus or CAN-style traffic — where the wire format is unknown and must be recovered from evidence. Applies the deterministic pipeline: capture, survey, correlate, bit/field search, schema, verify-as-gate, instead of guessing field boundaries. | unique | source-backed | `skills/reverse-engineering/auto-re-protocols-beyond-can` |
| `go-rust-re` | Use when analyzing or reverse-engineering Go or Rust binaries — recovering function names from .gopclntab, decoding mangled Rust symbols (_ZN/_R), finding panic strings and core::fmt literals in .rodata/.rdata, distinguishing Rust from C/C++, or triaging stripped Go/Rust executables with GoReSym, redress, objdump, gdb, and Delve. | improved | source-backed | `skills/reverse-engineering/go-rust-re` |
| `reverse-engineering-can-signal-extraction` | Use when extracting CAN signal layouts from a DBC file or reverse-engineering unknown CAN signals: DBC bit numbering (Intel vs Motorola sawtooth), scale/offset, little/big endian, and the parked-vs-moving gate (SWEEP vs HOLDS) before certifying a signal as speed-like. | improved | researched | `skills/reverse-engineering/reverse-engineering-can-signal-extraction` |
| `reverse-engineering-ghidra-agent-automation` | Use when an agent drives Ghidra (PyGhidra daemon/RPC or headless) in a triage-annotate-type-recovery-diff loop, or when an automated verdict about a binary is being trusted. Prevents confident-but-wrong identity claims, rebase errors ($0000 vs $A000), and unverified byte patches. | unique | researched | `skills/reverse-engineering/reverse-engineering-ghidra-agent-automation` |
| `reverse-engineering-shellcode-analysis` | Use when reading, verifying, or extracting information from raw shellcode bytes (x86-64 Linux focus): syscall numbers, IP/port constants, instruction inventory. Prevents invented instructions, wrong syscall tables, and byte-order errors in IP/port extraction. | unique | source-backed | `skills/reverse-engineering/reverse-engineering-shellcode-analysis` |

### mobile

Android reverse engineering: DEX/Smali format extraction and JNI/native layer analysis.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `android-re-dex-smali-format` | Use when reading, writing, or reviewing smali and DEX-format artifacts: register model, instruction widths, verifier constraints, R8/ProGuard mapping parsing, and Kotlin-metadata name recovery. Prevents register-type confusion, move-object misuse, const width errors, and fabricated deobfuscation claims. | unique | researched | `skills/mobile/android-re-dex-smali-format` |
| `android-re-native-jni-analysis` | Use when analyzing native .so code in Android apps: JNI symbol names/signatures, JNIEnv function-table access, reference/array hygiene, and dynamic analysis via Frida. Prevents UnsatisfiedLinkError-style signature errors, global-ref leaks, and hooking guessed addresses instead of fingerprint-first real ones. | unique | researched | `skills/mobile/android-re-native-jni-analysis` |


## Performance & HPC

### performance

Measure-before-optimize discipline and cache/NUMA-aware code.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `cache-and-numa-optimization` | Use when writing or reviewing memory-bound C code where cache behavior and NUMA placement dominate performance: false sharing, cache-line padding, row-major vs column-major access, struct-of-arrays vs array-of-structs, strided access, prefetching, and NUMA node-local allocation with numactl. | common | source-backed | `skills/performance/cache-and-numa-optimization` |
| `performance-measurement-discipline` | Use when asked to optimize or benchmark C code, or when a performance claim needs evidence — profiling before changes, benchmark harness correctness, warmup and repetitions, dead-code elimination of benchmarks, statistical noise, regression baselines, and microbenchmark pitfalls like inlining and aliasing. | common | source-backed | `skills/performance/performance-measurement-discipline` |

### simd

Auto-vectorization, blockers, intrinsics.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `simd-vectorization-cross-layer` | Use when reasoning about why a C loop did or did not vectorize, reading GCC `-fopt-info-vec`/`-fopt-info-missed-vec` or Clang `-Rpass=loop-vectorize` output, diagnosing aliasing, loop-carried dependency, alignment, or trip-count blockers, choosing between auto-vectorization, `restrict`, runtime dispatch, and intrinsics, and inspecting xmm/ymm vector asm. | cross-layer | source-backed | `skills/simd/simd-vectorization-cross-layer` |
| `vectorization-reasoning` | Use when analyzing whether a C loop can vectorize or why it does not — loop-carried dependencies, aliasing and restrict, known vs unknown trip counts, reductions, induction variables, alignment and cost-model assumptions, and interpreting GCC -fopt-info-vec and missed reports before touching intrinsics. | common | source-backed | `skills/simd/vectorization-reasoning` |

### gpu

GPU coherence scopes and PTX assembly.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `gpu-communication-primitives` | Use when designing or reviewing multi-GPU kernels that exchange data: point-to-point peer transfers, ring and tree all-reduce/all-gather collectives, NCCL usage and topology choices, expert-parallel sharding. Prevents deadlocks, wrong chunking, and non-deterministic or unsafe collectives that silently corrupt data. | unique | researched | `skills/gpu/gpu-communication-primitives` |
| `gpu-kernel-reward-hacking-detection` | Use when reviewing or building benchmarks for LLM-generated CUDA/GPU kernels: detecting hardcoded bypasses and fake speedups that exploit the narrow test distribution, and applying verified-evaluation protocols (hidden distributions, realistic baselines, memory metrics) so reward hacking cannot fake benchmark wins. | unique | researched | `skills/gpu/gpu-kernel-reward-hacking-detection` |
| `gpu-kernel-verification-beyond-oracle` | Use when verifying GPU kernels: escaping the fixed-shape allclose oracle trap, building fuzz harnesses over unseen shapes, comparing against fp64 references, and checking edge sizes that trip grid/block indexing. Prevents kernels that pass review but segfault or corrupt under real loads. | unique | researched | `skills/gpu/gpu-kernel-verification-beyond-oracle` |
| `gpu-memory-model-coherence` | Use when writing, reviewing, or debugging CUDA/HIP kernels where memory coherence matters — shared-memory races without __syncthreads(), relaxed atomics used as synchronization, cross-block visibility without __threadfence(), volatile for inter-thread sync, or host-device ordering. Teaches GPU memory hierarchy, coherence scopes, and correct synchronization. | common | researched | `skills/gpu/gpu-memory-model-coherence` |
| `ptx-assembly` | Use when writing, reading, or reviewing NVIDIA PTX assembly: state spaces, register types, memory loads/stores with scope and ordering, predication, bar.sync barriers, atomics, and warp-level shfl/vote, or when mapping between CUDA C++ and PTX/SASS. Teaches correct PTX syntax and GPU memory-model semantics. | unique | researched | `skills/gpu/ptx-assembly` |

### hpc

MPI parallel programming, OpenMP offload, RDMA verbs for high-performance compute.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `hpc-mpi-programming` | Use when writing, reviewing, or debugging MPI programs: point-to-point Send/Recv and non-blocking Isend/Irecv, collectives, communicators and comm_split, hybrid MPI+OpenMP, MPI-IO, and mpirun/Slurm launch. Prevents deadlocks, ordering bugs, wrong collective counts, and hangs that appear only with 2+ ranks. | unique | researched | `skills/hpc/hpc-mpi-programming` |
| `hpc-openmp-parallel-programming` | Use when writing, reviewing, or debugging OpenMP programs: parallel regions, worksharing loops and schedules, reductions, firstprivate/private/lastprivate, atomic and critical sections, data races on shared variables, and target offload. Prevents races, wrong reduction semantics, and schedule-dependent bugs that only appear with multiple threads. | unique | source-backed | `skills/hpc/hpc-openmp-parallel-programming` |
| `hpc-rdma-verbs` | Use when writing, reviewing, or debugging RDMA programs built on libibverbs: QP/CQ/MR lifecycle, RC/UC/UD transports, work requests and completions, RDMA write/read/atomic, RoCE vs InfiniBand, and perftest interpretation. Prevents QP state-machine violations, rkey misuse, and completion-handling races. | unique | researched | `skills/hpc/hpc-rdma-verbs` |


## Tooling & agent behavior

### sanitizers

The agent CI loop and reading sanitizer reports.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `fuzzing-harness-evidence-gate` | Use when reporting or reviewing fuzzing results to decide whether a finding is evidence or noise. Enforces the proof standard: reproducible sanitizer report, minimized crashing input, demonstrated reachable path, before any bug or CVE claim. Covers libFuzzer and AFL++ harnesses. | improved | researched | `skills/sanitizers/fuzzing-harness-evidence-gate` |
| `sanitizer-agent-ci-loop` | Use when integrating sanitizers (ASan/UBSan/TSan/MSan) into an agent's build-and-test loop for C/C++/Rust — so every change is automatically checked, reports are parsed and deduplicated, and regressions are caught. Fills the gap where "how to use sanitizers" exists but the universal agent loop does not. | unique | source-backed | `skills/sanitizers/sanitizer-agent-ci-loop` |
| `sanitizer-report-reading` | Use when interpreting sanitizer output — ASan/UBSan/TSan/MSan/LSan reports from builds, CI logs, or fuzzing — to identify bug category, access site versus allocation/free site, root cause, and fix. Triggers on report headers like 'ERROR: AddressSanitizer', shadow bytes, 'data race', or 'use-of-uninitialized-value'. | improved | researched | `skills/sanitizers/sanitizer-report-reading` |

### _meta

Agent behavior: routing, evidence, verification, assumptions, rationalizations, completion — plus the flagship cross-layer skills.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `agent-scope-management` | Use when an agent session spans many edits, spawns subagents, or loses context. Teaches persisting state to files (progress/WORKLOG), single-responsibility scope boundaries, stopping rules, and why conclusions not written down do not exist. | common | source-backed | `skills/_meta/agent-scope-management` |
| `agent-tool-whitelist` | Use when an agent must decide which shell/tool operations are allowed in a low-level repo or CI environment: maintaining a whitelist of safe, deterministic operations, rejecting ad-hoc dangerous commands, and enforcing read-only-by-default. Teaches the discipline of allowed-operations gates that prevent environment-destructive or non-reproducible commands. | common | source-backed | `skills/_meta/agent-tool-whitelist` |
| `llm-verifier-warning-disposition` | Use when an LLM agent accepts or dismisses bug reports, sanitizer warnings, or static-analysis findings: dismissal requires a reachability argument (witness/trace or solver pass), not plausibility. Teaches the burden-of-proof discipline so agents never self-certify that a reported error state is unreachable. | unique | researched | `skills/_meta/llm-verifier-warning-disposition` |
| `meta-assumptions` | Use when code correctness depends on implicit assumptions — compiler, ABI, platform, memory model, optimization level, or endianness. Forces surfacing and documenting every assumption before concluding. | common | researched | `skills/_meta/meta-assumptions` |
| `meta-claim-extraction` | Use when documenting claims in a skill or SKILL.md/references. Teaches extracting claim → source → section → skill into registry/claims.yaml with confidence levels and KNOWN/INFERRED/UNVERIFIED evidence classification. | common | researched | `skills/_meta/meta-claim-extraction` |
| `meta-completion` | Use before declaring a low-level task complete. Enforces honest completion criteria: verifiable success, no hidden partial results, updated state files, and explicit uncertainty. | common | researched | `skills/_meta/meta-completion` |
| `meta-eval-runner` | Use when running evals for skills: synthetic, false-positive (FP), adversarial, and historical-CVE loops. Teaches the eval loop, scoring, and recording results in evals/README.md and registry/evals.yaml. | common | researched | `skills/_meta/meta-eval-runner` |
| `meta-evidence` | Use whenever making a normative or factual claim about C/C++/Rust/asm/ABI/UB/compiler behavior. Enforces the KNOWN / INFERRED / UNVERIFIED classification and requires source-backed evidence for strong claims. | common | researched | `skills/_meta/meta-evidence` |
| `meta-formal-verification` | Use when deciding whether formal verification is required or empirical testing suffices for a low-level claim. Teaches Kani/CBMC/Frama-C/Z3 selection and sound loop-invariant encoding. | common | researched | `skills/_meta/meta-formal-verification` |
| `meta-rationalizations` | Use during code review or self-review to catch and reject rationalizations that excuse unsafe or incorrect low-level code. Contains the "Rationalizations to Reject" list derived from trailofbits and failure modes B1-B22. | common | researched | `skills/_meta/meta-rationalizations` |
| `meta-routing` | Use at the start of any low-level task to choose the minimal relevant skill set. Prevents "load everything" behavior, enables dependency expansion, and routes to the correct skill from the registry. | common | researched | `skills/_meta/meta-routing` |
| `meta-security-audit` | Use when reviewing low-level code or this repository for security. Teaches license compatibility checks, CVE regression verification, examples/bad scrutiny, and a security-review checklist with evidence. | common | researched | `skills/_meta/meta-security-audit` |
| `meta-token-optimization` | Use when authoring or editing SKILL.md to keep activation cost at or under 2000 tokens. Teaches measuring with tools/tokens/token_measure.py, moving depth to references/, and avoiding duplication across the skill. | common | researched | `skills/_meta/meta-token-optimization` |
| `meta-verification` | Use before concluding that low-level code is correct or that a bug is found. Enforces executable verification (compile+run, sanitizers, asm inspection, debugger) instead of "it compiles" or "tests pass". | common | researched | `skills/_meta/meta-verification` |
| `meta-verification-harness-validity` | Use before trusting a "passing" test harness, eval, or CI gate. Verifies the verification: a harness that cannot fail when its target is broken (unconditional pass, never-executed path, self-test bypass) certifies nothing. Teaches ablation-delta, coverage gates, and --self-test. | improved | source-backed | `skills/_meta/meta-verification-harness-validity` |
| `safe-low-level-from-scratch` | Use when writing NEW low-level code (C/C++/Rust/asm) from scratch that must be memory-safe and correct across optimization levels and platforms. Provides the positive writing process integrating UB semantics, layout/alignment, ownership, atomics, and FFI, with verification gates at each step. | cross-layer | source-backed | `skills/_meta/safe-low-level-from-scratch` |
| `wasm-runtime-from-scratch` | Use when writing, reviewing, or debugging a WebAssembly runtime, interpreter, loader, or validator in C — module binary parsing, validation, linear memory bounds, tables and call_indirect, traps vs undefined behavior, memory.grow, and host function imports. | unique | source-backed | `skills/_meta/wasm-runtime-from-scratch` |
| `zeroize-constant-time` | Use when writing or reviewing code handling secrets (keys, passwords, nonces) that must be zeroized or compared in constant time. Triggers on memset-before-return, secret-dependent branches or indexing, memcmp on secrets, and claims that a secret is cleared. Teaches volatile-sink zeroization, explicit_bzero, ct_memcmp, and asm verification. | improved | source-backed | `skills/_meta/zeroize-constant-time` |

### build-systems

Build systems turn source into binaries: CMake diagnostics, toolchain drift, linker errors.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `build-linker-error-diagnostics` | Use when linking fails: undefined references, symbol/ABI mismatches, archive pull-in and --whole-archive behavior, wrong-mangled or versioned symbols, or massive undefined-ref cascades. Teaches reading symbol tables (nm/objdump/readelf) instead of guessing which flag to add. | unique | source-backed | `skills/build-systems/build-linker-error-diagnostics` |
| `build-process-signal-and-state-safety` | Use when a build may not have actually run or succeeded: signals killing ninja/make mid-write, corrupted .ninja_deps, sandbox no-ops that exit 0, ignored build exit codes. Teaches exit-code checks, output-change verification, ninja state inspection/repair, and write-only-after-success discipline. | unique | researched | `skills/build-systems/build-process-signal-and-state-safety` |
| `build-system-cmake-diagnostics` | Use when a CMake build fails or a dependency is misdeclared: missing imported targets, find_package errors, hand-rolled include/link paths, wrong target_link_libraries semantics. Teaches diagnosing the target/dependency graph instead of inventing versions, paths, or home-cooked logic. | unique | source-backed | `skills/build-systems/build-system-cmake-diagnostics` |
| `build-toolchain-version-drift` | Use when a build fails or "misbehaves" due to compiler, libstdc++/ABI, or -std= version drift, or when -O levels produce identical binaries. Teaches pinning the standard, dumping compiler macros, and proving which flags actually reached the compile command. | unique | source-backed | `skills/build-systems/build-toolchain-version-drift` |

### debugging

Crash triage and instrumentation-over-reasoning — log before you guess.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `debugging-crash-triage-discipline` | Use when debugging a crash, segfault, or access violation in C/C++ or low-level code. Guides reliable reproduction, capturing backtraces and registers, deciding whether the crash site is the bug or corruption surfaces later, and verifying the fix. Prevents merry-go-round hypothesis churn and false "fixed" verdicts. | unique | source-backed | `skills/debugging/debugging-crash-triage-discipline` |
| `debugging-instrumentation-over-reasoning` | Use when a bug resists a debugger and pure reasoning: repeated full runs, truncated traces, or fixes aimed at the wrong object. Replace speculation with deterministic append-only file instrumentation, entry logging, checkpoints, counters, and before/after values, and keep the log as evidence. | unique | source-backed | `skills/debugging/debugging-instrumentation-over-reasoning` |


## Security & hardware

### security

Side-channel constant-time verification, formal spec loop invariants, SMT/Z3 sound usage.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `capability-based-security` | Use when designing or reviewing access control that must avoid ambient authority — capability systems (seL4 cspace, CHERI capabilities, object capabilities), delegation, confinement, revocation, and least privilege. Teaches what makes a true capability, how to audit capability flows, and how to design unforgeable, revocable, confined grants. | unique | researched | `skills/security/capability-based-security` |
| `formal-spec-loop-invariants` | Use when writing or reviewing formal specifications, ACSL contracts, Kani annotations, or CBMC/Frama-C proofs of C/Rust loops: loop invariants, pre/postconditions, and vacuous specifications. Prevents vacuous or wrong invariants that pass provers and prove nothing, and inheriting implementation bugs into specs. Requires checking inductiveness and implication, not just prover success. | unique | researched | `skills/security/formal-spec-loop-invariants` |
| `formal-verification-kani-verus` | Use when deciding to formally verify Rust or C code — writing Kani proof harnesses, Verus proofs, CBMC/Frama-C specs, loop invariants, and interpreting results without overclaiming. Teaches what model checkers and SMT-based provers actually prove, how to write non-vacuous harnesses, and how to tell "no counterexample" from "verified". | unique | researched | `skills/security/formal-verification-kani-verus` |
| `invariant-identification` | Use when writing verification harnesses (Kani, CBMC, Frama-C) or asserting program properties. Teaches extracting real invariants from code, building inductive loop invariants, and avoiding non-inductive or vacuous assertions that pass but prove nothing. | unique | researched | `skills/security/invariant-identification` |
| `side-channel-constant-time-verification` | Use when writing or reviewing cryptographic or security-critical code that handles secrets: timing-leak audits, constant-time compares, secret-indexed lookups, division-timing. Prevents early-exit memcmp, secret-derived branches and indices, and value-dependent division in C, C++, and Rust. Requires measuring timing, not just reading source. | improved | source-backed | `skills/security/side-channel-constant-time-verification` |
| `side-channel-mitigation` | Use when choosing or reviewing countermeasures against side-channel leaks — constant-time vs masking vs blinding, cache/timing/power/EM channels, speculative-execution leaks (Spectre, Meltdown, MDS), and verifying a mitigation actually closes the channel. Teaches threat-model-driven countermeasure selection and evidence-based verification. | improved | source-backed | `skills/security/side-channel-mitigation` |
| `smt-z3-sound-usage` | Use when checking facts with SMT solvers (Z3, cvc5) or reviewing "prover passed" claims: feeding axioms to Z3, reading sat/unsat/model() results, and symbolic protocol-verification confidence. Prevents unsound axioms as evidence, "prover passed" overclaims, and solver confidence as correctness. Requires validating axioms against the real system and checking counterexamples. | unique | researched | `skills/security/smt-z3-sound-usage` |

### hdl

Clock domain crossing audit, timing closure, and constraint authoring for FPGA design.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `hdl-cdc-audit` | Use when reviewing or fixing clock domain crossing in HDL: classifying the crossing, choosing the right synchronization (2-FF for single-bit, gray-code/handshake/FIFO for multi-bit), and rejecting constraints that hide a CDC bug instead of fixing it. Prevents per-bit 2-FF on multi-bit buses and single-FF "synchronizers". | unique | researched | `skills/hdl/hdl-cdc-audit` |
| `hdl-constraints-authoring` | Use when writing, reading, or triaging SDC timing constraints for FPGA/ASIC flow: create_clock, input/output delays, false/multicycle paths, and lint triage of constraint-vs-netlist mismatches. Prevents duplicate clocks, blanket exceptions, unconstrained paths, and constraint/RTL object mismatches. | unique | researched | `skills/hdl/hdl-constraints-authoring` |
| `hdl-timing-closure` | Use when closing timing or judging a timing report in FPGA/ASIC flow: distinguishing real fixes (pipelining, retiming, shorter logic) from report-hiding (false_path on a real path, loosened clock, max_delay crutch). Prevents certifying a green WNS that hides a real setup violation. | unique | researched | `skills/hdl/hdl-timing-closure` |


## Accelerators

### accelerator

AI accelerator pipeline programs: cross-unit (DMA/vector/matrix/scalar) synchronization and barrier coverage on shared on-chip buffers.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `accelerator-pipeline-synchronization` | Use when writing or reviewing AI-accelerator pipeline programs (DMA, vector, matrix, scalar units on shared on-chip buffers): checking barrier/sync coverage across units, because missing or misplaced synchronization escapes simulation and golden testing. Teaches happens-before reasoning over cross-unit write-read pairs. | unique | researched | `skills/accelerator/accelerator-pipeline-synchronization` |


## Design

### design

Designer-mode skills for AI agents: design tokens (DTCG), typography hierarchy, WCAG 2.2 color/contrast accessibility, layout grids and reflow, visual hierarchy, and anti-AI-look originality review.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `design-anti-ai-look-originality-review` | Use when reviewing generated UI for generic AI styling or when asked to make a design feel original. Teaches fingerprinting AI-look markers, a uniqueness self-test, brand grounding, and spending boldness in one place. | unique | source-backed | `skills/design/design-anti-ai-look-originality-review` |
| `design-color-contrast-wcag-a11y` | Use when choosing color pairs or verifying accessibility of text, UI, and images. Teaches WCAG 2.2 as a design contract: relative luminance and contrast math, 4.5:1/3:1 thresholds, alt/labels/lang, 24px targets, axe-core/Lighthouse verification. | improved | source-backed | `skills/design/design-color-contrast-wcag-a11y` |
| `design-layout-spacing-grid` | Use when building or reviewing page layout, spacing, and responsive grids. Teaches a 4/8pt spacing scale, 12-column grids with consistent gutters, WCAG 1.4.10 reflow at 320px, and eliminating arbitrary px margins. | improved | source-backed | `skills/design/design-layout-spacing-grid` |
| `design-token-system-discipline` | Use when building or reviewing design tokens, theme files, or component CSS that hard-codes colors or spacing. Teaches DTCG format ($type/$value, aliases, composite tokens), semantic vs primitive layers, and token-first CSS without raw hex or px literals. | improved | source-backed | `skills/design/design-token-system-discipline` |
| `design-typography-hierarchy` | Use when choosing type sizes, weights, or font stacks, or when reviewing heading structure. Teaches modular type scales and clamp(), display vs body roles, weight contrast, ≤3 sizes, WCAG text spacing, and never skipping heading levels. | improved | source-backed | `skills/design/design-typography-hierarchy` |
| `design-visual-hierarchy-composition` | Use when structuring a page or evaluating hierarchy and composition. Teaches NN/g visual hierarchy: exactly one h1, heading order, squint test, F-pattern, hero-as-thesis, and key content in the first two paragraphs. | common | source-backed | `skills/design/design-visual-hierarchy-composition` |


---

Generated by `tools/reports/gen_skills_catalog.py` from `registry/skills.yaml`.
