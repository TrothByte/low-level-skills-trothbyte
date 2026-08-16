# BAD: the frontend can write the driver-owned 'used' ring, and the driver
# can rewrite 'desc' the frontend published. No capability check: the shared
# structure is treated as one flat object both sides may touch.
# This is the isolation-violation pattern the sDDF exists to prevent.
# Run: python examples/bad/virtio_queue_unsafe.py
# Marker: intentionally incorrect

class SharedRing:
    def __init__(self):
        self.desc = [None] * 16
        self.avail = []
        self.used = [None] * 16

class Frontend:
    def __init__(self, ring):
        self.ring = ring
    def submit(self, buf):
        idx = len(self.ring.avail)
        self.ring.desc[idx] = buf
        self.ring.avail.append(idx)
        return idx
    def tamper_used(self, idx, value):
        self.ring.used[idx] = value      # intentionally incorrect: frontend
        return value                     # writes the driver's completion ring

class Driver:
    def __init__(self, ring):
        self.ring = ring
    def poll(self):
        if self.ring.avail:
            i = self.ring.avail.pop(0)
            self.ring.used[i] = f"done:{self.ring.desc[i]}"
            return i
        return None
    def clobber_desc(self, idx):
        self.ring.desc[idx] = b"corrupted"  # intentionally incorrect: driver
        return idx                          # rewrites a frontend-owned entry

def main():
    ring = SharedRing()
    fe = Frontend(ring)
    drv = Driver(ring)
    fe.submit(b"request")
    drv.poll()
    fe.tamper_used(0, "forged-completion")   # silently accepted
    drv.clobber_desc(0)                       # silently accepted
    print("BAD: isolation violated silently — frontend forged driver data, "
          "driver destroyed a live descriptor, nothing faulted")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
