# DOCUMENTED-AS-TARGET (AAPCS64, AArch64): no cross-compiler in this repo.
# Generate with `aarch64-linux-gnu-gcc -S` or Godbolt to confirm.
# AAPCS64: args x0..x7 (int) and v0..v7 (FP); x8 = sret; x19..x28 + x29 callee-
# saved; x30 = LR, saved with `stp x29, x30, [sp, #-16]!` in non-leaf functions;
# sp 16-byte aligned.
    .globl  aapcs_add
aapcs_add:
    add     x0, x0, x1
    ret
