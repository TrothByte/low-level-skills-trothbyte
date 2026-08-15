"""
BAD: intentionally incorrect — probe contention from orphaned sessions.
Simulates a flash helper that spawns an "OpenOCD" holder and a "GDB" session
but never tears them down. The next flash attempt on the same host fails with
"couldn't open the device". This is the stm32-gdb-mcp #30/#48 class (~3
incidents/day).

Run: python bad/probe_contention.py
"""
import time

class Probe:
    def __init__(self):
        self.claimed = None

class FakeProcess:
    def __init__(self, name, owner=None):
        self.name = name
        self.owner = owner

class FakeOpenOCD:
    def __init__(self, probe):
        self.probe = probe

    def start(self):
        self.probe.claimed = self
        return FakeProcess("openocd")

    def shutdown(self):
        if self.probe.claimed is self:
            self.probe.claimed = None

class FakeGDB:
    def __init__(self, openocd):
        self.openocd = openocd

    def attach(self):
        # keeps the probe busy even after "flashing"
        return self.openocd

    def detach(self):
        pass


def flash_once(probe):
    """BUG: starts a session and never tears it down."""
    ocd = FakeOpenOCD(probe)
    ocd.start()                    # claim the probe
    gdb = FakeGDB(ocd)
    gdb.attach()
    print("flash reported success")
    # no shutdown(), no detach(), no lock cleanup — probe stays claimed


def main():
    probe = Probe()
    flash_once(probe)              # first run "succeeds"
    flash_once(probe)              # second run: probe still claimed
    print("second flash: probe held by first session -> contention")


if __name__ == "__main__":
    main()
