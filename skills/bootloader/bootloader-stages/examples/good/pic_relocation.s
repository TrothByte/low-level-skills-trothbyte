# GOOD: stage-2 relocation handled two correct ways.
# Teaching:
#   (1) Position-independent data access (long mode): RIP-relative addressing
#       computes the address from the instruction pointer, so the image works
#       at ANY aligned load address — no relocation pass needed.
#   (2) Classic relocation pass (32-bit): when non-PIC code must run at a
#       different address than it was linked for, add delta = load_addr -
#       link_addr to every absolute pointer. The table lists the link-time
#       addresses of the pointer slots; the pass adds the delta stored in edi.
# Assemble with: gcc -c pic_relocation.s

.text
.code64

.globl stage2_good_pic
stage2_good_pic:
    lea     good_msg(%rip), %rsi     # PIC: address computed from RIP
    movb    (%rsi), %cl              # correct wherever the image is loaded
    ret

.section .rodata
good_msg:
    .asciz  "address independent"

.text
.code32
.globl stage2_good_reloc
stage2_good_reloc:
    pushl   %esi
    movl    $reloc_table, %esi       # link-time address of the table
    movl    $reloc_count, %ecx
.Lreloc_loop:
    movl    (%esi), %edx             # link-time address of a pointer slot
    addl    %edi, (%edx)             # pointer += delta (edi holds load - link)
    addl    $4, %esi
    loop    .Lreloc_loop
    popl    %esi
    ret

.section .data
.balign 4
reloc_table:
    .long   abs_slot_0
    .long   abs_slot_1
reloc_count = 2

.data
abs_slot_0:
    .long   0x00000000
abs_slot_1:
    .long   0x00000000
