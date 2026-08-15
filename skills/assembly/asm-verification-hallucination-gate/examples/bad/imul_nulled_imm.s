# intentionally incorrect
# BAD: hand-encoded "imul eax,eax,38" with the immediate accidentally nulled
# (BBoeOS PR#584 root cause). Bytes 69 C0 00 00 00 00 decode to
# imul $0x0,%eax,%eax — multiplying by zero, not by 38. Disassembly exposes it.
	.text
	.globl	f
f:
	.byte	0x69, 0xC0, 0x00, 0x00, 0x00, 0x00
	ret
