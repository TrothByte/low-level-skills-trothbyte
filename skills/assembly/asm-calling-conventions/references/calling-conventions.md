# Calling Convention Reference Tables and Rules

Comparative reference for SysV AMD64, Windows x64, AAPCS64 (AArch64), and RISC-V
psABI (RV64). Facts marked VERIFIED were reproduced with MinGW GCC 16.1 on Windows
x64; facts marked DOCUMENTED-AS-TARGET come from the cited specs (no cross-compiler
available in this environment).

## 1. One-glance comparison

| Property | SysV AMD64 | Windows x64 | AAPCS64 (AArch64) | RISC-V psABI (RV64) |
|---|---|---|---|---|
| Integer arg regs | rdi, rsi, rdx, rcx, r8, r9 (6) | rcx, rdx, r8, r9 (4) | x0–x7 (8) | a0–a7 = x10–x17 (8) |
| FP/vector arg regs | xmm0–xmm7 (8) | xmm0–xmm3 (4) | v0–v7 (8) | fa0–fa7 = f10–f17 (8) |
| Stack args start at | rsp+8 (entry) | rsp+40 at call site (5th arg) | sp+0 after 8 regs (entry) | sp+0 after 8 regs (entry) |
| Caller stack alignment | rsp % 16 == 0 at call | rsp % 16 == 0 at call | sp 16-byte aligned | sp 16-byte aligned |
| Shadow space | none | 32 bytes every caller | none | none |
| Red zone | 128 bytes below rsp | none | none | none |
| Callee-saved GP | rbx, rbp, r12–r15 | rbx, rbp, rdi, rsi, r12–r15 | x19–x28, x29 (fp) | s0–s11 = x8–x9, x18–x27 |
| Return register | rax (int), xmm0 (FP), rax:rdx (128-bit) | rax, xmm0 | x0, v0 | a0, fa0 |
| Struct return (sret) | hidden ptr in rdi | hidden ptr in rcx | x8 | a0 (by value if fits) |
| Link register | stack (return addr pushed) | stack | x30 (lr) | ra = x1 |
| `long` size | 64-bit (LP64) | 32-bit (LLP64) | 64-bit (LP64) | 64-bit (LP64) |
| Object format | ELF | PE/COFF | ELF | ELF |
| Status | DOCUMENTED-AS-TARGET | VERIFIED | DOCUMENTED-AS-TARGET | DOCUMENTED-AS-TARGET |

## 2. Argument classification rules (register order)

- **SysV AMD64**: integer args use `rdi, rsi, rdx, rcx, r8, r9` in order; floating
  args use `xmm0–xmm7` in order, interleaved by position with the integer list. The
  7th integer and 9th SSE args go on the stack. Varargs: `AL` = number of vector
  registers used.
- **Windows x64**: integer args use `rcx, rdx, r8, r9`; floating args `xmm0–xmm3`;
  args 5+ on the stack, each 8 bytes, placed above the 32-byte shadow space. The
  caller allocates shadow space even for 0–4 arg calls (callee may spill there).
- **AAPCS64**: integer args `x0–x7`, FP/SIMD `v0–v7`, position-interleaved; args 9+
  on the stack at `sp` upward. `x8` receives the address for struct-return (sret).
- **RISC-V (lp64/lp64d)**: integer args `a0–a7`; FP args `fa0–fa7` in the `d`
  variants; args 9+ on the stack. `a0` returns values; `ra` (x1) holds the return
  address set by `jal`.

## 3. Rules

### R1: Integer argument registers depend on the ABI, never on "x86-64 habit"

- **RULE**: On x86-64, SysV passes the first 6 integer args in `rdi, rsi, rdx, rcx,
  r8, r9`; Windows x64 passes the first 4 in `rcx, rdx, r8, r9`. The same bytes of
  assembly are not portable.
