; intentionally incorrect
; BAD: recursion with s0 uninitialized. The function saves s0 (callee-saved)
; at 0(sp) but never establishes its own frame value before recursing; the
; recursive call clobbers s0, so the caller's s0 is garbage on return. The
; sum ends up corrupt. (Recorded RISC-V failure: s0 uninitialized, frame of
; 4 bytes instead of 8.)
        .text
        .globl  sum
sum:
        beqz    a0, .base
        addi    sp, sp, -8
        sw      s0, 0(sp)       ; save s0 but never set it
        addi    a0, a0, -1
        call    sum             ; BAD: s0 not preserved across this call
        lw      s0, 0(sp)
        addi    sp, sp, 8
        add     a0, a0, s0      ; s0 holds garbage
        ret
.base:
        li      a0, 0
        ret
