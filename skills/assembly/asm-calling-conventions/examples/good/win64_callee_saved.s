// Windows x64: a callee that uses rbx and r12..r15 must save them; GCC 16.1
// (MinGW) generated exactly these push sequences (reverse-order pops).
    .globl  callee_saved2
    .def    callee_saved2; .scl 2; .type 32; .endef
    .seh_proc callee_saved2
callee_saved2:
    pushq   %r15
    .seh_pushreg %r15
    pushq   %r14
    .seh_pushreg %r14
    pushq   %r13
    .seh_pushreg %r13
    pushq   %r12
    .seh_pushreg %r12
    pushq   %rbx
    .seh_pushreg %rbx
    .seh_endprologue
    movl    $0, %ebx
    movl    $0, %r12d
    movl    $0, %r13d
    movl    $0, %r14d
    movl    $0, %r15d
    testl   %edx, %edx
    jle     .L2
    movq    %rcx, %rax
    movslq  %edx, %rdx
    leaq    (%rcx,%rdx,8), %rdx
.L3:
    addq    (%rax), %rbx
    addq    $1, %r12
    addq    $2, %r13
    addq    $3, %r14
    addq    $4, %r15
    addq    $8, %rax
    cmpq    %rdx, %rax
    jne     .L3
.L2:
    addq    %r12, %rbx
    addq    %r13, %rbx
    addq    %r14, %rbx
    leaq    (%rbx,%r15), %rax
    popq    %rbx
    popq    %r12
    popq    %r13
    popq    %r14
    popq    %r15
    ret
    .seh_endproc
