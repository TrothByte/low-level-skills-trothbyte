; intentionally incorrect
; BAD: assuming CBZ/CBNZ can jump to arbitrary 32-bit addresses. Thumb-2 CBZ
; uses a signed 6-bit immediate offset (imm6<<1, range about +/-126 bytes
; from the instruction), not the full 32-bit literal of a B.W. A far target
; must use a conditional branch + B, or the assembler inserts a veneer/literal
; pool that you must account for.
        .syntax unified
        .thumb
        .text
        .globl  f
f:
        cbz     r0, .far_target   ; BAD: .far_target is >126 bytes away
        bx      lr
        .space  512
.far_target:
        bx      lr
