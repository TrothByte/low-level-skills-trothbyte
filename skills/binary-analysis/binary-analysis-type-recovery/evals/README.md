# Evaluation — binary-analysis-type-recovery

Toolchain: GCC 16.1 (MSYS2 MinGW x86-64), GNU objdump 2.46, PE/COFF target.
Recorded 2026-08-14. All commands below were executed on this toolchain.

## Verified facts (recorded outputs)

Disassembly of `examples/good/type_recovery.c` at `gcc -O2 -g -c`:

| Source function | Recorded instruction | Recovered type |
|---|---|---|
| `int f_int_from_char(char c)` | `movsbl %cl,%eax; ret` | `char` arg -> `int` result (sign-extend) |
| `unsigned f_uint_from_uchar(unsigned char c)` | `movzbl %cl,%eax; ret` | `unsigned char` arg -> `int` result (zero-extend) |
| `long f_long_from_short(short s)` | `movswl %cx,%eax; ret` | `short` arg -> `int` (LLP64 `long` = 32-bit) |
| `float f_float_add(float,float)` | `addss %xmm1,%xmm0; ret` | `float` args/result |
| `double f_double_mul(double,double)` | `mulsd %xmm1,%xmm0; ret` | `double` args/result |
| `float f_load_float(const float *p)` | `movss (%rcx),%xmm0; ret` | 4-byte FP load = `float` |
| `double f_load_double(const double *p)` | `movsd (%rcx),%xmm0; ret` | 8-byte FP load = `double` |
| `u64 f_load_u64(const u64 *p)` | `mov (%rcx),%rax; ret` | 8-byte integer load = 64-bit unsigned |
| `long f_load_long(const long *p)` | `mov (%rcx),%eax; ret` | 4-byte load; `long` is 32-bit on LLP64 |
| `long long f_big_d(struct Big *p)` | `mov 0x8(%rcx),%rax` | field at offset 8, 8 bytes (`d`) |
| `char f_big_c(struct Big *p)` | `movzbl 0x6(%rcx),%eax` | byte field at offset 6 (`c`) |
| `int f_arr_index(int *p,int n)` | `movslq %edx,%rdx; mov (%rcx,%rdx,4),%eax` | signed int index, 4-byte elements |
| `long f_sum_array(int*,int)` | loop `add (%rcx),%eax; add $0x4,%rcx` | stride 4 = int array |
| `int f_three_args(int,long,char)` | `add %edx,%ecx; movsbl %r8b,%r8d; ...` | args in rcx, rdx, r8 (Windows x64) |
| `int f_call_fp(int(*)(int),int)` | `mov %rcx,%rax; mov %edx,%ecx; jmp *%rax` | fp param + tail call through reg |
| `int f_vtable_call(struct VTab*,int)` | `mov 0x8(%rcx),%rax; mov %edx,%ecx; jmp *%rax` | vtable slot at offset 8 |

DWARF cross-check (`objdump --dwarf=info`): struct `Big` has
`DW_AT_data_member_location` 0, 4, 6, 8 and `DW_AT_byte_size: 16` — matches the
asm offsets 0x8 and 0x6 exactly.

Runtime sign/zero-extension check:

```
gcc -O2 tr_run.c -o tr_run.exe; tr_run.exe
int_from_char(0xFF)=-1      (movsbl: sign-extend, 0xFFFFFFFF)
uint_from_uchar(0xFF)=255   (movzbl: zero-extend, 0xFF)
long_from_short(0xFFFF)=-1  (movswl: sign-extend)
```

## Synthetic eval

1. Read `examples/good/type_recovery.dis` (objdump -d output, no symbols beyond
   function names) and write down the C signature + struct layout for every
   function.
2. Compare with `examples/good/type_recovery.c`. Every type must match:
   movsbl -> char, movzbl -> unsigned char, movswl -> short, addss/movss -> float,
   mulsd/movsd -> double, scale-4 + movslq -> int array with signed int index,
   Big fields at 0(int)/4(short)/6(char)/8(long long), args rcx/rdx/r8, vtable
   slot at 8.

## False-positive eval

The following correct facts must NOT be flagged as errors:

- `mov (%rcx),%eax` for `long` on MinGW — LLP64 makes `long` 32-bit; this is the
  compiler being correct.
- `lea (%rcx,%rcx,1),%eax` as the whole body of `f_smul(long)` — it is integer
  `2*x` arithmetic, not pointer formation.
- `movswl %cx,%eax` returning `long` — the 32-bit write zero-extends to rax;
  result value is correct for the LLP64 type model.
- An 8-byte load that is never dereferenced is a 64-bit integer, not a pointer.
- `jmp *%rax` in a tail position — it IS an indirect call (same semantics as
  `call *%rax; ret`).

## Adversarial eval

The annotations in `examples/bad/type_recovery.c` list misreadings that must be
caught:

- `mov 0x8(%rcx),%rax` interpreted as "second member" — must be corrected via
  DWARF layout (it is the 4th member `d`; offsets 4, 6 hold `b`, `c`).
- `movsbl %cl,%eax` read as "stays char" — must be corrected to `char` -> `int`.
- `mov (%rcx),%eax` read as "64-bit long" — corrected to 32-bit on LLP64.
- `addss` read as "128-bit vector op" — corrected to scalar single-precision.

## How to run

```
gcc -O2 -g -c examples/good/type_recovery.c -o t.o
objdump -d t.o > examples/good/type_recovery.dis
objdump --dwarf=info t.o | grep -E "data_member_location|byte_size"
gcc -O2 tr_run.c -o tr_run.exe && ./tr_run.exe
```

Expected: recovered types match the C source for every function; DWARF member
offsets (0,4,6,8) and byte_size (16) match asm; runtime prints
`-1`, `255`, `-1`.
