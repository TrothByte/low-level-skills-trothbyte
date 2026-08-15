# intentionally incorrect
# BAD: assembles cleanly but does the opposite of the intent. The agent wants
# "rax = rbx" (copy rbx into rax) but writes Intel-style src,dst. In AT&T the
# source comes first, so this actually stores rax INTO rbx. Silent corruption.
	.text
	.globl	f
f:
	movq	%rax, %rbx
	ret
