# BAD: # intentionally incorrect — the clock is enabled but the peripheral
# reset bit is never deasserted. A peripheral held in reset ignores all
# register writes: "I enabled the clock and wrote config, why is it dead?"
# The reset-deassert step is mandatory and must follow clock enable.
#
# Run: python sequence_missing_reset.py   (expects non-zero exit)

import sys

def bringup(sequence):
    clock_enabled = False
    reset_released = False
    for step in sequence:
        if step == "enable_clock":
            clock_enabled = True
        elif step == "write_config":
            if not clock_enabled:
                return "config before clock"
            # // intentionally incorrect: no check that reset is released
            if not reset_released:
                return "config while reset still held (silently ignored on silicon)"
        elif step == "enable_periph":
            pass
    return None  # claims success

def main():
    # // intentionally incorrect: no "deassert_reset" step anywhere
    seq = ["enable_clock", "write_config", "enable_periph"]
    err = bringup(seq)
    if err:
        print("BUG: " + err)
        return 1
    print("PASS")
    return 0

if __name__ == "__main__":
    sys.exit(main())
