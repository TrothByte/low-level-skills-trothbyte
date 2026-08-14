# DOCUMENTED-AS-TARGET (RISC-V psABI, RV64 lp64): no cross-compiler in this repo.
# Generate with `riscv64-linux-gnu-gcc -S` or Godbolt to confirm.
# RISC-V: args a0..a7 (x10..x17), FP fa0..fa7 (f10..f17); ra = x1 set by jal;
# callee-saved s0..s11 (x8..x9, x18..x27); sp 16-byte aligned; result in a0.
    .globl  riscv_add
riscv_add:
    add     a0, a0, a1
    ret
