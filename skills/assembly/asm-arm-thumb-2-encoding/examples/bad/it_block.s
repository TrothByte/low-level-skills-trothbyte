; intentionally incorrect
; BAD: missing the IT (if-then) block around a conditionally-executed Thumb-2
; instruction. In Thumb-2, conditional execution is done with IT; a bare
; conditional mnemonic like `moveq` outside an IT block assembles to an
; unconditional encoding (or is rejected), silently running always.
        .syntax unified
        .thumb
        .text
        .globl  f
f:
        cmp     r0, r1
        moveq   r0, #0        ; BAD: no ITEQ before this -> wrong semantics
        bx      lr
