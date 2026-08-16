# GOOD: iterate when the BPF verifier output is opaque. The loop:
#   extract failing instruction + register state from the log tail
#   minimize the program (delta-debug) while preserving the failure signature
#   bisect the register/pointer state to its creating instruction
#   repair minimally, re-verify with the same loop
# Motivation (lwn.net/Articles/1075067): verifier error dumps "are insane" and
# writing BPF code is the only case where LLMs give up rather than producing
# something. Persistence on the load loop is the skill. Runs with python 3.11.

import re


# ---- a tiny verifier that produces realistic, opaque-looking logs --------
def verify(prog):
    """prog: list of instructions. Returns (ok, log_text)."""
    regs = {}          # rN -> (type, off, r)
    log = []
    for idx, insn in enumerate(prog):
        op = insn[0]
        if op == "data":
            r = insn[1]
            regs[r] = ("PTR_TO_PACKET", 0, 0)
            log.append(f"{idx}: (bf) r{r} = ctx->data")
        elif op == "end":
            r = insn[1]
            regs[r] = ("PTR_TO_PACKET_END", 0, 0)
            log.append(f"{idx}: (bf) r{r} = ctx->data_end")
        elif op == "addi":
            r, imm = insn[1], insn[2]
            st = regs.get(r)
            if st and st[0] == "PTR_TO_PACKET":
                regs[r] = (st[0], st[1] + imm, st[2])
            log.append(f"{idx}: (07) r{r} += {imm}")
        elif op == "check":
            base, endr, size = insn[1], insn[2], insn[3]
            st, en = regs.get(base), regs.get(endr)
            log.append(f"{idx}: (15) if r{base} + {size} > r{endr} goto +2")
            if st and st[0] == "PTR_TO_PACKET" and en and en[0] == "PTR_TO_PACKET_END":
                regs[base] = (st[0], st[1], size)
        elif op == "ldw":
            dst, base, off = insn[1], insn[2], insn[3]
            st = regs.get(base)
            log.append(f"{idx}: (79) r{dst} = *(u64 *)(r{base} + {off})")
            if st is None:
                log.append(f"{idx}: R{base} !read_ok")
            elif st[0] != "PTR_TO_PACKET":
                log.append(f"{idx}: R{base} type={st[0]} invalid mem access")
            elif st[2] < off + 8:
                log.append(f"{idx}: R{base} type=PTR_TO_PACKET(id=0,off={st[1]},r={st[2]})")
                log.append(f"{idx}: invalid access to packet, off={off} size=8, "
                           f"R{base}(id=0,off={st[1]},r={st[2]})")
                log.append(f"processed {idx+1} insns (limit 1000000) "
                           "max_states_per_insn 0 total_states 1 peak_states 1 mark_read 1")
                return False, "\n".join(log)
            else:
                regs[dst] = ("SCALAR", 0, 0)
        elif op == "exit":
            log.append(f"{idx}: (95) exit")
    log.append(f"processed {len(prog)} insns (limit 1000000) "
               "max_states_per_insn 0 total_states 1 peak_states 1 mark_read 1")
    return True, "\n".join(log)


# ---- iteration machinery -------------------------------------------------
def extract(log):
    """Return (failing_insn_index, register_state_line, message)."""
    lines = log.splitlines()
    msg = None
    for ln in reversed(lines):
        if any(k in ln for k in ("invalid access", "!read_ok",
                                 "unbounded memory")):
            msg = ln
            break
    mi = lines.index(msg)
    fidx = None
    for j in range(mi, -1, -1):
        m = re.match(r"^(\d+):", lines[j])
        if m:
            fidx = int(m.group(1))
            break
    reg_state = None
    for j in range(mi - 1, -1, -1):
        if re.match(r"^(\d+):", lines[j]):
            reg_state = lines[j]
            break
    return fidx, reg_state, msg


def signature(ok, log):
    """Failure signature: (message-kind, named register) -- invariant under
    removal of unrelated instructions (absolute indices are NOT part of it)."""
    if ok:
        return None
    _, _, msg = extract(log)
    m = re.search(r"R(\d+)", msg)
    reg = int(m.group(1)) if m else None
    kind = re.sub(r"^\d+:\s*", "", msg).split(",", 1)[0].strip()
    return (kind, reg)


def minimize(prog, verify_fn):
    cur = list(prog)
    sig = signature(*verify_fn(cur))
    changed = True
    while changed:
        changed = False
        for i in range(len(cur)):
            trial = cur[:i] + cur[i + 1:]
            if trial and signature(*verify_fn(trial)) == sig:
                cur = trial
                changed = True
                break
    return cur


def bisect_creator(prog, target_reg):
    """Which instruction created the failing register's state?"""
    for idx, insn in enumerate(prog):
        if insn[0] == "data" and insn[1] == target_reg:
            return idx, "ctx->data load", "PTR_TO_PACKET(id=0,off=0,r=0)"
    return None, None, None


def repair(prog, msg):
    m = re.search(r"R(\d+)\(id=0,off=(\d+),r=(\d+)\)", msg)
    base = int(m.group(1)) if m else 1
    fixed = list(prog)
    load_idx = next(i for i, insn in enumerate(fixed)
                    if insn[0] == "ldw" and insn[2] == base)
    endr, size = 9, 8
    fixed.insert(load_idx, ("check", base, endr, size))
    fixed.insert(load_idx, ("end", endr))
    return fixed


# ---- the program under test ----------------------------------------------
PROG = [
    ("data", 1),
    ("addi", 1, 0),
    ("addi", 1, 0),
    ("ldw", 0, 1, 0),
    ("exit",),
]

if __name__ == "__main__":
    print("iterate on the opaque verifier log\n")
    ok, log = verify(PROG)
    assert not ok
    print("opaque verifier dump:")
    for ln in log.splitlines():
        print(f"  {ln}")

    fidx, reg_state, msg = extract(log)
    print(f"\nstep 1 (extract): failing insn = {fidx}, "
          f"message = {msg!r}")
    print(f"                  register state = {reg_state!r}")

    min_p = minimize(PROG, verify)
    print(f"step 2 (minimize): {len(PROG)} insns -> {len(min_p)} insns")
    print(f"                  minimal failing program: {min_p}")

    creator, desc, state = bisect_creator(min_p, 1)
    print(f"step 3 (bisect): r1 state = {state} created at insn {creator} "
          f"({desc})")

    fixed = repair(min_p, msg)
    print(f"step 4 (repair): insert ctx->data_end + bounds check before the "
          f"failing load")
    print(f"                  fixed program: {fixed}")

    ok2, log2 = verify(fixed)
    assert ok2, log2
    print("step 5 (re-verify): PASS -- program loads cleanly")
    print("\nRESULT: extract -> minimize -> bisect -> repair -> re-verify.")
    print("The opaque dump never justified giving up.")
