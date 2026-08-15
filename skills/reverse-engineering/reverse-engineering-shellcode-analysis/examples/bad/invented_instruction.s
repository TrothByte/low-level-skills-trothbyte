# intentionally incorrect — transcription contains a HALLUCINATED instruction.
# This is a "transcript" of examples/good/stub.s that an agent produced; it has
# an extra `dec %al` that is NOT present in the actual bytes (the 2023
# ArchCloudLabs GPT-3 failure mode: "decrements AL" invented from nowhere).
# `dec %al` is a real instruction and assembles cleanly — that is exactly why
# assembly success is NOT evidence of byte-identity with the source blob.
    .text
    .globl  _start
_start:
    movl    $41, %eax
    movl    $2, %edi
    movl    $1, %esi
    xorl    %edx, %edx
    syscall
    movl    %eax, %edi
    dec     %al             # HALLUCINATED: not in the original bytes
    xorl    %ecx, %ecx
    pushq   %rcx
    pushq   $0x0100007f
    pushq   $0x5c110002
    movq    %rsp, %rsi
    movl    $42, %eax
    movl    $16, %edx
    syscall
    movl    $1, %eax
    leaq    msg(%rip), %rsi
    movl    $11, %edx
    syscall
    movl    $60, %eax
    xorl    %edi, %edi
    syscall

.section .rodata
msg:
    .ascii  "connected\n"
