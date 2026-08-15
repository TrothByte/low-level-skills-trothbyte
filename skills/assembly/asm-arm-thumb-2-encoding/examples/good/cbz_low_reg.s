; GOOD: cbz/cbnz with LOW registers r0-r7 only, as Thumb-2 encoding T1
; requires. For hi-registers, use cmp + beq/bne instead.
        .syntax unified
        .thumb
        .text
        .globl  f
f:
        cbz     r3, .L1       ; ok: r0-r7 allowed
        cbnz    r7, .L2       ; ok: r0-r7 allowed
        cmp     r9, #0        ; hi-register zero-test
        beq     .L1
.L1:
        bx      lr
.L2:
        bx      lr
