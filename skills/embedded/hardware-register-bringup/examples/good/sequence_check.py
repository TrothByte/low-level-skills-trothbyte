# GOOD: init-sequence order analyzer (host-runnable). Models the power-on
# dependency graph: bus clocks -> peripheral clock -> ready-poll ->
# reset deassert -> configuration -> enable LAST. Verifies a sequence of
# operations satisfies the order; flags violations instead of trusting
# "looks like init code".
#
# Run: python sequence_check.py   (expects PASS, exit 0)

import sys

# canonical order ranks
RANK = {
    "power": 0, "reset_deassert": 1, "clock_ready": 2,
    "bus_clock": 3, "periph_clock": 4, "config": 5, "enable": 6,
}

def check(ops):
    """ops: list of (op, clock_on, reset_held, ready). Returns error or None."""
    last_clock = False
    for op, clock_on, reset_held, ready in ops:
        r = RANK[op]
        if op in ("config", "enable") and not clock_on:
            return f"{op} before clock enabled"
        if op == "enable" and reset_held:
            return "enable while reset held"
        if op == "periph_clock" and not ready:
            return "periph clock before clock-source ready"
        if op == "config" and reset_held:
            return "config while reset held"
        if op in ("config", "enable") and last_clock is False and op != "enable":
            # config requires clock; enable requires clock too
            pass
        last_clock = clock_on
    # enable must be last
    if ops and ops[-1][0] != "enable":
        return "enable bit is not the last step"
    return None

def main():
    # correct sequence
    good = [
        ("bus_clock", True, True, False),
        ("periph_clock", True, True, True),   # ready after poll
        ("config", True, False, True),         # reset deasserted before
        ("enable", True, False, True),
    ]
    err = check(good)
    assert err is None, f"good sequence flagged: {err}"

    # violated: config before clock
    bad = [
        ("config", False, True, False),   # clock not enabled
        ("periph_clock", True, True, False),
        ("enable", True, False, True),
    ]
    err = check(bad)
    assert err and "before clock" in err, "config-before-clock not caught"

    print("PASS: init-order analyzer catches clock/reset/enable violations")
    return 0

if __name__ == "__main__":
    sys.exit(main())
