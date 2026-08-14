// BAD example — misread type-recovery patterns that this skill teaches you to avoid.
//
// Same toolchain: MinGW GCC 16.1, PE x86-64, Windows x64 calling convention
// (first integer arg in RCX; on SysV/Linux it would be RDI).
//
// Misreadings documented for each function:
//
//  f_int_from_char    movsbl %cl,%eax
//    WRONG: "movsbl copies a byte, so c must be char and the result stays char"
//    RIGHT: movsbl SIGN-extends the low byte into a 32-bit int result; the
//           source is a signed char promoted to int.
//
//  f_uint_from_uchar  movzbl %cl,%eax
//    WRONG: "movzbl zeroes bits, so the value is unsigned — must be unsigned int"
//    RIGHT: the operand is unsigned char; the destination is a 32-bit int.
//           The unsigned-ness is about the extension, not the destination type.
//
//  f_load_long        mov (%rcx),%eax
//    WRONG: "64-bit register load means long/64-bit type" (if agent misreads
//           AT&T: `mov` with %eax means 32-bit; on this LLP64 target long=32)
//    WRONG (LP64): "long is 64 bits everywhere" — on Windows/MinGW long is
//           32-bit; on Linux/SysV long is 64-bit (`mov (%rcx),%rax`).
//
//  f_big_d            mov 0x8(%rcx),%rax
//    WRONG: "struct field at offset 8 must be the second member"
//    RIGHT: DWARF shows members at 0(int),4(short),6(char),8(long long);
//           offset 8 is the 4th member (padding from 7 to 8). Layout is
//           alignment-driven, NOT declaration order 1:1.
//
//  f_arr_index        movslq %edx,%rdx ; mov (%rcx,%rdx,4),%eax
//    WRONG: "scale 4 means the index is multiplied, so the element type is
//           unknown/pointer" 
//    RIGHT: scale 4 = element size 4 bytes -> int (or float, but integer
//           arithmetic on the result disambiguates). movslq = SIGNED index.
//
//  f_call_fp / f_vtable_call  mov %rcx,%rax ; ... ; jmp *%rax
//    WRONG: "jmp means no function call / no function pointer"
//    RIGHT: jmp *%rax is an indirect (tail) call through a register that was
//           loaded from the vtable slot; a vtable call at offset 8.
//
//  f_float_add        addss %xmm1,%xmm0
//    WRONG: "xmm registers are always 128-bit SSE, so float ops use the full
//           register" — addss is SCALAR single-precision; the 32-bit lane is
//           used, upper lanes untouched. movss/movsd suffixes encode width.
//
// The real asm is in type_recovery_good.dis (same functions). Compare your
// reading against the annotated walkthrough in the GOOD file.

typedef unsigned long long u64;

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
