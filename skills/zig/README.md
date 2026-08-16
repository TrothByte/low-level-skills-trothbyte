# zig — Skills

Zig gives explicit control over build, allocator, comptime, and FFI semantics.

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

## How to use

- Read the `SKILL.md` of a skill for its triggers, reasoning rules, and verification commands.
  (`references/` hold deep knowledge; `examples/good` and `examples/bad` are compiled/run
  fixtures; `evals/README.md` defines eval cases.)
- Load only the skill you need (see `skills/_meta/meta-routing`; references load on demand.

## Related

[Back to repository root](../../README.md)
