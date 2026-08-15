# Linux x86-64 TCP connect-back shellcode stub — analysis fixture.
# Purpose: teach byte-accurate shellcode reading. Assembles with gcc -c on this
# host (COFF object); the instructions and immediates below are target-identical
# to the Linux ELF case. Linux syscalls per syscall_64.tbl:
#   read=0 write=1 socket=41 connect=42 bind=49 listen=50 exit=60
    .text
    .globl  _start
_start:
    # socket(AF_INET=2, SOCK_STREAM=1, 0) -> syscall 41, fd in %eax
    movl    $41, %eax
    movl    $2, %edi
    movl    $1, %esi
    xorl    %edx, %edx
    syscall
    movl    %eax, %edi       # fd -> first syscall argument

    # Build struct sockaddr_in (16 bytes) on the stack:
    #   offset 0..1 sin_family = AF_INET (0x0002)
    #   offset 2..3 sin_port   = 0x115c = 4444 (network byte order)
    #   offset 4..7 sin_addr   = 127.0.0.1
    #   offset 8..15 sin_zero  = 0
    # Pushes go high-to-low; the last push lands at offset 0.
    xorl    %ecx, %ecx
    pushq   %rcx              # sin_zero, covers offsets 8..15
    pushq   $0x0100007f       # 127.0.0.1 as pushed dword -> memory bytes 7f 00 00 01
    pushq   $0x5c110002       # AF_INET(0x0002) | port 4444 -> memory bytes 02 00 11 5c
    movq    %rsp, %rsi        # struct sockaddr_in * (fd is already in %edi)

    # connect(fd, sockaddr, 16) -> syscall 42
    movl    $42, %eax
    movl    $16, %edx
    syscall

    # write(fd, msg, 11) -> syscall 1
    movl    $1, %eax
    leaq    msg(%rip), %rsi
    movl    $11, %edx
    syscall

    # exit(0) -> syscall 60
    movl    $60, %eax
    xorl    %edi, %edi
    syscall

.section .rodata
msg:
    .ascii  "connected\n"
