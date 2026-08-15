; GOOD: every memory operand with an ambiguous size gets an explicit size
; hint: `inc qword [counter]`, `mov dword [x], eax`, etc.
        section .data
counter: dq 0
        section .text
        global  main
main:
        inc     qword [counter] ; explicit qword size
        mov     dword [counter], 7
        ret
