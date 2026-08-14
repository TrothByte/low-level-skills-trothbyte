# BAD: protected mode is entered without enabling the A20 gate.
# Teaching: until A20 is on, memory accesses above 1 MiB wrap: physical
# 0x100000 aliases 0x000000. Code or data placed above 1 MiB is read from the
# wrong address, so the "stage-2 loaded at 0x100000" scenario silently executes
# wrapped code or corrupts low memory. A20 must be enabled AND verified (wrap
# probe) before CR0.PE when anything lives above 1 MiB.
# This assembles cleanly (gcc -c); on hardware it boots corrupted memory.

.text
.code16

.globl bad_a20_missing
bad_a20_missing:
    cli
    lgdt    gdt_desc_a20
    mov     %cr0, %eax
    or      $1, %eax
    mov     %eax, %cr0            # WRONG: CR0.PE set, A20 never enabled
    ljmp    $0x08, $prot_a20_bad

.code32
prot_a20_bad:
    mov     $0x10, %ax
    mov     %ax, %ds
    movl    $0x12345678, 0x100000 # WRONG: aliases physical 0x000000
    movl    0x000000, %ebx        # reads back the value written "above 1 MiB"
    ret

.section .rodata
gdt_a20:
    .long   0x00000000, 0x00000000
    .long   0x0000ffff, 0x00cf9a00
    .long   0x0000ffff, 0x00cf9200
gdt_desc_a20:
    .word   23
    .long   gdt_a20
