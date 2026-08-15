; GOOD: recursion with s0 properly saved AND its frame value established.
; s0 (frame pointer) is callee-saved: we save the caller's s0 at 0(sp) and
; restore it before returning. ra is also callee-saved across calls.
        .text
        .globl  sum
sum:
        beqz    a0, .base
        addi    sp, sp, -16     ; 16-byte aligned frame (RV64: 16-byte stack)
        sd      ra, 8(sp)       ; save ra (caller-saved across calls)
        sd      s0, 0(sp)       ; save s0 (callee-saved)
        addi    a0, a0, -1
        call    sum
        ld      s0, 0(sp)       ; restore s0 from OUR frame
        ld      ra, 8(sp)
        addi    sp, sp, 16
        ret
.base:
        li      a0, 0
        ret
