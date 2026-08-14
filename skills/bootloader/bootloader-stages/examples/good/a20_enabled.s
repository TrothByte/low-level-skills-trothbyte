# GOOD: enable the A20 gate through the 8042 keyboard controller, verify it
# with the 1 MiB wrap probe, and only then enter protected mode.
# Teaching: ports 0x64 = status/command, 0x60 = data. Sequence:
#   status bit 1 clear (input buffer empty)
#   command 0xD1 = "write to the 8042 output port"
#   data 0xDF = A20 enable (bit 1 set)
# Then the probe: write 0x00 to 0x000000 and 0xFF to 0x100000; if reading back
# 0x000000 still yields 0x00, the write did NOT wrap, so A20 is on. Never trust
# the gate without the probe.
# Assemble with: gcc -c a20_enabled.s

.text
.code16

.globl good_a20
good_a20:
    cli
    call    .Lwait_in
    movb    $0xd1, %al             # command: write to 8042 output port
    outb    %al, $0x64
    call    .Lwait_in
    movb    $0xdf, %al             # data: enable A20
    outb    %al, $0x60
    call    .Lwait_out
    call    .Lprobe_a20
    testl   %eax, %eax
    jnz     .La20_broken
    lgdt    gdt_desc_a20g
    mov     %cr0, %eax
    or      $1, %eax
    mov     %eax, %cr0             # A20 verified on: safe to enable protected mode
    ret

.Lwait_in:                          # status bit 1 clear = input buffer empty
    inb     $0x64, %al
    testb   $2, %al
    jnz     .Lwait_in
    ret

.Lwait_out:                         # status bit 0 set = output buffer full
    inb     $0x64, %al
    testb   $1, %al
    jz      .Lwait_out
    ret

.Lprobe_a20:                        # 1 MiB wrap probe, segment:offset form
    pushf                           # 0x1000:0 -> physical 0x100000
    push    %es
    cli
    xorw    %ax, %ax
    movw    %ax, %es
    movb    $0x00, %es:(0)          # physical 0x000000 = 0x00
    movw    $0x1000, %ax
    movw    %ax, %es
    movb    $0xff, %es:(0)          # physical 0x100000 = 0xff
    xorw    %ax, %ax
    movw    %ax, %es
    cmpb    $0x00, %es:(0)          # if A20 off, the 0xff wrapped to 0x000000
    jne     .La20_off
    xorl    %eax, %eax              # A20 on
    jmp     .La20_done
.La20_off:
    movl    $1, %eax                # A20 still off
.La20_done:
    pop     %es
    popf
    ret

.La20_broken:
    hlt                             # halt visibly instead of booting wrapped memory
    jmp     .La20_broken

.section .rodata
gdt_a20g:
    .long   0x00000000, 0x00000000
    .long   0x0000ffff, 0x00cf9a00
    .long   0x0000ffff, 0x00cf9200
gdt_desc_a20g:
    .word   23
    .long   gdt_a20g
