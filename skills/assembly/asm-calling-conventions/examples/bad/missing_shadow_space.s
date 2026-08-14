// BAD: Windows x64 caller that does NOT reserve the 32-byte shadow space before a
// call. The callee spills its register args at pre-call rsp+0..31, which lands on
// the caller's own local at rbp-16.
// VERIFIED (MinGW GCC 16.1): the sentinel 0x1122334455667788 was clobbered to 0x1.
    .globl  caller_noshadow
    .def    caller_noshadow; .scl 2; .type 32; .endef
    .seh_proc caller_noshadow
caller_noshadow:
    pushq   %rbp
    .seh_pushreg %rbp
    movq    %rsp, %rbp
    .seh_setframe %rbp, 0
    subq    $16, %rsp
    .seh_stackalloc 16
    .seh_endprologue
    movabsq $0x1122334455667788, %rax
    movq    %rax, -16(%rbp)
    movl    $1, %ecx
    movl    $2, %edx
    call    callee_writes_shadow
    movq    -16(%rbp), %rax
    addq    $16, %rsp
    popq    %rbp
    ret
    .seh_endproc

// Callee that spills its register args into the caller-provided shadow space.
    .globl  callee_writes_shadow
    .def    callee_writes_shadow; .scl 2; .type 32; .endef
    .seh_proc callee_writes_shadow
callee_writes_shadow:
    pushq   %rbp
    .seh_pushreg %rbp
    movq    %rsp, %rbp
    .seh_setframe %rbp, 0
    .seh_endprologue
    movq    %rcx, 16(%rbp)
    movq    %rdx, 24(%rbp)
    leaq    (%rcx,%rdx), %rax
    popq    %rbp
    ret
    .seh_endproc
