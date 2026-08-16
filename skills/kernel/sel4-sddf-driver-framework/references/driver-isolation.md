# seL4 / SDDF: Driver Isolation and System Description

## 1. Isolation is enforced by the system description, not by driver discipline

- **RULE**: in a Microkit/sDDF system, isolation comes from the static system
  description (which protection domain owns which region, channel, IRQ) that
  generates the boot image. Runtime "allocation" of shared regions does not
  exist in the base model. KNOWN (Microkit manual).
- **WHY AI GETS IT WRONG**: agents trained on dynamic OSes write code that
  "shares a buffer at runtime" and never write the config that grants it.
  The code then looks right and cannot run.
- **CORRECT REASONING**: the description is the authority. If a region or
  channel is absent from it, the corresponding code is dead or faulting.
- **EXAMPLE** (bad): two components use a shared `struct shm` pointer with no
  memory region for it in the system description — fault at first touch.
- **COUNTEREXAMPLE** (good): the description declares the shared memory
  region and the channel; both components map only that region.
- **VERIFICATION**: the Microkit build fails or the component faults without
  the description entry; documented, not run here.
- **SOURCE**: microkit-docs (system description, manual) [proposed].

## 2. "It compiles in the stub" is not "it is configured in the system"

- **RULE**: a compilable driver stub proves only syntax. Whether the driver
  is actually isolated, or even booted, is a property of the description and
  the run. Citing a stub as verification of an isolation property is the
  harness-validity failure (A-blame meta-verification-harness-validity).
- **WHY AI GETS IT WRONG**: the agent validates the stub on the host,
  records exit 0, and claims the driver works — the exact "harness ran ⇒
  target correct" fallacy.
- **CORRECT REASONING**: state the property ("frontend cannot write driver
  descriptors"), then name the artifact that enforces it (the system
  description + MMU). Verify that artifact, not the stub.
- **EXAMPLE** (bad): `bad/seL4_driver_stub_bad.c` compiles cleanly but skips
  IRQ binding and shares a global device pointer.
- **COUNTEREXAMPLE** (good): `good/seL4_driver_stub.c` mirrors the real call
  sequence including capability-error checks.
- **VERIFICATION**: `gcc -Wall -Wextra -Werror -O2 -c` both; then the
  property check is done on the description, not the C (documented).
- **SOURCE**: microkit-docs; meta-verification-harness-validity
  (arxiv-2606-20128; arxiv-2607-00107).

## 3. Driver classes come with prescribed topologies

- **RULE**: sDDF covers network, block, serial, I2C, audio; the network class
  has a mature frontend/backend split, the others are experimental. An agent
  writing an sDDF component must first identify which class protocol it is
  implementing. KNOWN (sDDF README).
- **WHY AI GETS IT WRONG**: without a class taxonomy, agents invent their own
  ring formats that no other component speaks.
- **CORRECT REASONING**: the protocol is fixed by the framework; your job is
  to implement one side of it faithfully, plus the device-specific register
  work.
- **EXAMPLE** (bad): a network frontend that exchanges data over a raw
  message channel instead of the ring protocol.
- **COUNTEREXAMPLE** (good): the frontend uses the sDDF network interface:
  split ring, descriptor/available/used regions with the documented field
  ownership.
- **VERIFICATION**: inspect against the class design doc; build + boot the
  example system (documented).
- **SOURCE**: sddf (README: device classes; design doc) [proposed].
