; GOOD: callee-saved usage — s0 is saved and restored around the call to
; helper. The caller's s0 survives.
        .text
        .globl  caller
caller:
        addi    sp, sp, -16
        sd      ra, 8(sp)
        sd      s0, 0(sp)       ; save caller's s0 before we touch it
        mv      s0, a0          ; now safe to use s0
        call    helper
        ld      s0, 0(sp)       ; restore
        ld      ra, 8(sp)
        addi    sp, sp, 16
        ret

        .globl  helper
helper:
        li      t0, 5
        mul     a0, a0, t0      ; uses only caller-saved regs
        ret
