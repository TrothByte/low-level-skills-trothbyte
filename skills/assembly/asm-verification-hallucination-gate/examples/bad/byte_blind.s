# intentionally incorrect
# BAD: byte-blind claim. The agent asserts bytes 8B 00 are "mov (%r8),%eax".
# Without the REX.B prefix (0x41) they are actually "mov (%rax),%eax".
# Reading bytes requires a disassembler, not pattern matching by eye.
	.text
	.globl	f
f:
	.byte	0x8B, 0x00
	ret
