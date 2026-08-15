; GOOD: the counter lives in 64-bit lanes so it cannot wrap during the loop;
; per-lane overflow is impossible for the guard itself. SIMD counters should
; be sized so lane width exceeds (iterations * step).
        .text
        .globl  loop_simd_good
loop_simd_good:
        movi    v0.2d, #0             ; two 64-bit counters
        mov     x1, #1000
.Lloop:
        add     v0.2d, v0.2d, v1.2d   ; 64-bit lanes, no wrap for 1000 iters
        subs    x1, x1, #1
        b.ne    .Lloop
        ret
