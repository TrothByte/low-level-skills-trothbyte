; intentionally incorrect
; BAD: treating s0 as caller-saved. s0 is callee-saved (saved across calls);
; using it as a scratch register without saving it corrupts the caller's
; value. The confusion between callee-saved (s0-s11) and caller-saved
; (a0-a7, t0-t6) registers is a recorded failure class.
        .text
        .globl  helper
helper:
        mv      s0, a0          ; BAD: clobbers caller's s0, not saved
        li      t0, 5
        mul     a0, a0, t0
        add     a0, a0, s0
        ret
