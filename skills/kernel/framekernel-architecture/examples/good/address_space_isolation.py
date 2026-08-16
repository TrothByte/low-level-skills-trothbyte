# GOOD: MMU-based service isolation inside a single address space.
# Each service has a page-table view (its own frames + granted shared
# regions). A foreign address lookup faults — isolation is enforced by the
# map, not by politeness.
# Run: python examples/good/address_space_isolation.py   (expect exit 0)

class Fault(Exception):
    pass

class PageTable:
    def __init__(self, frames):
        self.map = set(frames)          # frames this service may access
    def translate(self, frame):
        if frame not in self.map:
            raise Fault(f"page fault: frame {frame} not mapped for this service")
        return frame

class Service:
    def __init__(self, name, page_table):
        self.name, self.pt = name, page_table

def main():
    # one address space; each service gets its own mapping view
    net = Service("net", PageTable(frames=["net-rx", "net-tx", "shared-mbox"]))
    block = Service("block", PageTable(frames=["blk-cache", "shared-mbox"]))
    fw = Service("framework", PageTable(frames={"net-rx", "net-tx", "blk-cache",
                                                "shared-mbox", "fw-core"}))

    # normal traffic: services touch their own frames + the shared mailbox
    net.pt.translate("net-rx")
    block.pt.translate("blk-cache")
    assert net.pt.translate("shared-mbox") == "shared-mbox"

    # isolation: net must NOT reach blk-cache, block must NOT reach net-rx,
    # and neither may reach fw-core
    for svc, foreign in [(net, "blk-cache"), (block, "net-rx"),
                         (net, "fw-core"), (block, "fw-core")]:
        try:
            svc.pt.translate(foreign)
            raise AssertionError(f"{svc.name} reached foreign frame {foreign}")
        except Fault as e:
            print(f"isolated: {svc.name} faulted on {foreign} ({e})")

    print("GOOD: page-table enforcement contains service bugs; framework "
          "core unreachable from services")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
