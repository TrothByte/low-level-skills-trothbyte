# Low-level skills TrothByte — Skill Catalog

All **60 skills** in one place. 47 are source-backed (verified with real toolchains); the rest are researched and honestly marked. For orientation by domain, see the domain READMEs under `skills/`; for triggers and rules, open each skill's `SKILL.md`.

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
| `rust-ffi-boundary` | Use when writing or reviewing Rust FFI code — repr(C) layout, enum discriminants, CString/CStr and Box::into_raw ownership, extern "C" callbacks, opaque handles, or panic/unwind at the boundary. Teaches Rust-specific rules for safe C interop and how to verify layout and ownership on both sides. | improved | source-backed | `skills/rust/rust-ffi-boundary` |
| `rust-panic-safety` | Use when writing, reviewing, or debugging Rust code where a panic may be reachable — unwrap/expect on untrusted input, unwinding through extern "C" exports, catch_unwind boundaries, RefCell/Mutex poisoning, Drop during unwind, or panic=abort vs panic=unwind. Teaches panic reachability and unwind discipline as a single reasoning model. | improved | source-backed | `skills/rust/rust-panic-safety` |
| `rust-unsafe-reasoning` | Use when writing, reviewing, or debugging Rust code that uses unsafe blocks — raw pointers, transmute, MaybeUninit, Box::from_raw/into_raw, unsafe impl Send/Sync, or FFI-adjacent code — to reason about validity invariants, aliasing, and pointer provenance, and to detect undefined behavior that compiles and runs but is wrong. | improved | source-backed | `skills/rust/rust-unsafe-reasoning` |

### concurrency

Memory ordering, the atomics API, and lock/condvar discipline.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `atomics-c11-cpp11-rust` | Use when writing, reviewing, or porting atomic code in C11, C++20, or Rust — API lookup for _Atomic / std::atomic / std::sync::atomic, compare_exchange semantics (expected in-out, weak spurious failure), memory_order/Ordering validity, lock-free guarantees, and cross-language porting. | common | source-backed | `skills/concurrency/atomics-c11-cpp11-rust` |
| `concurrency-condvar-and-spurious-wakeup` | Use when writing, reviewing, or debugging condition-variable code (std::condition_variable, C11 cnd_*) — pairing wait with a predicate and mutex, handling spurious and lost wakeups (CON36-C), choosing notify_one vs notify_all, or fixing a program that occasionally hangs. | common | source-backed | `skills/concurrency/concurrency-condvar-and-spurious-wakeup` |
| `concurrency-deadlock-and-lock-ordering` | Use when writing or reviewing code that acquires two or more locks — detecting ABBA deadlock, enforcing consistent lock ordering (CON35-C), choosing std::lock/std::scoped_lock, avoiding recursive/try_lock hazards, or interpreting a TSan/helgrind lock-order report. | common | source-backed | `skills/concurrency/concurrency-deadlock-and-lock-ordering` |
| `memory-ordering-reasoning` | Use when reasoning about atomic operations, memory ordering, happens-before, or lock-free synchronization in C11/C++11/Rust — choosing Relaxed vs Acquire/Release vs SeqCst, diagnosing "compiles but races at runtime", or explaining why ordering matters across architectures (x86 vs ARM). Teaches the ordering model and how to verify it. | improved | source-backed | `skills/concurrency/memory-ordering-reasoning` |


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
| `asm-calling-conventions` | Use when writing or reviewing assembly, inline asm, or FFI code that calls or defines functions on x86-64 (SysV or Windows), AArch64, or RISC-V, or when reading disassembly and predicting argument registers, shadow space, stack alignment, callee-saved sets, and prologue shape. | common | source-backed | `skills/assembly/asm-calling-conventions` |
| `asm-inline-asm-constraints` | Use when writing, reviewing, or porting inline assembly in C, C++, or Rust — GCC extended asm constraints, memory and register clobbers, asm goto, and Rust asm! operand classes. Teaches declaring every asm side effect so the optimizer cannot miscompile around it. | common | source-backed | `skills/assembly/asm-inline-asm-constraints` |
| `asm-optimizer-artifacts` | Use when reading compiler-generated assembly (gcc/clang -O2 -S, objdump, Godbolt) and explaining why machine code diverges from C source — inlining, tail calls, dead-code elimination, constant folding, lea strength reduction, RIP-relative addressing. Teaches spotting optimizer artifacts without misreading them as missing code. | common | source-backed | `skills/assembly/asm-optimizer-artifacts` |
| `asm-signed-unsigned-branches` | Use when writing or reading x86-64 assembly, inline asm, or disassembly where signed vs unsigned semantics decide the instruction — jl/jg/jge/jle vs jb/ja/jae/jbe, sar vs shr, cdq/idiv vs xor/div, movsx vs movzx. Teaches flag semantics and how compilers select branch mnemonics from C types. | common | source-backed | `skills/assembly/asm-signed-unsigned-branches` |
| `asm-x86-64-registers-and-addressing` | Use when reading, writing, or reviewing x86-64 assembly: choosing register widths, operand-size suffixes, addressing modes, RIP-relative operands, REX encoding, canonical 48-bit addresses, or zero/sign extension. Prevents wrong-size moves, stale-flag branches, and non-canonical-address bugs in hand-written asm and inline asm. | common | source-backed | `skills/assembly/asm-x86-64-registers-and-addressing` |

