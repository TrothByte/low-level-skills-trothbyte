# DOCUMENTED-AS-TARGET (SysV AMD64, ELF/Linux): no cross-compiler in this repo.
# Generate with `gcc -S` on a Linux host or Godbolt to confirm.
# SysV: int args rdi, rsi, rdx, rcx, r8, r9; rsp % 16 == 0 at call; red zone
# below rsp; callee-saved rbx, rbp, r12..r15.
    .globl  sysv_add
sysv_add:
    leaq    (%rdi,%rsi), %rax
    ret
