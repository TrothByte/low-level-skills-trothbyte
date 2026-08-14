// GOOD walkthrough — recover types from disassembly and cross-check with DWARF.
//
// Toolchain for the recorded disassembly: MinGW GCC 16.1, PE x86-64, Windows x64
// calling convention (first integer arg in RCX; on SysV/Linux it would be RDI).
// Compile with:  gcc -O2 -g -c type_recovery.c -o type_recovery.o
// Disassemble:  objdump -d type_recovery.o
// DWARF check:  objdump --dwarf=info type_recovery.o
//
// Recover the types from the instruction patterns, THEN compare with the source:
//
//  f_int_from_char    movsbl %cl,%eax   -> signed char (sign-extended) -> int
//  f_uint_from_uchar  movzbl %cl,%eax   -> unsigned char (zero-ext)    -> int
//  f_long_from_short  movswl %cx,%eax   -> short, sign-extended.
//                     On this LLP64 target `long` is 32-bit, so `movswl`
//                     (not `movswq`) and eax, not rax.
//  f_float_add        addss %xmm1,%xmm0 -> float args in xmm regs
//  f_double_mul       mulsd %xmm1,%xmm0 -> double args in xmm regs
//  f_load_float       movss (%rcx),%xmm0-> 4-byte FP load -> float
//  f_load_double      movsd (%rcx),%xmm0-> 8-byte FP load -> double
//  f_load_u64         mov (%rcx),%rax   -> 8-byte integer load -> u64
//  f_load_long        mov (%rcx),%eax   -> 4-byte load: `long` is 32-bit
//                     here (LLP64). On LP64/Linux it would be %rax.
//  f_big_d            mov 0x8(%rcx),%rax-> struct field at offset 8, 8 bytes
//  f_big_c            movzbl 0x6(%rcx),%eax -> byte field at offset 6
//  f_arr_index        movslq %edx,%rdx; mov (%rcx,%rdx,4),%eax
//                     -> int index sign-extended; scale 4 => element int(4)
//  f_sum_array        loop: add (%rcx),%eax; add $0x4,%rcx
//                     -> stride 4 -> int array
//  f_three_args       args rcx, rdx, r8 -> (int, long, char): char is
//                     passed in r8b, sign-extended to r8d by movsbl
//  f_call_fp / f_vtable_call  mov %rcx,%rax ... jmp *%rax -> indirect call
//                     through a register; vtable slot at offset 8
//
// DWARF cross-check for struct Big: a@0 (int), b@4 (short), c@6 (char),
// d@8 (long long), byte_size 16 — matches the asm offsets 0x8 and 0x6.

typedef unsigned long long u64;
typedef long long i64;

char f_char_ret(short x) { return (char)(x + 1); }

int f_int_from_char(char c) { return c; }

unsigned int f_uint_from_uchar(unsigned char c) { return c; }

long f_long_from_short(short s) { return s; }

float f_float_add(float a, float b) { return a + b; }

double f_double_mul(double a, double b) { return a * b; }

float f_load_float(const float *p) { return *p; }

double f_load_double(const double *p) { return *p; }

u64 f_load_u64(const u64 *p) { return *p; }

long f_load_long(const long *p) { return *p; }

struct Big { int a; short b; char c; long long d; };
long long f_big_d(struct Big *p) { return p->d; }

char f_big_c(struct Big *p) { return p->c; }

int f_arr_index(int *p, int n) { return p[n]; }

long f_sum_array(int *arr, int n) {
  long s = 0;
  for (int i = 0; i < n; i++) s += arr[i];
  return s;
}

int f_three_args(int a, long b, char c) { return a + (int)b + c; }

int f_call_fp(int (*fp)(int), int x) { return fp(x); }

struct VTab { int (*f0)(int); int (*f1)(int); };
int f_vtable_call(struct VTab *vt, int x) { return vt->f1(x); }
