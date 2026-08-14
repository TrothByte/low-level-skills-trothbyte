// Windows x64 callee receiving 8 integer args: 4 in registers, 4 on the stack.
// This is the exact prologue GCC 16.1 (MinGW) generated for:
//   long long f64(long long a, long long b, long long c, long long d,
//                 long long e, long long f6, long long g, long long h);
// Observed facts: register args spill into the caller's shadow space at
// 16..40(%rbp); stack args 5..8 are read from 48/56/64/72(%rbp).
    .globl  f64
    .def    f64; .scl 2; .type 32; .endef
    .seh_proc f64
f64:
    pushq   %rbp
    .seh_pushreg %rbp
    movq    %rsp, %rbp
    .seh_setframe %rbp, 0
    .seh_endprologue
    movq    %rcx, 16(%rbp)
    movq    %rdx, 24(%rbp)
    movq    %r8, 32(%rbp)
    movq    %r9, 40(%rbp)
    movq    16(%rbp), %rdx
    movq    24(%rbp), %rax
    addq    %rax, %rdx
    movq    32(%rbp), %rax
    addq    %rax, %rdx
    movq    40(%rbp), %rax
    addq    %rax, %rdx
    movq    48(%rbp), %rax
    addq    %rax, %rdx
    movq    56(%rbp), %rax
    addq    %rax, %rdx
    movq    64(%rbp), %rax
    addq    %rax, %rdx
    movq    72(%rbp), %rax
    addq    %rdx, %rax
    popq    %rbp
    ret
    .seh_endproc
