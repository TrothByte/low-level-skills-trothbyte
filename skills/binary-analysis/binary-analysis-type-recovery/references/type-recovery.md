# Type Recovery from x86-64 Assembly — Reference

Format per rule: RULE / WHY AI GETS IT WRONG / CORRECT REASONING / EXAMPLE (bad) /
COUNTEREXAMPLE (good) / VERIFICATION / SOURCE.

Empirical basis: GCC 16.1 (MinGW x86-64, PE/COFF), `gcc -O2 -g -c` + `objdump -d`,
recorded 2026-08-14 from `examples/good/type_recovery.c`. Calling convention in the
recordings is the Windows x64 ABI (first integer arg in RCX); SysV AMD64 uses RDI.
Always check which ABI applies before naming argument registers.

## 1. `movzx`/`movsx` — narrow integer types (char/short)

- **RULE**: a load/argument of a narrow integer that is extended to 32 or 64 bits
  appears as `movzbl/movzwl/movsbw/movswl/movslq` (or `movzx`/`movsx` in Intel syntax).
  `z` = zero-extension (unsigned source), `s` = sign-extension (signed source).
  The extension suffix tells the SOURCE width and signedness, not the destination
  "kind": `movsbl %cl,%eax` reads a signed byte and produces an int.
- **WHY AI GETS IT WRONG**: "movsbl copies a byte, so the value must be char and
  stays char" (the result is a 32-bit int). Or "movzbl zeroes bits, so the type
  must be unsigned int" (unsigned-ness describes the source; the promoted value
  is a plain int). Or "movsx is never used because char is signed anyway" —
  a signed char IS extended with movsx, an unsigned char with movzx.
- **CORRECT REASONING**: the extension instruction is the compiler implementing
  integer promotion. `signed char c; int x = c;` -> `movsbl`; `unsigned char c;
  int x = c;` -> `movzbl`. For `short` the same rule with `w` (16-bit) suffixes.
- **EXAMPLE** (bad): `int f(char c) { return c; }` disassembles to
  `movsbl %cl,%eax; ret`. Agent reports "function takes a byte and returns a byte,
  probably `char f(char)`".
- **COUNTEREXAMPLE** (good): `movsbl %cl,%eax` = `char` argument, `int` result.
  `movzbl %cl,%eax` = `unsigned char` argument, `int` result. Difference proven by
  feeding 0xFF: the `s` version returns 0xFFFFFFFF (-1), the `z` version 0xFF (255).
- **VERIFICATION**: `gcc -O2 -c -x c -o /dev/null - <<< 'int f(char c){return c;}'`
  then `objdump -d`; 0xFF input check via a tiny test main, run both builds.
- **SOURCE**: intel-sdm (MOVSX/MOVZX, Vol.2); sysv-amd64-abi (4.1 integer
  argument passing); verified GCC 16.1: `movsbl %cl,%eax` (f_int_from_char),
  `movzbl %cl,%eax` (f_uint_from_uchar), `movswl %cx,%eax` (f_long_from_short).

## 2. `movss`/`movsd` — float vs double

- **RULE**: scalar FP loads/arguments use the `ss` (32-bit, single) and `sd`
  (64-bit, double) forms of SSE/SSE2 instructions: `movss (%rcx),%xmm0` loads a
  float, `movsd (%rcx),%xmm0` loads a double; `addss`/`addsd`, `mulsd`, etc. follow
  the same suffix rule. FP arguments travel in `%xmm0-%xmm7`.
- **WHY AI GETS IT WRONG**: "xmm registers are 128-bit SSE, so all xmm ops are
  full-width vector ops" (they are SCALAR single/double operations; the suffix is
  the width). Or "movsd moves a string/pointer" (confusion with the historical
  string MOVSD; the `sd` scalar-double interpretation applies in FP context).
- **CORRECT REASONING**: identify the register class (`%xmmN`) then the suffix:
  `ss`/`sd`/`ps`/`pd`. Scalar `ss`/`sd` = one float/double. Vector `ps`/`pd` =
  packed lanes (not a single C scalar). `cvtss2sd` (float->double), `cvttsd2si`
  (double->int) confirm the FP->integer boundary.
