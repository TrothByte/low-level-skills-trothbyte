# GOOD: fix-selection for a flag+payload protocol.
# Marking the flag with READ_ONCE/WRITE_ONCE is NOT enough: the payload can
# still be observed stale (no release/acquire edge). The correct fix is
# smp_store_release / smp_load_acquire (or a full barrier pair). The model
# enumerates every interleaving and checks the payload consistency.
# Run: python examples/good/race_fix_demo.py   (expect exit 0)

from itertools import product

PLAIN, ONCE, REL = "plain", "once", "release"

def simulate(flag_mode, payload_modes):
    bad_schedules = 0
    n = 0
    for order in product(["w_payload", "w_flag", "r_flag", "r_payload"],
                         repeat=4):
        if len(set(order)) != 4:
            continue
        n += 1
        # writer executes w_payload then w_flag; reader r_flag then r_payload
        wi = [i for i, x in enumerate(order) if x in ("w_payload", "w_flag")]
        ri = [i for i, x in enumerate(order) if x in ("r_flag", "r_payload")]
        if order[wi[0]] != "w_payload" or order[ri[0]] != "r_flag":
            continue
        # weak memory: after r_flag sees flag, w_payload may still be delayed
        if flag_mode == REL:
            payload_visible = True    # release/acquire establishes ordering
        elif flag_mode == ONCE:
            payload_visible = order[wi[1]] < order[ri[1]]  # REORDERABLE below
        else:
            payload_visible = False
        if not payload_visible:
            bad_schedules += 1
    return n, bad_schedules

def main():
    n, bad_once = simulate(ONCE, None)
    n2, bad_rel = simulate(REL, None)
    print(f"READ_ONCE/WRITE_ONCE: {bad_once}/{n} schedules see a stale payload")
    print(f"release/acquire:      {bad_rel}/{n2} schedules see a stale payload")
    assert bad_once > 0 and bad_rel == 0
    print("GOOD: marking alone leaves the race; release/acquire closes it")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
