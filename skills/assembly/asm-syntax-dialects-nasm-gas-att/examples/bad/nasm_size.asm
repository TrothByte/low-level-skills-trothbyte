; intentionally incorrect
; BAD: missing size hint. `inc [counter]` is ambiguous — NASM needs
; `inc qword [counter]`. The error message asks for the size hint; leaving it
; out does not infer a default (ocrosby PR#33 error class 3).
        section .data
counter: dq 0
        section .text
        global  main
main:
        inc     [counter]       ; BAD: NASM: "operation size not specified"
        ret