### abi

Struct layout and argument passing for SysV AMD64, AAPCS64, and RISC-V — verified with the compiler.

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
| `kernel-atomic-context` | Use when writing, reviewing, or debugging Linux kernel code that runs in atomic context: interrupt handlers, bottom halves, spinlock-held or preemption-disabled regions. Covers what is forbidden there (sleeping kmalloc/mutex/schedule), GFP_ATOMIC, irqsave/bh lock variants, deferring to process context, and verifying with lockdep. | common | source-backed | `skills/kernel/kernel-atomic-context` |
| `kernel-rcu-memory-barriers` | Use when writing or reviewing Linux kernel code that needs memory barriers, READ_ONCE/WRITE_ONCE, or RCU — publish-subscribe patterns, rcu_assign_pointer/rcu_dereference, synchronize_rcu, or atomic-context rules like no sleeping in spinlocks. Teaches the kernel memory model and why it differs from C11 atomics. | cross-layer | source-backed | `skills/kernel/kernel-rcu-memory-barriers` |
| `kernel-uaccess-safety` | Use when writing, reviewing, or fixing Linux kernel driver code that exchanges data with user space — read/write, ioctl, mmap, poll/fasync, copy_to_user/copy_from_user, get_user/put_user, access_ok, compat_ioctl. Teaches fault-safe copying, size validation, and the -EFAULT/-ENOTTY/SIGSEGV symptom classes. | common | source-backed | `skills/kernel/kernel-uaccess-safety` |

### networking

The eBPF verifier model.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `ebpf-verifier-reasoning` | Use when writing, reviewing, or debugging eBPF C programs — map_lookup_elem null checks, bounded loops, pointer arithmetic on packet/map/stack pointers, helper argument constraints, and reading verifier rejection logs. Teaches how the kernel verifier proves safety and which patterns it rejects at load time. | improved | researched | `skills/networking/ebpf-verifier-reasoning` |

### embedded

