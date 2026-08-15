; GOOD: `default rel` near the top selects RIP-relative addressing on
; Mach-O/ELF PIE, so symbol references assemble to relocations the linker can
; resolve instead of absolute addresses.
        default rel
        section .data
msg:    db      "hi", 0
        section .text
        global  _main
_main:
        mov     rax, msg        ; RIP-relative under default rel
        ret