- **WHY AI GETS IT WRONG**: most training material and Linux tutorials show SysV
  registers; the agent copies them into a Windows x64 binary where the caller put the
  values in `rcx/rdx` and the callee reads `rdi/rsi` (stale or garbage).
- **CORRECT REASONING**: identify the object format and OS first (PE → Windows, ELF →
  SysV unless AArch64/RISC-V). Then assign registers positionally from the target's
  list. A Windows callee must read its 1st/2nd args from `rcx`/`rdx`; a SysV callee
  from `rdi`/`rsi`.
- **EXAMPLE** (bad):
  ```asm
  # linked into a Windows x64 binary
  bad_sysv_add:      # long long f(long long a, long long b)
      leaq (%rdi,%rsi), %rax
      ret
  ```
- **COUNTEREXAMPLE** (good):
  ```asm
  win_add:           # long long f(long long a, long long b)
      leaq (%rcx,%rdx), %rax
      ret
  ```
- **VERIFICATION**: assembled and run with MinGW GCC 16.1: `bad_sysv_add(40,2)`
  returned garbage (observed 88 and 96 across builds — the value depends on whatever
  the caller left in rdi/rsi), `win_add(40,2)` returned 42.
- **SOURCE**: `sysv-amd64-abi` §3.2.3 (SysV int regs); Microsoft x64 calling
  convention (int regs rcx/rdx/r8/r9); `aapcs64` §6.1; `riscv-psabi` riscv-cc.adoc.

### R2: Windows x64 callers must reserve 32 bytes of shadow space

- **RULE**: before every call, a Windows x64 caller allocates 32 bytes
  (`subq $32` minimum) so the callee may spill its 4 register args at
  `rsp+0..31`. The 5th+ stack args go at `rsp+32` upward.
- **WHY AI GETS IT WRONG**: the agent writes a SysV-style call (no extra allocation,
  stack args at `rsp+0`) and the code "usually works" until a callee actually spills
  its registers, silently overwriting the caller's frame.
- **CORRECT REASONING**: the shadow space is part of the contract, not an
  optimization. The compiler allocates `subq $32` (plus stack-arg space and alignment
  padding) before every call; hand-written callers must do the same.
- **EXAMPLE** (bad):
  ```asm
  # caller has a local at -16(%rbp), calls with no shadow space:
  subq $16, %rsp
  movq %rax, -16(%rbp)
  movl $1, %ecx
  movl $2, %edx
  call callee_writes_shadow   # callee spills rcx/rdx at rsp+0..31
  movq -16(%rbp), %rax        # reads back 1, not the stored sentinel
  ```
- **COUNTEREXAMPLE** (good):
  ```asm
  subq $32, %rsp              # 32-byte shadow space
  movl $1, %ecx
  movl $2, %edx
  call callee_writes_shadow   # spills land in the shadow space, not on locals
  addq $32, %rsp
  ```
- **VERIFICATION**: verified with MinGW GCC 16.1 — the no-shadow caller's local
  `0x1122334455667788` was clobbered to `0x1`; the shadow-space caller kept it intact.
  Compiler check: `gcc -O0 -S` for any call site emits `subq $32, %rsp`.
- **SOURCE**: Microsoft x64 calling convention (stack allocation / shadow space);
  `sysv-amd64-abi` §3.2.3 (SysV has no shadow space — stack args at `rsp+8`).

### R3: `rsp % 16 == 0` at the call site is mandatory on x86-64

- **RULE**: SysV and Windows x64 both require the stack pointer to be 16-byte aligned
  at the instant of `call` (after pushing the 8-byte return address the callee's
  entry `rsp % 16 == 8`). AAPCS64/RISC-V keep `sp` 16-aligned at all times.
- **WHY AI GETS IT WRONG**: alignment is invisible in most runs; the agent allocates
  an arbitrary byte count (`subq $8`), or forgets that each `push` moves the pointer
  by 8, and only misalignment crashes — inside a callee using `movaps`, at `-O2`,
  on another machine.
