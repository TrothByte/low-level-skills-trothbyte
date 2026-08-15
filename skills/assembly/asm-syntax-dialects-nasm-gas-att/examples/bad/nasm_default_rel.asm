; intentionally incorrect
; BAD: missing `default rel` on Mach-O. Without it NASM defaults to absolute
; addressing, which Mach-O's no-absolute-address relocation model rejects or
; silently resolves wrongly for position-independent code (ocrosby PR#33
; error class 4).
        section .data
msg:    db      "hi", 0
        section .text
        global  _main
_main:
        mov     rax, msg        ; BAD: absolute addressing; needs default rel
        ret
