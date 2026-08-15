; GOOD: leaf function — no calls, so ra needs no save. 16-byte aligned frame;
; use a0-a7/t0-t6 freely (caller-saved). This is the callee-saved vs leaf
; distinction: a leaf can skip prologue/epilogue entirely.
        .text
        .globl  leaf
leaf:
        li      t0, 38
        mul     a0, a0, t0      ; a0 * 38, no memory, no calls
        ret
