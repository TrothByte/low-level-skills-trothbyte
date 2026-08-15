# intentionally incorrect — byte-order error in IP extraction.
# The agent claims `pushq $0x7f000001` loads 127.0.0.1. On a little-endian
# machine a pushed dword value 0x7f000001 occupies memory as bytes
# 01 00 00 7f, which the kernel reads as s_addr 0x0100007f = 1.0.0.127.
# For 127.0.0.1 the pushed dword must be 0x0100007f (memory bytes 7f 00 00 01).
# The stub assembles cleanly — the error is semantic, invisible to objdump.
    .text
    .globl  _start
_start:
    movl    $41, %eax
    movl    $2, %edi
    movl    $1, %esi
    xorl    %edx, %edx
    syscall
    movl    %eax, %edi

    xorl    %ecx, %ecx
    pushq   %rcx              # sin_zero
    pushq   $0x7f000001       # WRONG dword for 127.0.0.1 -> bytes 01 00 00 7f
    pushq   $0x5c110002       # AF_INET | port 4444
    movq    %rsp, %rsi

    movl    $42, %eax
    movl    $16, %edx
    syscall
    movl    $60, %eax
    xorl    %edi, %edi
    syscall
