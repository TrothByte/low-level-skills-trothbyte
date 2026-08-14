// BAD: SysV AMD64 argument registers used in a function that runs under Windows
// x64, where the caller places args in rcx, rdx — not rdi, rsi.
// VERIFIED (MinGW GCC 16.1): bad_sysv_add(40,2) returned garbage (88/96 across
// builds) because rdi/rsi hold unrelated caller values; win_add(40,2)==42.
    .globl  bad_sysv_add
    .def    bad_sysv_add; .scl 2; .type 32; .endef
    .seh_proc bad_sysv_add
bad_sysv_add:
    .seh_endprologue
    leaq    (%rdi,%rsi), %rax
    ret
    .seh_endproc
