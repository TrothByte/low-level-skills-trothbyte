# BAD: passthrough/bypass — the device's DMA goes straight to physical
# memory with no translation, so it can write anywhere. Presented as "the
# device is trusted, no need for isolation". This is the classic bypass hole.
# intentionally incorrect
class PassthroughDevice:
    """BAD: bypass mode — every DMA address is treated as physical."""

    def __init__(self, name):
        self.name = name

    def dma_write(self, physical_addr, data):
        # BAD: no IOVA domain, no translation, no fault — writes anywhere.
        print(f"WROTE {data} to physical {hex(physical_addr)} (no isolation)")

    def dma_read(self, physical_addr):
        print(f"READ physical {hex(physical_addr)} (no isolation)")
        return 0xDEAD


def main():
    dev = PassthroughDevice("untrusted-gpu")
    print("attached in passthrough mode (bypass)")

    # BAD: the device can read/write ANY physical address, including host
    # kernel memory, because there is no stage-1/stage-2 domain.
    dev.dma_read(0x100000)   # e.g. host kernel text
    dev.dma_write(0x200000, b"pwn")
    print("bypass hole demonstrated: no fault, no boundary")


if __name__ == "__main__":
    main()
