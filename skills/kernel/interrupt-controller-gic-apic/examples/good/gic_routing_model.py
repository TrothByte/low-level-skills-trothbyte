# GOOD: GICv3-style routing model — an SPI interrupt is routed to a target
# CPU via the distributor, and the handler EOIs correctly. Run:
#   python3 gic_routing_model.py
class GicDistributor:
    """Models GICv3 distributor routing for SPIs (32..1019)."""

    def __init__(self):
        self.routes = {}      # spi -> target cpu
        self.masked = {}      # spi -> bool

    def configure(self, spi, target_cpu, masked=False):
        # GOOD: SPI numbers are 32..1019 (add 32 to a device IRQ line).
        if not (32 <= spi <= 1019):
            raise ValueError(f"SPI out of range: {spi}")
        self.routes[spi] = target_cpu
        self.masked[spi] = masked

    def deliver(self, spi, cpu):
        if self.masked.get(spi, True):
            return False
        if self.routes.get(spi) != cpu:
            return False
        return True


class Handler:
    def __init__(self):
        self.calls = 0

    def handle(self):
        # GOOD: service then EOI (priority drop + deactivate).
        self.calls += 1
        return True  # EOI sent


def main():
    gic = GicDistributor()
    h = Handler()
    # Device IRQ line 5 on the board -> SPI 32 + 5 = 37, routed to CPU 0.
    gic.configure(spi=37, target_cpu=0)

    print("CPU0 gets SPI37:", gic.deliver(37, 0))   # True
    print("CPU1 gets SPI37:", gic.deliver(37, 1))   # False (not routed)
    if gic.deliver(37, 0):
        h.handle()
    print("handler calls:", h.calls)                 # 1


if __name__ == "__main__":
    main()