Volatile/MMIO ordering, interrupts, linker scripts, MPU/TrustZone, RTOS ISR discipline.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `embedded-interrupt-and-nested` | Use when writing, reviewing, or debugging bare-metal Cortex-M firmware that uses interrupts — NVIC enable/pending/priority, exception vector table, nesting and preemption, PRIMASK critical sections with __disable_irq/__enable_irq, interrupt latency, and sharing state between ISR and main context. Teaches Cortex-M interrupt discipline and host verification. | common | source-backed | `skills/embedded/embedded-interrupt-and-nested` |
| `embedded-linker-script` | Use when writing, reviewing, or debugging a bare-metal embedded GNU ld linker script — MEMORY regions, SECTIONS placement, FLASH/RAM mapping, KEEP() on the vector table, startup copy loops with __etext/__data_start/__bss_start, ALIGN(), the location counter, and why firmware fails to boot or starts with stale .data. | common | source-backed | `skills/embedded/embedded-linker-script` |
| `embedded-mpu-trustzone` | Use when writing, reviewing, or debugging Cortex-M firmware that configures the MPU (ARMv7-M RBAR/RASR or ARMv8-M RBAR/RLAR regions, PRIVDEFENA background region, AP privilege) or ARMv8-M TrustZone (SAU regions, NSC veneers, secure/non-secure transitions, flash/SRAM partitioning), or when secure software faults on non-secure calls. | unique | source-backed | `skills/embedded/embedded-mpu-trustzone` |
| `embedded-volatile-and-memory-ordering` | Use when writing, reviewing, or debugging embedded C that accesses memory-mapped I/O (MMIO) registers or shares flags with an interrupt handler — volatile vs non-volatile access, why -O2 changes behavior, why volatile is not atomic, device vs normal memory attributes, and barriers for ordering. Teaches volatile rules and verification. | common | source-backed | `skills/embedded/embedded-volatile-and-memory-ordering` |
| `rtos-concurrency-and-isr` | Use when writing, reviewing, or debugging FreeRTOS/Zephyr firmware with tasks, queues, semaphores, mutexes, or interrupt handlers — blocking calls in ISRs, ISR-safe FromISR APIs, priority inversion and inheritance, task vs ISR context, periodic task timing, and stack sizing. Teaches context-correct RTOS usage and verification. | unique | source-backed | `skills/embedded/rtos-concurrency-and-isr` |

### bootloader

Boot stages and relocation.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `bootloader-stages` | Use when writing or debugging bootloaders and firmware — BIOS/UEFI stage-1 loading, MBR/GPT, x86 real to protected to long mode (A20, GDT, CR0/CR4/EFER, paging), AArch64 and RISC-V boot protocols, and link-address vs load-address relocation at stage-2 entry. | unique | source-backed | `skills/bootloader/bootloader-stages` |

### qemu

System emulation for kernel, firmware, and bare-metal verification.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `qemu-system-setup` | Use when setting up QEMU system emulation to boot a Linux kernel, firmware, or bare-metal ELF for x86-64, ARM Cortex-M, or AArch64 — machine model selection, -kernel/-nographic/-drive/netdev, serial console, and gdb remote debugging. | common | researched | `skills/qemu/qemu-system-setup` |


## Analysis & performance

### binary-analysis

Type recovery from disassembly.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `binary-analysis-type-recovery` | Use when recovering C types (char, short, int, long, float, double, structs, arrays, pointers, function/vtable signatures) from x86-64 disassembly via instruction-width and addressing patterns (movzx/movsx, movss/movsd, movl/movq, disp(%reg), scale indexing), validated with DWARF when present. | common | source-backed | `skills/binary-analysis/binary-analysis-type-recovery` |

### reverse-engineering

Go/Rust binaries and automated protocol RE.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `auto-re-protocols-beyond-can` | Use when reverse-engineering a binary protocol from captured bytes — UART/serial frames, industrial fieldbus or CAN-style traffic — where the wire format is unknown and must be recovered from evidence. Applies the deterministic pipeline: capture, survey, correlate, bit/field search, schema, verify-as-gate, instead of guessing field boundaries. | unique | source-backed | `skills/reverse-engineering/auto-re-protocols-beyond-can` |
| `go-rust-re` | Use when analyzing or reverse-engineering Go or Rust binaries — recovering function names from .gopclntab, decoding mangled Rust symbols (_ZN/_R), finding panic strings and core::fmt literals in .rodata/.rdata, distinguishing Rust from C/C++, or triaging stripped Go/Rust executables with GoReSym, redress, objdump, gdb, and Delve. | improved | source-backed | `skills/reverse-engineering/go-rust-re` |

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
| `gpu-memory-model-coherence` | Use when writing, reviewing, or debugging CUDA/HIP kernels where memory coherence matters — shared-memory races without __syncthreads(), relaxed atomics used as synchronization, cross-block visibility without __threadfence(), volatile for inter-thread sync, or host-device ordering. Teaches GPU memory hierarchy, coherence scopes, and correct synchronization. | common | researched | `skills/gpu/gpu-memory-model-coherence` |
| `ptx-assembly` | Use when writing, reading, or reviewing NVIDIA PTX assembly: state spaces, register types, memory loads/stores with scope and ordering, predication, bar.sync barriers, atomics, and warp-level shfl/vote, or when mapping between CUDA C++ and PTX/SASS. Teaches correct PTX syntax and GPU memory-model semantics. | unique | researched | `skills/gpu/ptx-assembly` |


