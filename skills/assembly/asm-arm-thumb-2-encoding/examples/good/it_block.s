; GOOD: every conditionally-executed Thumb-2 instruction sits inside its IT
; block. ITEQ:if-equal, ITT:if-then-then, ITE:if-then-else. The number of
; letters after IT determines the chain length.
        .syntax unified
        .thumb
        .text
        .globl  f
f:
        cmp     r0, r1
        iteq
        moveq   r0, #0        ; inside ITEQ: conditionally executed
        itte    ne
        movne   r1, #1
        movne   r2, #2
        bx      lr
