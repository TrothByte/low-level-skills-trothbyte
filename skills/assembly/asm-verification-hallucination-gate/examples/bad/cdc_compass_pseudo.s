# intentionally incorrect
# BAD: "JOB", "SST", "OCT" are CDC COMPASS pseudo-ops (a CDC 6600/CYBER
# dialect), not x86-64 GAS directives or mnemonics. GAS rejects each line
# (JOB parses as `jo` with a `b` suffix — operand mismatch; SST/OCT are
# unknown instructions). Symptom of a fabricated "assembly-looking" program.
	.text
	.globl	f
f:
	JOB
	SST
	OCT	10
	ret
