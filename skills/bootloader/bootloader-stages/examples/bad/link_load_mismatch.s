# BAD: stage-2 code that assumes load address == link address.
# Teaching: the linker resolves absolute references against the link-time
# layout (here 0x100000). The stage-1 loader placed the image at 0x9000 and
# applied no relocation. Every absolute address is now off by the delta
# (load - link), so the bootloader reads the wrong data and jumps to the wrong
# function. PC-relative control flow (call) survives only because the image is
# contiguous; absolute data references do not.
# This assembles cleanly (gcc -c); at runtime it reads garbage.

.text
.code32

# Entry called by the stage-1 loader after the copy to 0x9000.
.globl stage2_bad_entry
stage2_bad_entry:
    movl    $bad_msg, %eax         # WRONG: link-time absolute address (0x100000)
    movb    (%eax), %cl            # loads from 0x100000, not from 0x9000
    testb   %cl, %cl
    jz      .Lempty
    call    bad_print              # PC-relative: works within a contiguous image
.Lempty:
    ret

bad_print:
    movl    $0x0003f8, %edx        # COM1
    movb    %cl, %al
    outb    %al, %dx
    ret

.section .rodata
bad_msg:
    .asciz  "wrong address"
