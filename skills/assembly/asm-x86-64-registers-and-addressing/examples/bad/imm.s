# BAD: hand-encoded bytes `48 C7 C0 FF FF FF FF` decode to `mov rax,-1`
# (C7 /0 sign-extends the imm32), not to `mov rax,0x00000000FFFFFFFF`.
# The compact imm32 encoding only represents signed 32-bit values; a full 64-bit
# constant needs the B8+imm64 movabs form (reference rule 10).
	.text
	.globl	bad_imm32
bad_imm32:
	.byte	0x48, 0xC7, 0xC0, 0xFF, 0xFF, 0xFF, 0xFF
	ret