- **CORRECT REASONING**: count stack motion in 8-byte units. Entry (x86-64):
  `rsp % 16 == 8` after the caller's call. `push` × k → `8k` bytes; `subq $N` → N
  bytes; total motion must keep `rsp % 16 == 0` right before the next `call`. Each
  `call` itself adds 8 more.
- **EXAMPLE** (bad):
  ```asm
  # caller_misaligned: entry rsp%16==8, pushq %rbp -> 0, subq $40 -> 8  -> BAD at call
  pushq %rbp
  movq %rsp, %rbp
  subq $40, %rsp
  pxor %xmm0, %xmm0
  call callee_movaps      # callee's movaps on a misaligned frame slot faults
  ```
- **COUNTEREXAMPLE** (good):
  ```asm
  pushq %rbp
  movq %rsp, %rbp
  subq $32, %rsp          # multiple of 16: stays %16==0
  pxor %xmm0, %xmm0
  call callee_movaps
  ```
- **VERIFICATION**: verified with MinGW GCC 16.1 — the `$40` caller triggered
  SIGSEGV/access-violation (`0xC0000005`) inside the callee's `movaps`; the `$32`
  caller ran clean. Count bytes: `subq` amounts and pushes must total a multiple of
  16 (given entry `rsp % 16 == 8` and the `call` itself).
