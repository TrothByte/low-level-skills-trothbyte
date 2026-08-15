# Zig Inline Assembly and ABI — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE(bad) →
COUNTEREXAMPLE(good) → VERIFICATION → SOURCE. Source ids refer to registry/sources.yaml.
Version markers: KNOWN / INFERRED / UNVERIFIED.

## 1. Inline assembly syntax and volatile

- **RULE**: inline asm is an expression: `asm [volatile] ("asm" : outputs : inputs :
  clobbers)`. Without `volatile`, Zig may delete the block if its result is unused.
  `%[name]` refers to a named constraint; a literal `%` is `%%`.
- **WHY AI GETS IT WRONG**: omits `volatile` on a side-effecting block, then at -O2 the
  block disappears and the agent "fixes" it by disabling optimization.
- **CORRECT REASONING**: `volatile` tells Zig the asm has side effects beyond the
  declared outputs. Syscall wrappers, MMIO writes, and any asm whose output is discarded
  must be `volatile`.
- **EXAMPLE** (bad):
  ```zig
  fn doSyscall(number: usize) usize {
      return asm ("syscall"            // no volatile: deletable if unused
          : [ret] "={rax}" (-> usize),
          : [number] "{rax}" (number),
          : .{ .rcx = true, .r11 = true });
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  fn doSyscall(number: usize) usize {
      return asm volatile ("syscall"
          : [ret] "={rax}" (-> usize),
          : [number] "{rax}" (number),
          : .{ .rcx = true, .r11 = true });
  }
  ```
- **VERIFICATION**: `zig test examples/good/syscall.zig -target x86_64-linux`; compare
  -O0 and -O2 disassembly to confirm the block survives.
- **SOURCE**: zig-langref §Assembly; zig-release-notes 0.15.x (typed clobbers example).

## 2. Clobbers are typed (0.15+); missing clobbers are unchecked Illegal Behavior

- **RULE**: since 0.15.0, clobbers are typed struct literals — `: .{ .rcx = true, .r11 =
  true }` — replacing the stringly-typed list `: "rcx", "r11"`. Failure to declare the
  full clobber set is unchecked Illegal Behavior: the code compiles and silently corrupts.
  Output/input registers must NOT be listed as clobbers.
- **WHY AI GETS IT WRONG**: writes `: "rcx", "r11"` from 0.14 memory; or lists only
  inputs/outputs and misses that `syscall` clobbers `rcx` and `r11`.
- **CORRECT REASONING**: enumerate the registers the instruction trashes that are neither
  inputs nor outputs. The "memory" clobber means arbitrary undeclared memory is written.
  A wrong clobber set is UB — the compiler assumes preserved registers.
- **EXAMPLE** (bad, 0.15+):
  ```zig
  return asm volatile ("syscall"
      : [ret] "={rax}" (-> usize),
      : [number] "{rax}" (number),
      : "rcx", "r11"); // error on 0.15+: expected typed clobbers
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  return asm volatile ("syscall"
      : [ret] "={rax}" (-> usize),
      : [number] "{rax}" (number),
      : .{ .rcx = true, .r11 = true });
  ```
- **VERIFICATION**: `zig test examples/bad/string_clobbers.zig` fails on 0.15+;
  `zig test examples/good/syscall.zig` passes. A missing-clobber variant compiles and is
  caught only by review (runtime corruption — UNVERIFIED exact symptom).
- **SOURCE**: zig-langref §Assembly (Clobbers); zig-release-notes 0.15.x "Inline
  Assembly: Typed Clobbers".

## 3. x86/x86-64 inline asm uses AT&T syntax

- **RULE**: on x86 and x86_64 targets the inline assembly string is AT&T syntax (source,
  dest order; `%rax`; `lea (%rdi,%rsi,1),%eax`). Intel syntax is not used due to LLVM
  parser limitations.
- **WHY AI GETS IT WRONG**: writes `mov eax, 1` or `mov [rdi], rax` inside the string and
  gets an assembler error, or swaps the operands of AT&T instructions.
- **CORRECT REASONING**: AT&T: `movq %rsi, %rdi` copies rsi→rdi; `lea (%rdi,%rsi,1),
  %eax` computes rdi+rsi. Escaping: `%%` for a literal `%`.
- **EXAMPLE** (bad):
  ```zig
  return asm volatile ("mov eax, 1" : [ret] "={rax}" (-> usize));
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  return asm volatile ("mov $1, %[ret]"
      : [ret] "=r" (-> usize)); // AT&T immediate and destination order
  ```
- **VERIFICATION**: `zig test`; objdump the object to confirm the encoding matches intent.
- **SOURCE**: zig-langref §Assembly; binutils-docs (GNU as AT&T syntax).

## 4. Global assembly

- **RULE**: an `asm` in a namespace-level `comptime` block is global assembly: no
  `volatile` (it is always included), no inputs/outputs/clobbers, concatenated verbatim.
- **WHY AI GETS IT WRONG**: writes `asm volatile` at namespace level, or declares
  clobbers on global asm.
