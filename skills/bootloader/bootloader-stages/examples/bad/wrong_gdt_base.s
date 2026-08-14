# BAD: wrong GDT base in the real -> protected mode transition.
# Teaching: lgdt takes a 6-byte pseudo-descriptor {limit, base}. The base must
# be the ADDRESS of the GDT. Loading any other value makes every segment
# descriptor lookup read garbage, so after CR0.PE the first memory access via a
# segment register faults (#GP) or silently uses a corrupted descriptor.
# Second variant: the pseudo-descriptor base is correct but the code descriptor
# itself has a nonzero base, which offsets every address below it.
# These assemble cleanly (gcc -c) but fault or corrupt memory on real hardware.

.text
.code16

# BAD 1: pseudo-descriptor base points at 0x00012345, not at gdt_real.
.globl bad_gdt_wrong_base
bad_gdt_wrong_base:
    cli
    lgdt    wrong_base_desc       # base field = 0x00012345, not &gdt_real
    mov     %cr0, %eax
    or      $1, %eax              # CR0.PE = 1
    mov     %eax, %cr0
    ljmp    $0x08, $prot_bad_base # far jump through a corrupted GDT

.code32
prot_bad_base:
    mov     $0x10, %ax
    mov     %ax, %ds
    movl    $1, (%eax)            # WRONG: first protected-mode access faults
    ret

.section .rodata
gdt_real:
    .long   0x00000000, 0x00000000       # null
    .long   0x0000ffff, 0x00cf9a00       # flat code, base 0
    .long   0x0000ffff, 0x00cf9200       # flat data, base 0

# BAD 1 operand: limit is right, base is a magic number that is NOT the GDT.
wrong_base_desc:
    .word   23
    .long   0x00012345             # WRONG: GDT is at gdt_real, not here

# BAD 2: base is correct, but the code descriptor has base = 0x00010000.
# After the far jump, every linear address is segment_base + offset, so all
# instruction fetches and data references are shifted by 64 KiB.
.globl bad_gdt_nonflat_desc
bad_gdt_nonflat_desc:
    cli
    lgdt    nonflat_desc
    mov     %cr0, %eax
    or      $1, %eax
    mov     %eax, %cr0
    ljmp    $0x08, $prot_bad_nonflat

.code32
prot_bad_nonflat:
    mov     $0x10, %ax
    mov     %ax, %ds
    movl    $7, 0x100000           # WRONG: lands at 0x100000 + 0x10000 base
    ret

.section .rodata
nonflat_gdt:
    .long   0x00000000, 0x00000000
    .long   0x0000ffff, 0x00cf9a01 # WRONG: second dword byte 4 = base 23:16
                                   # is 0x01, so base = 0x00010000, not 0
    .long   0x0000ffff, 0x00cf9200
nonflat_desc:
    .word   23
    .long   nonflat_gdt
