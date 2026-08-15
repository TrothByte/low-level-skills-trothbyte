; intentionally incorrect
; BAD: `$` in a NASM 3.x directive argument. `global $foo`/`extern $bar` worked
; in NASM 2.x but the `$` is invalid there in 3.x (BBoeOS PR#506). In NASM `$`
; is only the current-location counter inside expressions.
        section .text
        global  $main
        extern  $puts
main:
        ret
