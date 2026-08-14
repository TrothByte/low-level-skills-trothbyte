// Windows x64 caller: 32-byte shadow space, args in rcx/rdx/r8/r9.
// This is the exact prologue GCC 16.1 (MinGW) generated for:
//   long long callee(long long a, long long b, long long c, long long d);
//   long long caller(void) { return callee(1, 2, 3, 4); }
    .globl  caller
    .def    caller; .scl 2; .type 32; .endef
    .seh_proc caller
caller:
    pushq   %rbp
    .seh_pushreg %rbp
    movq    %rsp, %rbp
    .seh_setframe %rbp, 0
    subq    $32, %rsp
    .seh_stackalloc 32
    .seh_endprologue
    movl    $4, %r9d
    movl    $3, %r8d
    movl    $2, %edx
    movl    $1, %ecx
    call    callee
    addq    $32, %rsp
    popq    %rbp
    ret
    .seh_endproc
