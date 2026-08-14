# GOOD: correct real -> protected mode entry.
# Teaching: the four mandatory steps are
#   1. build a flat GDT: null + code + data, all bases 0, P/S/DPL/type set;
#   2. lgdt with a pseudo-descriptor whose base REALLY is the GDT address;
#   3. set CR0.PE and take a far jump into the 32-bit code selector — the far
#      jump is what actually reloads CS from the GDT;
#   4. reload DS/ES/SS with the flat data selector and set a stack.
# Encodings: code 0x00cf9a00 (G=1 D=1 P=1 S=1, type code), data 0x00cf9200.
# Assemble with: gcc -c correct_gdt.s

.text
.code16

.globl good_gdt
good_gdt:
    cli
    lgdt    gdt_desc
    mov     %cr0, %eax
    or      $1, %eax               # CR0.PE = 1
    mov     %eax, %cr0
    ljmp    $0x08, $prot_good      # far jump -> CS = flat code descriptor

.code32
prot_good:
    mov     $0x10, %ax             # flat data selector
    mov     %ax, %ds
    mov     %ax, %es
    mov     %ax, %ss
    xorl    %esp, %esp
    movl    $0x00090000, %esp      # stack in guaranteed RAM (demo target only)
    ret

.section .rodata
gdt:
    .long   0x00000000, 0x00000000       # null descriptor
    .long   0x0000ffff, 0x00cf9a00       # code: base 0, limit 0xfffff, G=1 D=1
    .long   0x0000ffff, 0x00cf9200       # data: base 0, limit 0xfffff, G=1 D=1
gdt_end:
gdt_desc:
    .word   gdt_end - gdt - 1
    .long   gdt
