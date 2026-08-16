# seL4 / SDDF: Virtio Ring Protocols Between Frontend and Driver

## 1. sDDF drivers speak virtio-style rings, not Linux driver APIs

- **RULE**: the sDDF defines device-class protocols (network, block, serial,
  I2C, audio) where the driver component is a backend serving a frontend
  component through shared-memory ring buffers in the virtio style. The
  network class is the most mature; block/serial are under active
  development. KNOWN (sDDF README/design).
- **WHY AI GETS IT WRONG**: agents port Linux `struct net_device` /
  `blk_mq` logic directly; those APIs assume a monolithic kernel with global
  memory, and do not express the ownership rules of a shared ring.
- **CORRECT REASONING**: identify which side owns which part of the ring
  (the split-virtio model: descriptor table, available ring, used ring) and
  who may touch which field. The protocol is the contract; the device is
  hidden behind it.
- **EXAMPLE** (bad): the frontend writes directly into the driver's
  descriptor table to "speed up" I/O — corrupts the ring and crosses the
  isolation boundary.
- **COUNTEREXAMPLE** (good): the frontend writes only its available ring and
  descriptor table entries; the driver consumes them and returns completion
  via the used ring; each component only touches its own region.
- **VERIFICATION**: `examples/good/virtio_queue.py` simulates the ownership
  check and passes; `bad/virtio_queue_unsafe.py` violates it and fails.
- **SOURCE**: sddf (design doc: ring protocols, device classes) [proposed].

## 2. Ring ownership is a two-party capability contract

- **RULE**: for each ring field, exactly one side may write it, and that side
  holds the capability for the region it writes. A component that touches a
  field it does not own is violating the isolation the framework exists to
  provide. KNOWN (sDDF design).
- **WHY AI GETS IT WRONG**: shared memory reads as "everyone can touch it";
  the agent misses that "shared" means "shared by grant, split by role".
- **CORRECT REASONING**: draw the field table (field, writer, reader,
  region-capability owner). If a write does not belong to the writer column,
  the code is broken even if it runs in the stub.
- **EXAMPLE** (bad): `bad/virtio_queue_unsafe.py` — a `Queue` object that
  lets any component read/write any field; the frontend overwrites a
  descriptor the driver is using.
- **COUNTEREXAMPLE** (good): `good/virtio_queue.py` — `AvailabilityRing` and
  `UsedRing` expose only the fields each side owns; an access to a
  non-owned field raises a fault.
- **VERIFICATION**: `python examples/good/virtio_queue.py` (exit 0);
  `python examples/bad/virtio_queue_unsafe.py` (prints the isolation
  violation).
- **SOURCE**: sddf (design doc) [proposed].

## 3. DMA buffers must be explicit frame capabilities, not raw addresses

- **RULE**: memory that the device writes must be mapped for DMA and passed
  to the device as a capability-derived physical frame. A buffer at a "nice"
  address the driver can see is not necessarily reachable by the device.
  INFERRED (from seL4 memory-management rules + sDDF design).
- **WHY AI GETS IT WRONG**: on Linux, `dma_alloc_coherent` hides the whole
  problem; agents keep that habit and pass arbitrary buffer addresses.
- **CORRECT REASONING**: enumerate: which component maps which frame, who
  translates it to a device-visible address, and who recycles it after
  completion. If a step is implicit, it is missing.
- **EXAMPLE** (bad): driver passes a pointer into its own static buffer to
  the device; the device writes into memory no component mapped for it.
- **COUNTEREXAMPLE** (good): the driver allocates a frame capability, maps it
  into the shared region, and hands the device the frame's physical address;
  completion returns ownership to the driver.
- **VERIFICATION**: ring simulation with an explicit `Frame` capability
  object (see virtio_queue.py); target DMA tests documented in evals.
- **SOURCE**: sddf (design doc); sel4-docs (memory management) [proposed].