- **EXAMPLE** (bad): `float f(float a, float b) { return a+b; }` compiles to
  `addss %xmm1,%xmm0`. Agent reports "uses 128-bit vector addition, so the type
  is __m128 or double".
- **COUNTEREXAMPLE** (good): `addss` = single-precision scalar -> `float`;
  `mulsd`/`movsd` = double-precision scalar -> `double`. Cross-check with
  `cvtss2sd` when both appear.
- **VERIFICATION**: `gcc -O2` of `float g(float a,float b){return a+b;}` and
  `double h(double a,double b){return a*b;}` -> `addss %xmm1,%xmm0` and
  `mulsd %xmm1,%xmm0` (verified).
- **SOURCE**: intel-sdm (SSE instructions Vol.2); sysv-amd64-abi (3.2.3 FP
  argument passing); verified GCC 16.1: `movss (%rcx),%xmm0`, `movsd (%rcx),%xmm0`.

## 3. `movl` vs `movq` — 32- vs 64-bit integer width

- **RULE**: `mov (%reg),%eax` is a 32-bit load; `mov (%reg),%rax` is 64-bit.
  AT&T `l` suffix = 32-bit, `q` = 64-bit (NOT "long=64" by name: on Windows/LLP64
  `long` is 32-bit, on SysV/LP64 it is 64-bit). Suffix always matches register
  width; register width is the authoritative signal.
- **WHY AI GETS IT WRONG**: "movl loads a 'long'" (l-long confusion), or "the
  target is Windows, so long is 64 bits" (reversed: Windows LLP64 long = 32),
  or "all pointers are 64-bit, so an 8-byte load is a pointer" (an 8-byte load is
  a u64/long long, not necessarily a pointer; use usage, not width alone).
- **CORRECT REASONING**: read the register pair: `%eax`/`%r8d` = 32-bit,
  `%rax`/`%r8` = 64-bit. A 32-bit write to a 64-bit register zero-extends
  (intel-sdm Vol.1 §3.4). For `long` semantics, know the platform model:
  LP64 (Linux/SysV, `long`=8) vs LLP64 (Windows, `long`=4, `long long`=8).
- **EXAMPLE** (bad): `long f_load_long(const long *p){return *p;}` on MinGW
  (LLP64) compiles to `mov (%rcx),%eax`. Agent claims "that must be an int, the
  source is wrong" — the compiler is right, `long` is 32-bit here.
- **COUNTEREXAMPLE** (good): same function on LP64/Linux emits
  `mov (%rdi),%rax`. The 64-bit variant (`u64`, `long long`) on MinGW emits
  `mov (%rcx),%rax` (verified f_load_u64). Width signal is target-specific.
- **VERIFICATION**: compile `long l(const long*p){return *p;}` and
  `unsigned long long u(const unsigned long long*p){return *p;}`; objdump the
  `.text`; note `%eax` vs `%rax` (verified on GCC 16.1/MinGW).
- **SOURCE**: intel-sdm (MOV, Vol.2; zero-extension rule Vol.1 §3.4);
  sysv-amd64-abi (4.1 data sizes, LP64). Windows LLP64: Microsoft x64 convention
  (outside the registry; marked INFERRED from toolchain behavior).

## 4. Struct access via offsets — `[rbx+8]`, `disp(%reg)`

- **RULE**: `mov 0x8(%rcx),%rax` reads the 8-byte field at struct offset 8 of the
  object pointed by `%rcx`. Recovered struct layout = map of offsets to field
  types. Offsets are alignment-driven, NOT declaration order 1:1; padding is
  expected. Recover the layout, not just "a member".
- **WHY AI GETS IT WRONG**: "offset 8 is the second member" (may be 4th after
  padding); "a load at offset 0 is the first member, period" (sub-structs and
  unions change this); "the struct is a flat array because several offsets are
  accessed" (a struct has heterogeneous field types — distinguish by the width
  and usage of each access).
- **CORRECT REASONING**: collect every `disp(%reg)` access with its width; assign
  types (4-byte load -> int/float, 8-byte -> long long/double/pointer, byte ->
  char). `mov 0x4(%rcx),%eax` + `movsd 0x8(%rcx),%xmm0` + `movzbl 0x10(%rcx)` on
  the same base => {int@0; double@8; char@16} with padding between. For the
  layout between sampled offsets, infer from alignment rules (double aligns to 8).
