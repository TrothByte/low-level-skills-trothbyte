; GOOD: NASM labels are case-sensitive; keep every reference to a label in the
; exact case of its definition. Mnemonics themselves are case-insensitive.
        section .text
        global  main
main:
        mov     ecx, 3
.loop:  dec     ecx
        jnz     .loop           ; references the local label as defined
        ret
