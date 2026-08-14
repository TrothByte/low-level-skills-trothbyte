// BAD: x86-64 caller that reaches the call site with rsp % 16 == 8 (subq $40).
// The callee's movaps on its frame slot then faults.
// VERIFIED (MinGW GCC 16.1): SIGSEGV / access-violation 0xC0000005; the aligned
// counterpart (subq $32) runs clean.
    .globl  caller_misaligned
    .def    caller_misaligned; .scl 2; .type 32; .endef
    .seh_proc caller_misaligned
caller_misaligned:
    pushq   %rbp
    .seh_pushreg %rbp
    movq    %rsp, %rbp
    .seh_setframe %rbp, 0
    subq    $40, %rsp
    .seh_stackalloc 40
    .seh_endprologue
    pxor    %xmm0, %xmm0
    call    callee_movaps
    addq    $40, %rsp
    popq    %rbp
    ret
    .seh_endproc

// Callee whose frame slot alignment depends on the caller keeping rsp % 16 == 0.
    .globl  callee_movaps
    .def    callee_movaps; .scl 2; .type 32; .endef
    .seh_proc callee_movaps
callee_movaps:
    pushq   %rbp
    .seh_pushreg %rbp
    movq    %rsp, %rbp
    .seh_setframe %rbp, 0
    subq    $16, %rsp
    .seh_stackalloc 16
    .seh_endprologue
    movaps  %xmm0, -16(%rbp)
    movaps  -16(%rbp), %xmm0
    addq    $16, %rsp
    popq    %rbp
    ret
    .seh_endproc
