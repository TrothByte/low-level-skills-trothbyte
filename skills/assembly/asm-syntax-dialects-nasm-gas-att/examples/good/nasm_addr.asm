; GOOD: brackets make the difference. `mov rax, buf` = address; `mov rax, [buf]`
; = value at buf. Pick the one that matches the intent, and if both are used,
; the lea-equivalent is spelled `lea rax, [buf]`.
        section .data
buf:    dq      42
        section .text
        global  main
main:
        mov     rax, [buf]      ; value: 42
        lea     rbx, [buf]      ; address of buf (explicit)
        ret
