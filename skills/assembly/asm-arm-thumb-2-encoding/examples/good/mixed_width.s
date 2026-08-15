; GOOD: Thumb-2 state is explicit: .thumb, and instructions are a mix of 16-bit
; and 32-bit encodings chosen by the assembler. Mixed widths are normal; never
; hand-patch bytes into a Thumb stream without knowing which encoding each
; instruction takes.
        .syntax unified
        .thumb
        .text
        .globl  f
f:
        push    {r4, lr}          ; 16-bit encoding
        movs    r0, #38           ; 16-bit
        add     r0, r0, #1        ; 16-bit
        pop     {r4, pc}          ; 16-bit
