; intentionally incorrect
; BAD: NEON SIMD loop on AArch64 with no per-lane overflow guard for the
; counter. Every 255 iterations a byte lane wraps (256), so a loop that counts
; iterations in SIMD lanes overflows silently long before the scalar loop
; bound. Lemire's finding: SIMD counters need a horizontal reduce + guard at
; most every 255 iterations.
        .text
        .globl  loop_simd
loop_simd:
        movi    v0.16b, #0            ; per-byte counters
        mov     x1, #1000             ; iteration count
.Lloop:
        add     v0.16b, v0.16b, v1.16b ; BAD: byte lanes wrap at 256
        subs    x1, x1, #1
        b.ne    .Lloop
        ret