- **EXAMPLE** (bad): `struct Big { int a; short b; char c; long long d; }`; the
  code reads `p->d`. Asm: `mov 0x8(%rcx),%rax`. Agent writes "the second field is
  8 bytes — it must be `void*`".
- **COUNTEREXAMPLE** (good): DWARF for `Big` shows a@0 (int), b@4 (short), c@6
  (char), d@8 (long long), byte_size 16; the `mov 0x8(%rcx),%rax` IS `d`. Offsets
  4 and 6 are occupied by b and c; 7 is padding. `movzbl 0x6(%rcx),%eax` is `c`.
- **VERIFICATION**: `gcc -O2 -g`, then `objdump --dwarf=info` and grep
  `DW_AT_data_member_location`; compare with `objdump -d` offsets (verified for
  `Big`: 0,4,6,8 and byte_size 16).
- **SOURCE**: sysv-amd64-abi (4.1 data sizes/alignment); dwarf-v5
  (DW_AT_data_member_location); verified GCC 16.1 (f_big_d `mov 0x8(%rcx),%rax`,
  f_big_c `movzbl 0x6(%rcx),%eax`).

## 5. Pointer vs integer — register width plus usage

- **RULE**: an object is a pointer only if its bits are USED as an address
  (dereferenced by a memory operand, compared with another pointer, passed to a
  function that dereferences). A 64-bit integer that is never dereferenced is a
  u64/long long, not a pointer. Register width alone is necessary but not
  sufficient; usage decides.
- **WHY AI GETS IT WRONG**: "rax is 64-bit and the value came from a load, so
  it's a pointer"; "lea computes an address, so the value is a pointer" (`lea`
  is plain arithmetic, see asm-optimizer-artifacts §5); "comparing a value to 0
  makes it a pointer" (integers are compared too).
- **CORRECT REASONING**: trace the value: if it ever becomes the base of a
  memory operand `(reg, ...)` or the target of `call *%reg`/`jmp *%reg`, it is a
  pointer. If it is only shifted/added/masked/compared and then returned or stored
  as 8 bytes, it is an integer. `lea (%rcx,%rcx,1),%rax` for `x*2` is integer
  arithmetic (returns `2*x`), NOT pointer formation.
- **EXAMPLE** (bad): `long f_smul(long x){return x*2;}` at `-O2` ->
  `lea (%rcx,%rcx,1),%eax`. Agent reports "takes a pointer, doubles the address".
- **COUNTEREXAMPLE** (good): result is `%eax` (32-bit), never dereferenced —
  it is `2*x`. The same shape with a dereference `mov (%rax),%eax` right after
  would be pointer arithmetic.
- **VERIFICATION**: `gcc -O2 -S` for `long f(long x){return x*2;}` and for
  `int f(int*p){return *p;}`; compare: first has no memory operand from the lea
  result, second dereferences `%rcx` directly (verified).
- **SOURCE**: intel-sdm (LEA Vol.2 — address computation arithmetic);
  sysv-amd64-abi (4.1 pointer size 8); verified GCC 16.1 (f_smul/f_imul/f_llmul).

## 6. Arrays via indexed addressing — `[rax+rcx*4]`

- **RULE**: `mov (%rcx,%rdx,4),%eax` = load at `base + index*4`; the scale (1/2/4/8)
  is the element size. Combined with the load width it pins the element type:
  scale 4 + 32-bit load => `int`/`float`; scale 8 + 64-bit => `long long`/`double`
  /pointer; scale 1 + byte => `char`. The index register's extension (movslq vs
  plain mov) shows whether the index is signed (`int`) or unsigned.
- **WHY AI GETS IT WRONG**: "scale 4 means a pointer is multiplied, so this is
  pointer arithmetic, not an array"; "an indexed access proves the type is an
  array, but I can't tell the element type" (scale+load width give it exactly);
  "unsigned index uses movslq too" (it does not — `movslq` = sign extend).
