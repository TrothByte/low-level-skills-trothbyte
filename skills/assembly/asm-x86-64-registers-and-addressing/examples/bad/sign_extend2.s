# BAD: `movsbl` sign-extends a byte only to 32 bits; the 32-bit write then
# zero-extends to 64. Byte 0xFF gives %rax = 0x00000000FFFFFFFF, NOT
# 0xFFFFFFFFFFFFFFFF (reference rule 8).
	.text
	.globl	bad_byte_to_64
bad_byte_to_64:
	movsbl	(%rdi), %eax
	ret
