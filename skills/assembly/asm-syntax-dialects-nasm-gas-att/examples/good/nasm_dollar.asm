; GOOD: NASM 3.x — no `$` inside directive arguments. `global main`/`extern puts`
; take plain symbol names; `$` is reserved for the current-location counter
; inside expressions (e.g. `times 2-$ dd 0`).
        section .text
        global  main
        extern  puts
main:
        ret
