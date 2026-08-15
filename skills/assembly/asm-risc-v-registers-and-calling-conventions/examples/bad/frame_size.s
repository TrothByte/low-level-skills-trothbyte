; intentionally incorrect
; BAD: 4-byte stack frame instead of 8 for (s0 + ra). On RV64 the stack is
; 16-byte aligned and each slot is 8 bytes; a 4-byte frame misaligns sp and
; overlays s0 and ra. The recursive sum returns garbage.
        .text
        .globl  sum
sum:
        beqz    a0, .base
        addi    sp, sp, -4      ; BAD: 4-byte frame on RV64
        sw      s0, 0(sp)
        sw      ra, 4(sp)       ; BAD: overlaps the next slot / misaligned
        addi    a0, a0, -1
        call    sum
        lw      ra, 4(sp)
        lw      s0, 0(sp)
        addi    sp, sp, 4
        ret
.base:
        li      a0, 0
        ret
