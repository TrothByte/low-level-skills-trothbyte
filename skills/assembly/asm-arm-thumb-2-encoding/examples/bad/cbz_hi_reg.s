; intentionally incorrect
; BAD: `cbz`/`cbnz` (CBZ/CBNZ) are only encodable with LOW registers r0-r7
; (Thumb-2 encoding T1, bits 7:3 of Rt). Using r9 or r10 is invalid — this is
; the HerraduraKEx PR#33 root cause. Assemblers reject it with an
; "branch must be conditional" style error or "operand r9 out of range".
        .syntax unified
        .thumb
        .text
        .globl  f
f:
        cbz     r9, .L1      ; BAD: cbz only takes r0-r7
        cbnz    r10, .L2     ; BAD: cbnz only takes r0-r7
.L1:
        bx      lr
.L2:
        bx      lr
