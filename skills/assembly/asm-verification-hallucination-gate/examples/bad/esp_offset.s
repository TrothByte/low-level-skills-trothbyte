# intentionally incorrect
# BAD: stack offset off by one slot (HerraduraKEx PR#33 root cause). The data
# being read lives at (%rsp); 4(%rsp) / 8(%rsp) skips to a different slot, so
# "products are written to the wrong slot". Assembles cleanly — runtime bug.
	.text
	.globl	f
f:
	movl	8(%rsp), %eax
	ret
