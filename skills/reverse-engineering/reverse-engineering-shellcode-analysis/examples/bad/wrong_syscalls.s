# intentionally incorrect — i386 syscall numbers used on x86-64.
# Every instruction assembles (exit 0); every syscall is wrong on Linux x86-64,
# where syscall_64.tbl says write=1, exit=60 (not write=4, exit=1 like i386).
# The transcription "reads fine" but the behavior described is not what the
# target kernel will do.
    .text
    .globl  _start
_start:
    movl    $4, %eax        # agent: "write()" — 4 is write ONLY on i386
    movl    $1, %edi
    leaq    msg(%rip), %rsi
    movl    $11, %edx
    syscall
    movl    $1, %eax        # agent: "exit()" — 1 is exit ONLY on i386
    xorl    %edi, %edi
    syscall

.section .rodata
msg:
    .ascii  "connected\n"