- **CORRECT REASONING**: global asm is spliced into the object file; use it to define
  raw functions/symbols and reference them with `extern fn`.
- **COUNTEREXAMPLE** (good):
  ```zig
  comptime {
      asm (
          \\.global my_add;
          \\.type my_add, @function;
          \\my_add:
          \\  lea (%rdi,%rsi,1),%eax
          \\  retq
      );
  }
  extern fn my_add(a: i32, b: i32) i32;
  ```
- **VERIFICATION**: `zig test examples/good/global_asm.zig -target x86_64-linux -fllvm`
  (master requires `-fllvm` for this on x86_64-linux — KNOWN from langref test command).
- **SOURCE**: zig-langref §Assembly (Global Assembly).

## 5. Calling conventions and the SysV AMD64 ABI

- **RULE**: `callconv(.c)` follows the C ABI; other conventions exist per target
  (`.winapi`, `.naked`, `.sysv` in 0.16+ naming; `inline fn` forces inlining). Per
  sysv-amd64-abi §3.2: integer args in `rdi, rsi, rdx, rcx, r8, r9`, stack 16-byte
  aligned, return in `rax`. `extern "lib" fn` and `export fn` default semantics must be
  pinned to a version (INFERRED: `.x86_64`/`.win64` were the pre-0.16 names).
- **WHY AI GETS IT WRONG**: assumes args land in `rax`/`rbx`; writes a `.c` export without
  considering callee-saved registers; uses a convention name removed in 0.16.
- **CORRECT REASONING**: for SysV x86-64 the argument registers are rdi,rsi,rdx,rcx,r8,r9;
  rbx/rbp/r12-r15 are callee-saved. `callconv(.naked)` removes prologue/epilogue (for
  hand-written `_start`). Check the pinned langref for the exact convention enum names.
- **EXAMPLE** (bad):
  ```zig
  extern "c" fn putchar(c: u8) c_int;   // fine
  export fn sum(a: i32, b: i32) i32 {   // ok, .c implied
      return a + b;
  }
  // BUT hand-written asm assuming args in %rbx or %rcx violates SysV
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  extern "c" fn memcpy(dst: [*]u8, src: [*]const u8, n: usize) [*]u8;
  const SysV = std.builtin.CallingConvention.sysv; // name per pinned version (INFERRED)
  ```
- **VERIFICATION**: compile + objdump; confirm the wrapper passes args in the SysV
  registers and that the exported symbol uses the C ABI.
- **SOURCE**: sysv-amd64-abi §3.2; zig-langref §Functions (extern, export, callconv).

## 6. @export / @extern and symbol naming

- **RULE**: `@export(&sym, .{ .name = "foo", .linkage = .strong })` creates an object
  symbol at comptime; equivalent to `export fn foo() callconv(.c) void`. `@extern(T, .{
  .name = "bar" })` creates a reference to an external symbol. `@"any string"` identifiers
  select arbitrary symbol names.
- **WHY AI GETS IT WRONG**: uses `@export` on a non-C-ABI function without `callconv(.c)`;
  forgets the pointer must point at a global or comptime-known constant; invents
  ExternOptions fields.
- **CORRECT REASONING**: exported symbols intended for C or other languages must use the C
  ABI (`callconv(.c)` or the `export` keyword which implies it). `@export` from a
  `comptime` block allows conditional exports.
- **EXAMPLE** (bad):
  ```zig
  comptime {
      @export(&internal, .{ .name = "api" }); // internal has no callconv(.c)
  }
  fn internal() void {}
  ```
- **COUNTEREXAMPLE** (good):
  ```zig
  comptime {
      @export(&internal, .{ .name = "api", .linkage = .strong });
  }
  fn internal() callconv(.c) void {}
  ```
- **VERIFICATION**: `zig build-obj` + `nm`/`objdump -t` shows the symbol with the C ABI;
  the bad variant compiles but callers from C would call with the wrong convention
  (review-time rule).
- **SOURCE**: zig-langref §Builtin Functions (@export, @extern); §Exporting a C Library;
  binutils-docs (nm/objdump).

## Quick reference table

| Topic | Rule in one line |
|---|---|
| asm expression | `asm [volatile] ("str" : outs : ins : clobbers)`; `%[name]`, `%%` for literal `%` |
| volatile | required when side effects or output is discarded; else DCE may remove it |
| clobbers 0.15+ | typed: `.{ .rcx = true, .r11 = true }`; string list is 0.14-era |
| missing clobbers | unchecked Illegal Behavior — compiles, corrupts at runtime |
| "memory" clobber | declare when asm writes arbitrary undeclared memory |
| x86 syntax | AT&T: `lea (%rdi,%rsi,1),%eax`; Intel syntax unsupported |
| global asm | `comptime { asm (\\...) }`; no volatile/inputs/outputs |
| callconv | `.c` for C ABI; `.naked` no prologue; SysV args rdi rsi rdx rcx r8 r9 |
| @export/@extern | export implies C ABI; `@"symbol name"` for arbitrary strings |
