// Windows x64 caller that keeps rsp % 16 == 0 at the call site (subq $32 is a
// multiple of 16). VERIFIED: callee_movaps runs clean when called from here.
    .globl  caller_aligned
    .def    caller_aligned; .scl 2; .type 32; .endef
    .seh_proc caller_aligned
caller_aligned:
    pushq   %rbp
    .seh_pushreg %rbp
    movq    %rsp, %rbp
    .seh_setframe %rbp, 0
    subq    $32, %rsp
    .seh_stackalloc 32
    .seh_endprologue
    pxor    %xmm0, %xmm0
    call    callee_movaps
    addq    $32, %rsp
    popq    %rbp
    ret
    .seh_endproc
