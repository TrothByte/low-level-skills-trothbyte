# BAD: "shared address space" treated as "everything can touch everything".
# No page-table view exists; any component reads any frame, including the
# framework core. This is the isolation-bypass pattern the framekernel's
# MMU model exists to prevent.
# Marker: intentionally incorrect
# Run: python examples/bad/address_space_isolation_bad.py

class SharedSpace:
    def __init__(self):
        self.frames = {"net-rx": b"RX", "net-tx": b"TX",
                       "blk-cache": b"BLOCK", "fw-core": b"PRIVILEGED"}

class Service:
    def __init__(self, name, space):
        self.name, self.space = name, space

def main():
    space = SharedSpace()
    net = Service("net", space)
    block = Service("block", space)

    # intentionally incorrect: services read ANY frame — including the
    # framework core — because no page-table mapping limits them.
    print("net reads:", net.space.frames["blk-cache"])    # foreign!
    print("block reads:", block.space.frames["fw-core"])  # framework core!
    print("BAD: single address space without page-table enforcement lets "
          "services reach foreign frames and the privileged core")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
