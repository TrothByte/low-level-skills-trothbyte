; GOOD: short branches for nearby targets, conditional branch + B.W for far
; ones. CBZ covers about +/-126 bytes; beyond that, invert the condition and
; use a wide branch, letting the linker/assembler resolve ranges.
        .syntax unified
        .thumb
        .text
        .globl  f
f:
        cbnz    r0, .Lnear    ; within range
        bx      lr
.Lnear:
        bx      lr

        .globl  g
g:
        cbz     r0, .Lskip    ; near: within imm6<<1
        b       .Lfar_do      ; far: explicit wide/near branch for the work
        b       .Lskip
.Lskip:
        bx      lr
.Lfar_do:
        b.w     .Lfar_target  ; 32-bit wide branch for the far target
        .space  4096
.Lfar_target:
        bx      lr
