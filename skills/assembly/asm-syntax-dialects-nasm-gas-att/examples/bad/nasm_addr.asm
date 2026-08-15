; intentionally incorrect
; BAD: `mov rax, buf` in NASM loads the ADDRESS of buf into rax — a
; lea-equivalent, not the value at buf. `mov rax, [buf]` loads the value.
; "It's not an error, it's just not what you meant" — the classic silent
; address-vs-content confusion (ocrosby PR#33 error class 2).
        section .data
buf:    dq      42
        section .text
        global  main
main:
        mov     rax, buf        ; BAD: rax = address of buf
        mov     rbx, [buf]      ; correct: rbx = 42
        ret
