# BAD: giving up on an opaque verifier dump. The agent reads the dump,
# concludes "the log is too opaque to reason about", and produces no fix.
# This is the documented failure mode from lwn.net/Articles/1075067:
# writing BPF code is the only case where LLMs give up rather than producing
# something. The failing instruction is visible at line 4 and the message at
# line 6 -- extraction is trivial.
# # intentionally incorrect
#
# The correct loop is examples/good/good_verifier_log_iteration.py.

OPAQUE_LOG = """\
func#0 @0
0: (bf) r1 = ctx->data
1: (07) r1 += 0
2: (07) r1 += 0
3: (79) r0 = *(u64 *)(r1 + 0)
3: R1 type=PTR_TO_PACKET(id=0,off=0,r=0)
3: invalid access to packet, off=0 size=8, R1(id=0,off=0,r=0)
processed 4 insns (limit 1000000) max_states_per_insn 0 total_states 1 peak_states 1 mark_read 1
"""


def opaque_agent(log):
    lines = log.splitlines()
    print(f"  verifier log: {len(lines)} lines")
    print(f"  tail: ... {lines[-1][:60]}")
    print("  decision: 'the verifier dump is too opaque to parse; cannot")
    print("            identify the failing construct. Regenerating the")
    print("            whole program from scratch and trying again.'")
    return None


if __name__ == "__main__":
    print("agent response to an opaque verifier dump\n")
    fix = opaque_agent(OPAQUE_LOG)
    print(f"  fix produced: {fix!r}")
    print("  >>> GIVE-UP: no fix produced. The failing instruction is at")
    print("      line 5 ('3: (79) r0 = *(u64 *)(r1 + 0)'), the register state")
    print("      at line 6, the message at line 7. Extract, minimize, repair,")
    print("      re-verify instead of quitting (lwn.net/Articles/1075067).")
