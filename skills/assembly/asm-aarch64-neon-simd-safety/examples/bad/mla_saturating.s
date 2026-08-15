; intentionally incorrect
; BAD: assumes NEON integer multiply-accumulate with saturating semantics.
; `sqdmull` is signed saturating doubling multiply long; plain `mla`/`mul`
; are NOT saturating. Mixing them changes overflow behavior silently — the
; bitmask arithmetic (saturate vs wrap) is a recorded failure class.
        .text
        .globl  mla_bad
mla_bad:
        movi    v0.4s, #1
        movi    v1.4s, #0x7fffffff
        mla     v0.4s, v1.4s, v1.4s   ; BAD: plain mla wraps on overflow
        ret
