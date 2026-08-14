# GOOD: complete real -> protected -> long mode transition in the order the
# Intel SDM requires (Vol 3A 9.8.5):
#   CR4.PAE -> EFER.LME -> CR3 (PML4) -> CR0.PG -> far jump to an L=1 (64-bit)
#   code descriptor.
# Teaching: the GDT keeps SEPARATE descriptors for the 32-bit entry (0x08) and
# the 64-bit long-mode entry (0x18). The final far jump is mandatory: after
# CR0.PG the CPU is in compatibility mode; only reloading CS with the L=1
# descriptor switches instruction decode to 64-bit long mode.
# Page tables identity-map the first 2 MiB with 4 levels (PML4/PDPT/PD/PT),
# every table 4-KiB aligned, entry flags 0x007 = present | writable | user.
# Assemble with: gcc -c mode_switch_order.s

.text
.code16

.globl good_long_mode
good_long_mode:
    cli
    lgdt    gdt_desc64
    mov     %cr0, %eax
    or      $1, %eax
    mov     %eax, %cr0              # 1. enter protected mode
    ljmp    $0x08, $pm32            # 32-bit code descriptor

.code32
pm32:
    mov     $0x10, %ax
    mov     %ax, %ds
    mov     $0x20, %eax
    mov     %eax, %cr4              # 2. CR4.PAE = 1 (before EFER.LME)
    mov     $0xc0000080, %ecx
    rdmsr
    or      $0x100, %eax
    wrmsr                           # 3. EFER.LME = 1
    mov     $pml4, %eax
    mov     %eax, %cr3              # 4. CR3 = PML4 base (identity map)
    mov     %cr0, %eax
    or      $0x80000000, %eax
    mov     %eax, %cr0              # 5. CR0.PG = 1: now compatibility mode
    ljmp    $0x18, $long64          # 6. far jump -> CS = L=1 code: long mode

.code64
long64:
    mov     $0x10, %ax              # flat data selector still valid
    mov     %ax, %ds
    mov     %ax, %es
    mov     %ax, %ss
    lea     lm_msg(%rip), %rdi      # RIP-relative: load-address independent
    ret

.section .rodata
gdt64:
    .long   0x00000000, 0x00000000       # null
    .long   0x0000ffff, 0x00cf9a00       # 0x08: 32-bit code (L=0, D=1)
    .long   0x0000ffff, 0x00cf9200       # 0x10: data
    .long   0x00000000, 0x00af9a00       # 0x18: 64-bit code (L=1, D=0)
gdt_end64:
gdt_desc64:
    .word   gdt_end64 - gdt64 - 1
    .long   gdt64

.section .data
.balign 4096
pml4:
    .quad   pdpt + 0x007
    .fill   511, 8, 0
pdpt:
    .quad   pd + 0x007
    .fill   511, 8, 0
pd:
    .quad   pt + 0x007
    .fill   511, 8, 0
pt:
    .quad   0x0000000000000003          # identity map page 0 (present, RW)
    .fill   511, 8, 0

.section .rodata
lm_msg:
    .asciz  "long mode"
