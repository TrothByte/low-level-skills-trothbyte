// Windows x64 leaf function returning a+b. Args arrive in rcx, rdx; result in rax.
// VERIFIED: assembled and run with MinGW GCC 16.1, win_add(40,2) == 42.
    .globl  win_add
    .def    win_add; .scl 2; .type 32; .endef
    .seh_proc win_add
win_add:
    .seh_endprologue
    leaq    (%rcx,%rdx), %rax
    ret
    .seh_endproc