- **SOURCE**: `sysv-amd64-abi` §3.2.2 ("end of input argument area aligned on a 16
  byte boundary"); Microsoft x64 calling convention (stack alignment); `aapcs64` §7
  (sp 16-byte aligned); `riscv-psabi` (stack 16-byte alignment, RV64).

### R4: Callee-saved register sets differ — preserving the wrong set breaks callers

- **RULE**: SysV callee-saved GP: `rbx, rbp, r12–r15`. Windows x64 adds `rdi, rsi`
  (they are arg/scratch on SysV but must be preserved on Windows) and `xmm6–xmm15`.
  AAPCS64: `x19–x28`, `x29` (fp), `d8–d15`. RISC-V: `s0–s11` (x8–x9, x18–x27).
- **WHY AI GETS IT WRONG**: the agent clobbers `rdi`/`rsi` in a Windows callee (fine
  on SysV, fatal on Windows), or saves `rax`/`rcx` (caller-saved everywhere — a waste
  that masks nothing), or omits `x19` on AArch64.
- **CORRECT REASONING**: treat the target's callee-saved list as a contract: any
  register on the list used by your function must be pushed in the prologue and popped
  in the epilogue in reverse order. Everything else is scratch.
- **EXAMPLE** (bad):
  ```asm
  # Windows x64 callee that uses rdi for a loop counter and never preserves it
  win_clobber_rdi:
      movq $0, %rdi
      loop:
      addq $1, %rdi
      cmpq $10, %rdi
      jne loop
      ret                 # caller's rdi (its 3rd/4th-arg backup) destroyed
  ```
- **COUNTEREXAMPLE** (good):
  ```asm
  win_safe:
      pushq %rdi          # Windows requires rdi preserved
      movq $0, %rdi
  loop:
      addq $1, %rdi
      cmpq $10, %rdi
      jne loop
      popq %rdi
      ret
  ```
- **VERIFICATION**: MinGW GCC 16.1 prologue for a function using rbx/r12–r15:
  `pushq %r15; pushq %r14; pushq %r13; pushq %r12; pushq %rbx` and symmetric pops.
  GCC never pushes `rdi`/`rsi` for SysV output but does for Windows output — the
  compiler is the oracle for the toolchain's ABI.
- **SOURCE**: `sysv-amd64-abi` §3.2.1 (rbp, rbx, r12–r15); Microsoft x64 calling
  convention (nonvolatile registers rbx, rbp, rdi, rsi, r12–r15, xmm6–xmm15);
  `aapcs64` §6.1.2 (r19–r29, d8–d15); `riscv-psabi` (s0–s11).

### R5: The red zone exists only on SysV AMD64

- **RULE**: SysV gives leaf functions a 128-byte red zone below `rsp` that they may
  use without `subq`; Windows x64, AAPCS64, and RISC-V have no red zone.
- **WHY AI GETS IT WRONG**: the agent writes a leaf that stores locals below `rsp`
  and calls it a portable trick, or subtracts an extra 128 bytes on Windows "just in
  case" (wasted but harmless), or relies on the red zone in a Windows signal/exception
  context where the OS clobbers it.
- **CORRECT REASONING**: red-zone use is legal only in a SysV leaf (no `call` between
  store and use, or the region survives only until the next `call`). In inline asm,
  prefer an explicit frame; the compiler generates red-zone code only for SysV.
- **EXAMPLE** (bad):
  ```asm
  # Windows x64 leaf storing locals below rsp — no red zone, corrupts own frame
  win_leaf:
      movq %rcx, -16(%rsp)   # below rsp: may be clobbered by the OS / exception
      movq -16(%rsp), %rax
      ret
  ```
- **COUNTEREXAMPLE** (good):
  ```asm
  # Windows x64 leaf with explicit frame
  win_leaf:
      subq $32, %rsp
      movq %rcx, 24(%rsp)
      movq 24(%rsp), %rax
      addq $32, %rsp
      ret
  ```
- **VERIFICATION**: MinGW GCC 16.1 never emits `movq ..., -N(%rsp)` without an
  `subq`; SysV GCC frequently does. Check with `gcc -S` on each target.
- **SOURCE**: `sysv-amd64-abi` §3.2.3 (red zone); Microsoft x64 calling convention
  (no red zone); `aapcs64` §7; `riscv-psabi`.

### R6: Varargs on SysV use `AL` = number of vector registers

- **RULE**: on SysV, when calling a variadic function, the caller sets `AL` to the
  number of vector (xmm) registers used for the FP arguments.
- **WHY AI GETS IT WRONG**: the agent writes `printf`-style varargs calls in asm and
  leaves `AL` unset; floating arguments are then passed correctly (in xmm regs) but
  the callee has no reliable way to know how many xmms to save, or the agent sets it
  to a fixed wrong value.
- **CORRECT REASONING**: count the FP/vector args actually placed in `xmm0–xmm7` and
  put that count in `al` immediately before the call (integer args follow the normal
  register list).
- **EXAMPLE** (bad):
  ```asm
  movl $3, %edi
  movsd .LC0(%rip), %xmm0    # one float vararg
  xorl %eax, %eax            # AL=0 while xmm0 is used
  call printf
  ```
- **COUNTEREXAMPLE** (good):
  ```asm
  movl $3, %edi
  movsd .LC0(%rip), %xmm0
  movl $1, %eax              # AL=1: one vector register used
  call printf
  ```
- **VERIFICATION**: compare with GCC output: `gcc -O2 -S` for a `printf(..., f)`
  call emits `movl $1, %eax` before `call printf`.
- **SOURCE**: `sysv-amd64-abi` §3.3.2 (AL set to number of vector registers used).

### R7: Vector/`__m128` arguments are passed differently on Windows vs SysV

- **RULE**: SysV passes SSE/vector args by value in `xmm0–xmm7` (16-byte regs);
  Windows x64 passes `__m128`/`__m256` arguments by hidden pointer (address in an
  integer register), and only scalar doubles/floats use `xmm0–xmm3`.
- **WHY AI GETS IT WRONG**: the agent assumes a `__m128` parameter is in `xmm0`
  everywhere, then dereferences nothing / reads the wrong value on Windows, where the
  callee must load from the pointer in `rcx`.
- **CORRECT REASONING**: classify by ABI first. On Windows, vector args are passed
  indirectly — the callee sees `(%rcx)`/`(%rdx)` and loads; on SysV they arrive in
  the vector registers directly.
- **EXAMPLE** (bad):
  ```asm
  # Windows x64: __m128 v(__m128 a, __m128 b) — a is a POINTER in rcx, not data
  win_vec_bad:
      movaps %xmm0, %xmm1    # xmm0 holds a pointer's value, not vector data
      ret
  ```
- **COUNTEREXAMPLE** (good):
  ```asm
  # Windows x64: load the vector from the hidden pointer
  win_vec_good:
      movups (%rcx), %xmm0
      movups (%rdx), %xmm1
      addps %xmm1, %xmm0
      ret
  ```
- **VERIFICATION**: MinGW GCC 16.1 at `-O2` for `__m128 f(__m128, __m128, __m128,
  __m128)` emitted `addps (%rcx), ...`, `addps (%r8), ...` — pointers, not registers.
- **SOURCE**: Microsoft x64 calling convention (vector types passed by reference);
  `sysv-amd64-abi` §3.2.3 (SSE class, `xmm0–xmm7`).

### R8: AAPCS64 return address lives in `x30` (LR); RISC-V in `ra` (x1)

- **RULE**: on AArch64 `bl` writes the return address to `x30`; on RISC-V `jal`/`jalr`
  writes it to `ra` (x1). A function that calls another must save its incoming
  LR/`ra` before the nested call (typically `stp x29, x30, [sp, #-16]!` / `addi sp`,
  `sd ra, ...`) and restore it before `ret`.
- **WHY AI GETS IT WRONG**: the agent writes a non-leaf function that uses `ret`
  without saving LR, so after the nested call `x30`/`ra` holds the inner return
  address and the outer function returns to the middle of the caller.
- **CORRECT REASONING**: treat `x30`/`ra` as a live value you must preserve across any
  `bl`/`jal`. Leaf functions (no nested calls) may leave it untouched.
- **EXAMPLE** (bad):
  ```asm
  # AArch64 non-leaf without saving LR
  outer_bad:
      bl inner        # clobbers x30
      ret             # returns to inner's continuation, not the caller
  ```
- **COUNTEREXAMPLE** (good):
  ```asm
  outer_good:
      stp x29, x30, [sp, #-16]!   # save fp + LR, keep sp 16-aligned
      mov x29, sp
      bl inner
      ldp x29, x30, [sp], #16     # restore, keep sp 16-aligned
      ret
  ```
- **VERIFICATION**: DOCUMENTED-AS-TARGET — generate with `aarch64-linux-gnu-gcc -S`
  or Godbolt (AArch64 gcc at `-O0` emits `stp x29, x30, [sp, #-16]!` for non-leaf
  functions). RISC-V analog: `sd ra, N(sp)` / `ld ra, N(sp)`.
- **SOURCE**: `aapcs64` §6.1.1 (x30 LR), §6.2.3 (stack usage, SP alignment);
  `riscv-psabi` (ra = x1, s0-s11, stack alignment).

### R9: Struct/indirect return registers

- **RULE**: for structs returned indirectly: SysV passes the hidden address in `rdi`
  (first arg slot) and returns it in `rax`; Windows x64 in `rcx` (returned in `rax`);
  AAPCS64 in `x8`; RISC-V passes and returns in `a0`. Small structs that fit are
  returned in registers by value per each ABI's classification rules.
- **WHY AI GETS IT WRONG**: the agent ignores the hidden pointer and treats the
  struct as a normal first argument, or returns the address in the wrong register
  (e.g. `x0` instead of `x8` on AArch64).
- **CORRECT REASONING**: if the return type is a struct/union, check whether it fits
  in the ABI's return registers (SysV: rax/rdx/xmm0/1; AAPCS64: x0/x1, v0/v1) or needs
  the sret pointer; use the sret register of the target.
- **EXAMPLE** (bad):
  ```asm
  # AArch64: struct S make(void) — address must come in x8
  make_bad:
      adrp x0, sret_storage
      add  x0, x0, :lo12:sret_storage   # fills x0 (the RESULT reg), ignores x8
      ret
  ```
- **COUNTEREXAMPLE** (good):
  ```asm
  make_good:
      adrp x8, sret_storage             # x8 = indirect result location
      add  x8, x8, :lo12:sret_storage
      mov  x0, x8                       # also return the address in x0
      ret
  ```
- **VERIFICATION**: DOCUMENTED-AS-TARGET — compile a struct-return function with
  `aarch64-linux-gnu-gcc -S` and check `mov x8, ...` before the body.
- **SOURCE**: `aapcs64` §6.3 (indirect result location x8); `sysv-amd64-abi` §3.2.3;
  `riscv-psabi`; Microsoft x64 calling convention.

### R10: `long` width differs (LLP64 vs LP64) and changes register-width and stack slots

- **RULE**: on Windows x64 `long` is 32-bit (LLP64); on SysV/AAPCS64/RISC-V it is
  64-bit (LP64). Writing ABI code against `long` couples it to the platform.
- **WHY AI GETS IT WRONG**: the agent writes a function taking `long` and assumes an
  8-byte register and 8-byte stack slot; on Windows the caller passes a 4-byte value
  and the 5th-arg offsets shift accordingly, or the agent spills `%eax` where the
  callee expects `%rax`.
- **CORRECT REASONING**: use explicitly sized types (`long long`, `int32_t`,
  `int64_t`) when the argument layout must be identical across ABIs; on Windows a
  `long` argument uses `ecx`/`edx`/`r8d`/`r9d` and a 4-byte stack slot.
- **EXAMPLE** (bad):
  ```asm
  # Windows x64, C prototype f(long, long): GCC spills 32-bit regs (movl %ecx,...)
  win_long_bad:
      movl %ecx, 16(%rbp)   # long is 32-bit on Windows
      ...
  ```
- **COUNTEREXAMPLE** (good):
  ```asm
  # Windows x64, prototype f(long long, long long): 64-bit regs
  win_ll_good:
      movq %rcx, 16(%rbp)
      movq %rdx, 24(%rbp)
      ...
  ```
- **VERIFICATION**: MinGW GCC 16.1 emitted `movl %ecx, 16(%rbp)` for a `long` param
  and `movq %rcx, 16(%rbp)` for `long long` — the observed split confirms LLP64.
- **SOURCE**: Microsoft x64 calling convention (LLP64, `long` = 32-bit);
  `sysv-amd64-abi` §3.2 (LP64); `aapcs64` §8 (LP64); `riscv-psabi` (LP64).

### R11: Identify the ABI by reading the prologue

- **RULE**: the argument registers, the presence of a 32-byte shadow allocation, the
  callee-saved set, and the object format identify the convention. On x86-64: first
  int args to `rcx,rdx,r8,r9` plus `subq $32,...` before calls → Windows x64; first
  int args to `rdi,rsi,rdx,rcx` with no shadow space → SysV. AArch64: `stp x29,x30`
  prologues, args in `x0–x7`, sret in `x8`. RISC-V: args in `a0–a7`, `jal`/`ret` and
  `sd ra`.
- **WHY AI GETS IT WRONG**: the agent reads one disassembly, assumes SysV, and
  predicts every other function from that — ignoring that the same instruction bytes
  implement different contracts on PE vs ELF.
- **CORRECT REASONING**: check `objdump -f` for the format first, then verify three
  signatures: (1) arg registers of the first call, (2) stack allocation before a call
  (32-byte shadow on Windows), (3) which registers the prologue preserves (Windows
  saves `rdi/rsi`, SysV does not).
- **EXAMPLE** (bad):
  ```asm
  # Windows x64 function — but the reader predicts SysV because it sees %rbx saved
  # and misses that args arrive in rcx/rdx and calls reserve 32 bytes.
  ```
- **COUNTEREXAMPLE** (good):
  ```asm
  # Observed MinGW GCC 16.1, Windows x64, non-leaf caller:
  caller:
      pushq %rbp
      .seh_pushreg %rbp
      movq %rsp, %rbp
      .seh_setframe %rbp, 0
      subq $32, %rsp        # 32-byte shadow space -> Windows x64
      .seh_stackalloc 32
      movl $4, %r9d         # arg4 -> r9
      movl $3, %r8d         # arg3 -> r8
      movl $2, %edx         # arg2 -> rdx
      movl $1, %ecx         # arg1 -> rcx
      call callee
      addq $32, %rsp
      popq %rbp
      ret
  ```
- **VERIFICATION**: run the above on the MinGW target and compare with `gcc -S`
  output for the same C source; `objdump -f` reports `pe-x86-64`.
- **SOURCE**: `sysv-amd64-abi` §3.2; Microsoft x64 calling convention;
  `aapcs64` §6–7; `riscv-psabi`.

### R12: Stack-arg offsets in the callee differ

- **RULE**: SysV: stack args begin at `rsp+8` at callee entry (return address at
  `[rsp]`), so with a frame pointer the 7th int arg is at `16(%rbp)` after
  `pushq %rbp; movq %rsp,%rbp`. Windows x64: 5th+ args sit above the shadow space;
  with `pushq %rbp` frame the 5th arg is at `48(%rbp)` and the caller-provided shadow
  space occupies `16..40(%rbp)`.
- **WHY AI GETS IT WRONG**: the agent computes stack-arg offsets with the wrong
  convention's base (e.g. reads the 5th SysV arg at `rsp+8` on Windows, where that is
  the shadow space).