- **CORRECT REASONING**: for a loop over an array, the stride between successive
  loads is the element size: `add $0x4,%rcx` per iteration => element size 4.
  Indexed access `(base,index,4)` with `movslq %edx,%rdx` on the index = signed
  int index. `mov %edx,%edx` (plain, no sign extension) = unsigned index.
- **EXAMPLE** (bad): `int f(int*p,int n){return p[n];}` -> `movslq %edx,%rdx;
  mov (%rcx,%rdx,4),%eax`. Agent: "scale 4 means an array of 4-byte pointers".
- **COUNTEREXAMPLE** (good): 4-byte elements, 32-bit result, signed index —
  `int[]` indexed by `int`. An 8-byte result would mean `long long`/pointer
  elements. Verify element type with the stride in the loop version (verified:
  f_sum_array increments `%rcx` by 4).
- **VERIFICATION**: `gcc -O2 -c` of `int f(int*p,int n){return p[n];}` ->
  `movslq %edx,%rdx; mov (%rcx,%rdx,4),%eax` (verified); unsigned-index variant
  `unsigned int f(unsigned*p,unsigned i){return p[i];}` -> `mov %edx,%edx`
  without movslq (verified).
- **SOURCE**: intel-sdm (SIB addressing Vol.1 §3.7.5, Vol.2 MOV);
  sysv-amd64-abi (4.1); verified GCC 16.1.

## 7. Function signatures from calling convention

- **RULE**: integer/pointer args arrive in order in the integer registers
  (SysV: `%rdi,%rsi,%rdx,%rcx,%r8,%r9`; Windows x64: `%rcx,%rdx,%r8,%r9`), FP args
  in `%xmm0-%xmm7`. The register(s) a function reads without setting are its
  parameters; the register(s) it writes before `ret` are the return value
  (`%eax`/`%rax`/`%xmm0`). The ABI in force determines which register is arg 1.
- **WHY AI GETS IT WRONG**: "first arg is always in rdi" (that is SysV; Windows
  x64 uses rcx); "char args occupy a full 64-bit register" (they occupy the low
  8 bits, e.g. `%cl`/`%r8b`, extended on use); "the function only reads rcx, so
  it has one argument" (it has AT LEAST one; more args may be read later or the
  tail call may shuffle registers — `mov %rcx,%rax` then `jmp *%rax`).
- **CORRECT REASONING**: identify the ABI (ELF => SysV, PE/COFF => Windows x64),
  list the registers read before any write, and map them to parameter slots.
  A parameter is read at its ABI register; a `mov` FROM that register to another
  is register renaming for a tail call, not a new parameter.
- **EXAMPLE** (bad): `int f_call_fp(int(*fp)(int),int x){return fp(x);}` ->
  `mov %rcx,%rax; mov %edx,%ecx; jmp *%rax`. Agent: "reads rcx and rdx — but also
  writes rcx, so the signature is unclear / returns void".
- **COUNTEREXAMPLE** (good): `%rcx` (fp, a pointer) is moved to `%rax` for the
  indirect tail call; `%edx` (x) becomes the argument in `%ecx`. Signature:
  `int f(int(*fp)(int), int)`, an indirect call, `%eax` result.
- **VERIFICATION**: compile with `-O2 -g`; `objdump --dwarf=info` shows
  `DW_TAG_subprogram` parameter DIEs with types; match registers to ABI table
  (verified f_three_args: int in ecx, long in edx, char in r8b — Windows x64).
- **SOURCE**: sysv-amd64-abi (3.2 function calling sequence, 3.2.3 FP);
  Windows x64 calling convention (INFERRED from toolchain; PE target);
  dwarf-v5 (subprogram DIEs); verified GCC 16.1.

## 8. Function pointers / vtable calls

- **RULE**: `call *%reg` or `jmp *%reg` with `%reg` loaded just before is an
  indirect call through a function pointer. A vtable call loads the target from a
  struct/object field first: `mov 0x8(%rcx),%rax; ...; jmp *%rax` = load slot 8 of
  the object in `%rcx`, then call it. The offset identifies the virtual slot.
