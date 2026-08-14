# BAD: hand-encoded bytes `8B 00` decode to `mov (%rax),%eax`, not the intended
# `mov (%r8),%eax`. Accessing r8-r15 requires the REX.B prefix (41). The assembler
# emits it automatically when you write the register name — this trap only appears
# when bytes are patched or hand-assembled (reference rule 6).
	.text
	.globl	bad_rex
bad_rex:
	.byte	0x8B, 0x00		# decodes as mov (%rax),%eax
	ret