- **CORRECT REASONING**: on Windows, `16(%rbp)`/`24(%rbp)`/`32(%rbp)`/`40(%rbp)`
  after the `push rbp` frame are the callee's spill slots (shadow space); the 5th arg
  is `48(%rbp)`. On SysV the same slots hold the first stack args directly.
- **EXAMPLE** (bad):
  ```asm
  # Windows x64 callee reading its 5th arg from 16(%rbp) — that is the shadow slot
  win_bad_5th:
      pushq %rbp
      movq %rsp, %rbp
      movq 16(%rbp), %rax    # actually rcx's spill, not the 5th argument
      ...
  ```
- **COUNTEREXAMPLE** (good):
  ```asm
  # Windows x64 callee: 5th arg at 48(%rbp) (16=shadow ... 40=shadow end, 48=arg5)
  win_good_5th:
      pushq %rbp
      movq %rsp, %rbp
      movq 48(%rbp), %rax
      ...
  ```
- **VERIFICATION**: verified with MinGW GCC 16.1 — an 8-arg callee reads args 5–8
  from `48/56/64/72(%rbp)` and spills the 4 register args into `16/24/32/40(%rbp)`.
- **SOURCE**: `sysv-amd64-abi` §3.2.3 (stack args at rsp+8); Microsoft x64 calling
  convention (5th arg at rsp+40 at the call site).

## 4. Prologue identification quick table

| Signature in the disassembly | ABI |
|---|---|
| First args `rdi,rsi,rdx,rcx`; no 32-byte shadow; red-zone stores below `rsp` | SysV AMD64 |
| First args `rcx,rdx,r8,r9`; `subq $32,...` before calls; preserves `rdi,rsi` | Windows x64 |
| Args `x0–x7`; `stp x29,x30,[sp,#-16]!`; sret in `x8`; `ret` | AAPCS64 |
| Args `a0–a7`; `jal`/`ret`, `ra` = x1; `sd/ld ra`; s0–s11 preserved | RISC-V psABI |
| Same instructions, `file format pe-x86-64` | Windows x64 |
| Same instructions, `file format elf64-x86-64` | SysV AMD64 |