- **WHY AI GETS IT WRONG**: "jmp is never a call" (it is a tail call here, see
  asm-optimizer-artifacts §2); "a register loaded from memory then jumped through
  is a computed goto, not a vtable"; "the object pointer is the first vtable slot"
  (the vtable pointer lives at offset 0 typically and is dereferenced to reach
  the method — here the slot IS the method pointer in a manual vtable struct).
- **CORRECT REASONING**: an indirect `call`/`jmp *reg` whose target is loaded
  from `disp(%base)` is a function-pointer or virtual call. If the base holds the
  vtable pointer (first member) and the target comes through
  `mov (reg),reg; mov disp(reg),reg`, that is C++-style virtual dispatch. Manual
  vtables (struct of function pointers) dispatch directly from `disp(%base)`.
- **EXAMPLE** (bad): `struct VTab{int(*f0)(int);int(*f1)(int);};` +
  `f_vtable_call(vt,x) { return vt->f1(x); }` -> `mov 0x8(%rcx),%rax; mov %edx,
  %ecx; jmp *%rax`. Agent: "reads memory at offset 8 and jumps there — that is a
  switch jump table".
- **COUNTEREXAMPLE** (good): the object is `%rcx`; slot 8 is `f1`; `%edx` (x)
  is the argument; `jmp *%rax` is the virtual/indirect call. The same pattern with
  two loads (vtable ptr then slot) is C++ virtual dispatch.
- **VERIFICATION**: `gcc -O2 -c` of the VTab example -> `mov 0x8(%rcx),%rax;
  mov %edx,%ecx; rex.W jmp *%rax` (verified). Compare with a plain `int
  f(int(*fp)(int),int x){return fp(x);}`: `mov %rcx,%rax; mov %edx,%ecx; jmp *%rax`
  — no memory load, so a direct function-pointer parameter.
- **SOURCE**: intel-sdm (CALL/JMP indirect Vol.2); sysv-amd64-abi (3.2);
  itanium-cxx-abi (vtable layout); verified GCC 16.1.

## 9. How to verify with DWARF when present

- **RULE**: when debug info exists (`-g`), DWARF is ground truth for types:
  `objdump --dwarf=info` (works on PE and ELF) shows DIEs for functions
  (`DW_TAG_subprogram`), parameters, variables, structs (`DW_TAG_structure_type`
  with `DW_AT_data_member_location` and `DW_AT_byte_size`), and types.
- **WHY AI GETS IT WRONG**: "DWARF is only for line numbers" (DIEs carry full
  types); "readelf --debug-dump is the only tool" (on PE/MinGW it prints
  nothing — use `objdump --dwarf=info`); "stripped binary = no type info"
  (then fall back to the pattern rules above).
- **CORRECT REASONING**: for each recovered type claim, find the DIE:
  function -> `DW_TAG_subprogram` `DW_AT_name`; parameter -> `DW_TAG_formal_parameter`
  `DW_AT_type`; struct member -> `DW_TAG_member` `DW_AT_data_member_location`;
  struct size -> `DW_AT_byte_size`. Match member offsets to asm `disp(%reg)`.
- **EXAMPLE** (bad): agent "verifies" a struct by guessing; DWARF shows
  `Big` a@0, b@4, c@6, d@8, byte_size 16 — the guessed layout omits padding at 7.
- **COUNTEREXAMPLE** (good): the DWARF member list is the layout; asm offsets
  0x8 and 0x6 line up exactly with `d` and `c` (verified for `Big`).
- **VERIFICATION**: `gcc -O2 -g -c t.c; objdump --dwarf=info t.o | grep
  DW_AT_data_member_location` and compare with `objdump -d` (verified).
- **SOURCE**: dwarf-v5 (debug_info, DW_AT_data_member_location, byte_size);
  binutils-docs (objdump --dwarf); gcc-manual (-g); verified GCC 16.1.

## Calibration

- Width suffix (l/q, ss/sd) plus usage (dereference/call vs arithmetic) beats
  guessing from one instruction; always collect all accesses to a register.
- Register-to-parameter mapping depends on the ABI (SysV rdi/rsi/... vs
  Windows rcx/rdx/r8/r9); misidentifying the ABI misidentifies every signature.
- If DWARF exists, type recovery is verification, not inference.
- `lea` is arithmetic; `jmp *reg` after a load is an indirect/virtual call.
