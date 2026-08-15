"""
GOOD: single-probe discipline with clean teardown (host-runnable model).
Exactly one OpenOCD and one GDB per run; teardown is detach GDB then shutdown
OpenOCD, always executed (finally). The second flash on the same probe
connects cleanly. This is the required shape for any flash tool or MCP server.

Run: python good/session_discipline.py
"""
import time


class Probe:
    def __init__(self):
        self.claimed = None


class FakeOpenOCD:
    def __init__(self, probe):
        self.probe = probe

    def start(self):
        if self.probe.claimed is not None:
            raise RuntimeError("couldn't open the device (still claimed)")
        self.probe.claimed = self
        return self

    def shutdown(self):
        if self.probe.claimed is self:
            self.probe.claimed = None


class FakeGDB:
    def __init__(self, ocd):
        self.ocd = ocd
        self.attached = False

    def attach(self):
        self.attached = True

    def detach(self):
        self.attached = False


def flash_once(probe):
    ocd = FakeOpenOCD(probe)
    gdb = None
    try:
        ocd.start()
        gdb = FakeGDB(ocd)
        gdb.attach()
        print("flash reported success")
    finally:
        if gdb is not None:
            gdb.detach()          # 1. GDB detaches first
        ocd.shutdown()            # 2. OpenOCD releases the probe


def main():
    probe = Probe()
    flash_once(probe)
    assert probe.claimed is None, "probe must be free after teardown"
    flash_once(probe)             # second run connects cleanly
    print("second flash: probe free -> clean connect")
    print("PASS: single-probe discipline + teardown")


if __name__ == "__main__":
    main()