## Tooling & agent behavior

### sanitizers

The agent CI loop and reading sanitizer reports.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `sanitizer-agent-ci-loop` | Use when integrating sanitizers (ASan/UBSan/TSan/MSan) into an agent's build-and-test loop for C/C++/Rust — so every change is automatically checked, reports are parsed and deduplicated, and regressions are caught. Fills the gap where "how to use sanitizers" exists but the universal agent loop does not. | unique | source-backed | `skills/sanitizers/sanitizer-agent-ci-loop` |
| `sanitizer-report-reading` | Use when interpreting sanitizer output — ASan/UBSan/TSan/MSan/LSan reports from builds, CI logs, or fuzzing — to identify bug category, access site versus allocation/free site, root cause, and fix. Triggers on report headers like 'ERROR: AddressSanitizer', shadow bytes, 'data race', or 'use-of-uninitialized-value'. | improved | researched | `skills/sanitizers/sanitizer-report-reading` |

### _meta

Agent behavior: routing, evidence, verification, assumptions, rationalizations, completion — plus the flagship cross-layer skills.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `meta-assumptions` | Use when code correctness depends on implicit assumptions — compiler, ABI, platform, memory model, optimization level, or endianness. Forces surfacing and documenting every assumption before concluding. | common | researched | `skills/_meta/meta-assumptions` |
| `meta-completion` | Use before declaring a low-level task complete. Enforces honest completion criteria: verifiable success, no hidden partial results, updated state files, and explicit uncertainty. | common | researched | `skills/_meta/meta-completion` |
| `meta-evidence` | Use whenever making a normative or factual claim about C/C++/Rust/asm/ABI/UB/compiler behavior. Enforces the KNOWN / INFERRED / UNVERIFIED classification and requires source-backed evidence for strong claims. | common | researched | `skills/_meta/meta-evidence` |
| `meta-rationalizations` | Use during code review or self-review to catch and reject rationalizations that excuse unsafe or incorrect low-level code. Contains the "Rationalizations to Reject" list derived from trailofbits and failure modes B1-B22. | common | researched | `skills/_meta/meta-rationalizations` |
| `meta-routing` | Use at the start of any low-level task to choose the minimal relevant skill set. Prevents "load everything" behavior, enables dependency expansion, and routes to the correct skill from the registry. | common | researched | `skills/_meta/meta-routing` |
| `meta-verification` | Use before concluding that low-level code is correct or that a bug is found. Enforces executable verification (compile+run, sanitizers, asm inspection, debugger) instead of "it compiles" or "tests pass". | common | researched | `skills/_meta/meta-verification` |
| `safe-low-level-from-scratch` | Use when writing NEW low-level code (C/C++/Rust/asm) from scratch that must be memory-safe and correct across optimization levels and platforms. Provides the positive writing process integrating UB semantics, layout/alignment, ownership, atomics, and FFI, with verification gates at each step. | cross-layer | source-backed | `skills/_meta/safe-low-level-from-scratch` |
| `wasm-runtime-from-scratch` | Use when writing, reviewing, or debugging a WebAssembly runtime, interpreter, loader, or validator in C — module binary parsing, validation, linear memory bounds, tables and call_indirect, traps vs undefined behavior, memory.grow, and host function imports. | unique | source-backed | `skills/_meta/wasm-runtime-from-scratch` |
| `zeroize-constant-time` | Use when writing or reviewing code handling secrets (keys, passwords, nonces) that must be zeroized or compared in constant time. Triggers on memset-before-return, secret-dependent branches or indexing, memcmp on secrets, and claims that a secret is cleared. Teaches volatile-sink zeroization, explicit_bzero, ct_memcmp, and asm verification. | improved | source-backed | `skills/_meta/zeroize-constant-time` |


---

Generated by `tools/reports/gen_skills_catalog.py` from `registry/skills.yaml`.
