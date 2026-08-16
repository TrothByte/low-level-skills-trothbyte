# BAD: edge/level confusion — a level-triggered IRQ is handled as edge and
# the device is never deasserted, so the "interrupt" re-fires endlessly
# (the stuck-IRQ storm). Also handles SPI numbering off-by-32.
# intentionally incorrect
class Device:
    def __init__(self):
        self.pending = True     # level-triggered: stays asserted

    def clear(self):
        self.pending = False    # GOOD device would do this; BAD handler never calls it


class BadHandler:
    def __init__(self):
        self.calls = 0

    def handle(self, dev):
        # BAD: edge-style handler — no device deassert, then "EOI".
        self.calls += 1
        # BAD: no dev.clear() — level stays asserted, interrupt re-fires.
        return "EOI"            # BAD: EOI without deassert = storm


def main():
    dev = Device()
    h = BadHandler()
    storms = 0
    # BAD: SPI number 5 used directly (5 is an SGI, not an SPI).
    spi = 5

    while dev.pending and storms < 5:
        h.handle(dev)           # BAD: handler never clears dev.pending
        storms += 1
    print(f"interrupt storms={storms} (device still pending={dev.pending})")
    print(f"BAD: spi={spi} is an SGI, not an SPI (should be 32+)")
    print(f"handler calls={h.calls}")


if __name__ == "__main__":
    main()
