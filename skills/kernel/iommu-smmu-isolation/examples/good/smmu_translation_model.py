# GOOD: SMMUv3-style model — StreamID lookup, translation with an IOVA domain,
# device-TLB invalidation after a map, and a fault when the address is not
# mapped. Run: python3 smmu_translation_model.py
class SMMU:
    """Minimal SMMUv3-style stream-table + stage-1 translation model."""

    def __init__(self):
        self.streams = {}      # StreamID -> {iova->pa} (device page table)
        self.tlbi_count = 0    # device-TLB invalidations issued

    def attach(self, stream_id, initial_maps):
        self.streams[stream_id] = dict(initial_maps)

    def map_page(self, stream_id, iova, pa):
        # GOOD: after the PTE change, issue a device-TLB invalidation.
        self.streams[stream_id][iova] = pa
        self.tlbi_count += 1   # TLBI_EL2 / CMDQ_TLBI — NOT a CPU sfence.vma

    def translate(self, stream_id, iova):
        pt = self.streams.get(stream_id)
        if pt is None or iova not in pt:
            return None        # translation fault: abort the transaction
        return pt[iova]


def main():
    smmu = SMMU()
    smmu.attach(stream_id=0x4A1, initial_maps={})   # StreamID, not BDF
    smmu.map_page(0x4A1, iova=0x1000, pa=0x8000)
    print("DMA to 0x1000 ->", hex(smmu.translate(0x4A1, 0x1000)))  # 0x8000
    print("DMA to 0x2000 ->", smmu.translate(0x4A1, 0x2000))       # fault
    print("TLBIs issued:", smmu.tlbi_count)                        # 1


if __name__ == "__main__":
    main()
