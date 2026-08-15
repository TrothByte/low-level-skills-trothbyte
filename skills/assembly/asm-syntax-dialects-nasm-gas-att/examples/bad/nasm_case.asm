; intentionally incorrect
; BAD: NASM labels are case-sensitive. `loop` (the instruction), `Loop` and
; `LOOP` are three different identifiers. A loop jumping to the wrong case
; silently assembles to an infinite loop or a stray jump. This is ocrosby
; PR#33 error class 1.
        section .text
        global  main
main:
        mov     ecx, 3
.loop:  dec     ecx
        jnz     loop            ; BAD: lowercase label does not exist
        ret
