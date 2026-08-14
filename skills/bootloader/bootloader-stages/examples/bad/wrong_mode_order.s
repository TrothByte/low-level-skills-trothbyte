# BAD: long-mode enable in the wrong order.
# Teaching: Intel SDM Vol 3A 9.8.5 requires CR4.PAE -> EFER.LME -> CR3 ->
# CR0.PG, then a far jump into an L=1 (64-bit) code descriptor. Two bugs here:
# (1) CR0.PG is set while EFER.LME=1 and CR4.PAE=0, which is a #GP(0) on the
#     MOV CR0 itself;
# (2) even when paging comes up, CS still selects a 32-bit (L=0) descriptor, so
#     the CPU runs compatibility mode and decodes 32-bit instructions where
#     64-bit code was expected (garbage decode / triple fault).
# This assembles cleanly (gcc -c); on real hardware it faults at the MOV CR0.

.text
.code16

.globl bad_long_mode_order
bad_long_mode_order:
    cli
    lgdt    gdt_desc_bad
    mov     %cr0, %eax
    or      $1, %eax
    mov     %eax, %cr0            # enter protected mode
    ljmp    $0x08, $prot_bad32

.code32
prot_bad32:
    mov     $0x10, %ax
    mov     %ax, %ds
    mov     $0xc0000080, %ecx
    rdmsr
    or      $0x100, %eax          # EFER.LME = 1
    wrmsr
    mov     %cr0, %eax
    or      $0x80000000, %eax
    mov     %eax, %cr0            # WRONG: PG=1 with PAE=0 -> #GP(0)
    ret

.section .rodata
gdt_bad_mode:
    .long   0x00000000, 0x00000000
    .long   0x0000ffff, 0x00cf9a00 # 32-bit code (L=0): never reloaded to L=1
    .long   0x0000ffff, 0x00cf9200
gdt_desc_bad:
    .word   23
    .long   gdt_bad_mode
