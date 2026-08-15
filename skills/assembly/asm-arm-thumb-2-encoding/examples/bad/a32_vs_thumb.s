; intentionally incorrect
; BAD: assuming ARM (A32) and Thumb share the same encodings. In Thumb-2 the
; same mnemonic has a different encoding (often 16-bit), and a 32-bit
; instruction mis-encoded as two 16-bit ones corrupts the instruction stream.
; Here `mrs`/`msr` PSR forms differ between A32 and Thumb-2.
        .syntax unified
        .thumb
        .text
        .globl  f
f:
        mrs     r0, cpsr      ; BAD: in Thumb-2 the MRS encoding differs;
                              ; 16/32-bit split can desync the decoder
        bx      lr
