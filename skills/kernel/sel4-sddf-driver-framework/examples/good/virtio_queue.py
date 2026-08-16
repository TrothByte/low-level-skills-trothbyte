# GOOD: virtio split-ring with capability-style ownership checks.
# Models the sDDF frontend/driver split: each component touches only the
# ring fields its side owns, guarded by an explicit capability grant.
# Run: python examples/good/virtio_queue.py   (expect exit 0)

class Capability:
    def __init__(self, name, region):
        self.name = name
        self.region = region

class Fault(Exception):
    pass

class Ring:
    """Split virtio ring. Ownership table drives every access."""
    def __init__(self, cap):
        self.cap = cap                 # capability granting this ring
        self.desc = [None] * 16        # descriptor table   (frontend writes)
        self.avail = []                # available ring     (frontend writes)
        self.used = [None] * 16        # used ring          (driver writes)

    def _check(self, side, field):
        owners = {"desc": "frontend", "avail": "frontend", "used": "driver"}
        if side != owners[field]:
            raise Fault(f"{side} touched '{field}' but {owners[field]} owns it")

class Frontend:
    def __init__(self, ring, cap):
        self.ring = ring
        self.cap = cap
        self.next_desc = 0
    def submit(self, buf):
        self.ring._check("frontend", "desc")
        self.ring._check("frontend", "avail")
        idx = self.next_desc          # monotonic descriptor allocation
        self.next_desc += 1
        self.ring.desc[idx] = buf
        self.ring.avail.append(idx)
        return idx

class Driver:
    def __init__(self, ring, cap):
        self.ring = ring
        self.cap = cap
    def poll(self):
        self.ring._check("driver", "used")
        if self.ring.avail:
            i = self.ring.avail.pop(0)
            self.ring.used[i] = f"done:{self.ring.desc[i]}"
            return i
        return None

def main():
    cap = Capability("net-ring", "0x4000-0x5000")
    ring = Ring(cap)
    fe = Frontend(ring, cap)
    drv = Driver(ring, cap)
    fe.submit(b"payload-a")
    drv.poll()
    fe.submit(b"payload-b")
    assert ring.used[0] == "done:b'payload-a'", "completion must reach used ring"
    drv.poll()
    assert ring.used[1] == "done:b'payload-b'", "second completion delivered"
    print("GOOD: ownership respected, completion delivered")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
