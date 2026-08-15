; GOOD: saturating behavior chosen explicitly. `sqdmull` (signed saturating
; doubling multiply long) saturates; plain `mul`/`mla` wrap. Pick the one
; whose overflow semantics the algorithm requires, and document it.
        .text
        .globl  mla_good
mla_good:
        movi    v0.4s, #1
        movi    v1.4s, #0x7fffffff
        sqdmull  v0.2d, v1.2s, v1.2s  ; saturating doubling long multiply
        ret
