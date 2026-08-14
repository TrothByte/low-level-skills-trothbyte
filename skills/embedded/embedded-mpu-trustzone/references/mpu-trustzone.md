# ARMv7-M MPU and ARMv8-M TrustZone

Two independent mechanisms live behind the same "memory protection" label:

1. The **MPU** controls access *permissions* (read/write/execute, privileged or
   unprivileged) within the current security state. ARMv7-M programs it with
   RBAR/RASR; ARMv8-M programs it with RBAR/RLAR plus MAIR.
2. The **SAU/IDAU** (ARMv8-M TrustZone only) controls which *security state*
   owns each address: Secure (S), Non-secure (NS), or Non-secure-callable (NSC).

Register bit positions below are KNOWN and were verified against CMSIS-5
headers (`core_cm4.h`, `core_cm33.h`, `mpu_armv7.h`, `mpu_armv8.h`, V5.2/V5.9).
Where behavior is architecture-level, the ARMv8-M ARM (DDI 0553A) or ARMv7-M
ARM (DDI 0403) is cited.

---

## Part 1 — ARMv7-M MPU (Cortex-M3/M4/M7)

### 1.1 Region size and alignment

- **RULE**: An ARMv7-M MPU region is a power-of-two-sized block of at least
  32 bytes. The base address must be aligned to the region size. The RASR SIZE
  field holds N where size = 2^(N+1) bytes (32 B -> 0x04, 64 B -> 0x05,
  1 KB -> 0x09, 4 GB -> 0x1F).
- **WHY AI GETS IT WRONG**: agents "define a region from address X to address
  Y" as if regions were arbitrary ranges, and pick sizes like 0x50000 or bases
  like 0x20008000 that are not aligned to the size.
- **CORRECT REASONING**: the hardware does not support arbitrary ranges. Split
  any non-power-of-two range into power-of-two aligned regions. If base is not
  aligned to size, behavior is UNPREDICTABLE and the region can silently cover
  a different address range.
- **EXAMPLE** (bad):
  ```c
  MPU->RNR = 0U;
  MPU->RBAR = 0x20008000U;               /* want 64 KB @ 0x20008000 */
  MPU->RASR = (7U << MPU_RASR_SIZE_Pos) | MPU_RASR_ENABLE_Msk; /* 64 KB, misaligned */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  if ((base & (size - 1U)) == 0U && (size & (size - 1U)) == 0U) { /* 0x20000000, 64 KB */
      MPU->RBAR = base;
      MPU->RASR = (size_field << MPU_RASR_SIZE_Pos) | MPU_RASR_ENABLE_Msk;
  }
  ```
- **VERIFICATION**: host helper `region_ok(base, size)`; assert on every
  descriptor before writing it; read back RASR on target.
- **SOURCE**: cmsis (mpu_armv7.h size/AP tables); arm-arm (DDI 0403 MPU chapter).

### 1.2 A region is active only if it is enabled AND the MPU is enabled

- **RULE**: A region participates in access checking only if RASR.ENABLE=1
  (and RBAR.VALID=1 when the region number is set in RBAR) AND
  MPU_CTRL.ENABLE=1. If the MPU is disabled, all regions are ignored and the
  default system memory map applies with no protection.
- **WHY AI GETS IT WRONG**: "I wrote the region registers, so the MPU protects"
  — agents forget either the per-region ENABLE bit or the global MPU ENABLE.
- **CORRECT REASONING**: the sequence is: pick region (RNR, or RBAR.VALID),
  write RBAR, write RASR with ENABLE=1, then set MPU_CTRL.ENABLE (and the
  PRIVDEFENA policy), then DSB + ISB. Only then is any protection active.
- **EXAMPLE** (bad):
  ```c
  MPU->CTRL = MPU_CTRL_ENABLE_Msk;   /* MPU on */
  MPU->RNR  = 0U;
  MPU->RBAR = 0x00000000U;
  MPU->RASR = (0x0BU << MPU_RASR_SIZE_Pos);   /* ENABLE bit missing -> region dead */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  MPU->CTRL = MPU_CTRL_ENABLE_Msk | MPU_CTRL_PRIVDEFENA_Msk;
  MPU->RNR  = 0U;
  MPU->RBAR = 0x00000000U;
  MPU->RASR = (0x0BU << MPU_RASR_SIZE_Pos) | MPU_RASR_ENABLE_Msk;
  __DSB(); __ISB();
  ```
- **VERIFICATION**: read back RASR.ENABLE and MPU_CTRL.ENABLE after setup; on
  target, provoke an access and check for MemManage fault behavior.
- **SOURCE**: cmsis (mpu_armv7.h ARM_MPU_Enable/ARM_MPU_SetRegion); arm-arm (DDI 0403).

### 1.3 The background region (PRIVDEFENA)

- **RULE**: MPU_CTRL.PRIVDEFENA=1 lets *privileged* accesses to addresses that
  match no enabled region use the default system memory map. With PRIVDEFENA=0
  (or for *unprivileged* accesses, regardless), any access to an unmapped
  address raises a MemManage fault.
- **WHY AI GETS IT WRONG**: "PRIVDEFENA gives everyone the background region"
  or "the background region is always present".
- **CORRECT REASONING**: PRIVDEFENA is a privilege-scoped default, not a safety
  net for unprivileged code. Decide it explicitly: set it when privileged
  firmware intentionally touches unmapped peripherals via the default map;
  clear it when every address must be covered by an explicit region.
- **EXAMPLE** (bad): MPU enabled with PRIVDEFENA=0, then a privileged driver
  reads a peripheral that was never given a region -> unexpected MemManage
  fault at the first access.
- **COUNTEREXAMPLE** (good): `MPU->CTRL = MPU_CTRL_ENABLE_Msk | MPU_CTRL_PRIVDEFENA_Msk;`
  with a comment stating why privileged accesses may fall through to the
  default map.
- **VERIFICATION**: assert CTRL.PRIVDEFENA matches the intended policy in the
  host model; provoke both a privileged and an unprivileged unmapped access on
  target.
- **SOURCE**: cmsis (core_cm4.h MPU_CTRL_PRIVDEFENA_Pos); arm-arm (DDI 0403).

### 1.4 Privilege and the AP field

- **RULE**: RASR AP[26:24] encodes access permissions: 3 = full access (P+U
  read/write), 1 = privileged read/write only, 2 = P read/write + U read-only,
  5 = privileged read-only, 6 = read-only, 0 = no access. Unprivileged
  (Thread-mode, CONTROL.nPRIV=1) code can only access regions whose AP permits
  unprivileged access.
- **WHY AI GETS IT WRONG**: "all my code runs privileged, so AP does not
  matter" — true only until an RTOS task or a user callback drops to
  unprivileged, which then faults on every P-only region it touches.
- **CORRECT REASONING**: choose AP from the *callee's* privilege, not the
  caller's. Data shared with unprivileged tasks needs AP >= 2 (or 3).
- **EXAMPLE** (bad): a buffer passed to an unprivileged task sits in a region
  with AP=1; the task faults on its first store.
- **COUNTEREXAMPLE** (good): the shared-buffer region uses AP=3 (full access),
  while the privileged-only control region keeps AP=1.
- **VERIFICATION**: assert the intended AP per region in the host model; on
  target, run a one-line unprivileged access test.
- **SOURCE**: cmsis (mpu_armv7.h ARM_MPU_AP_* values); arm-arm (DDI 0403).

### 1.5 Memory attributes: TEX/S/C/B in RASR — there is NO MAIR on ARMv7-M

- **RULE**: ARMv7-M encodes memory attributes inline per region in RASR:
  TEX[21:19], S[18], C[17], B[16]. MAIR0/MAIR1 exist only on ARMv8-M (and on
  A-profile). Writing "MAIR" on an ARMv7-M target programs a register that
  does not exist.
- **WHY AI GETS IT WRONG**: ARMv8/A-profile habits (MAIR, shareability
  domains) leak into ARMv7-M code. Conversely, ARMv7-M SRD/SIZE habits leak
  into ARMv8-M RLAR code.
- **CORRECT REASONING**: for ARMv7-M, build attributes with
  `ARM_MPU_ACCESS_(TEX, S, C, B)`. Device memory = TEX 000/010 with C=0;
  normal cacheable = TEX 001/100 with C/B per policy. The default map
  attributes apply only where PRIVDEFENA permits.
- **EXAMPLE** (bad): `MPU->MAIR0 = ...` in an ARMv7-M build (no such register;
  on target this writes into an unimplemented alias).
- **COUNTEREXAMPLE** (good): `ARM_MPU_RASR(0, ARM_MPU_AP_FULL,
  ARM_MPU_ACCESS_DEVICE(0), 0, ARM_MPU_REGION_SIZE_4KB)` — attributes encoded
  in the RASR value.
- **VERIFICATION**: compile the file against the actual core header; on ARMv7-M
  a MAIR write will not appear in the disassembly.
- **SOURCE**: cmsis (mpu_armv7.h ARM_MPU_ACCESS_*, mpu_armv8.h ARM_MPU_ATTR_*); arm-arm.

---

## Part 2 — ARMv8-M MPU (Cortex-M23/M33/M55)

### 2.1 Region descriptors: RBAR/RLAR + MAIR

- **RULE**: ARMv8-M programs regions with RBAR + RLAR (no RASR). RBAR:
  BASE[31:5], SH[4:3], AP[2:1], XN[0]. RLAR: LIMIT[31:5], AttrIndx[4:1],
  EN[0]. Memory attributes live in MAIR0/MAIR1 (eight 8-bit slots, indices
  0-7); RLAR.AttrIndx selects the slot.
- **WHY AI GETS IT WRONG**: agents copy ARMv7-M RBAR/RASR habits into ARMv8-M
  code, or try to put attribute bits into RBAR instead of MAIR.
- **CORRECT REASONING**: attribute encodings (CMSIS mpu_armv8.h): 0b00xx =
  Device (0 = nGnRnE, 1 = nGnRE, 2 = nGRE, 3 = GRE), 0b0100 = Normal
  non-cacheable, 0b1xxx = Normal with cache policies. Program MAIR first, then
  reference slots via AttrIndx in each RLAR.
- **EXAMPLE** (bad):
  ```c
  MPU->RBAR = base;              /* attributes wrongly placed in the base */
  MPU->RLAR = limit | 1U;        /* no attribute index, slot never defined */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  ARM_MPU_SetMemAttr(0U, ARM_MPU_ATTR_DEVICE_nGnRnE);       /* MAIR slot 0 */
  MPU->RNR  = 0U;
  MPU->RBAR = ARM_MPU_RBAR(base, ARM_MPU_SH_NON, 0, 0, 1);  /* RO, NP, XN */
  MPU->RLAR = ARM_MPU_RLAR(base + size - 1U, 0U);           /* slot 0, EN */
  ```
- **VERIFICATION**: assert MAIR slots equal the intended attributes before
  use; objdump the MPU setup and check MAIR writes at offset 0x030/0x034.
- **SOURCE**: cmsis (core_cm33.h MPU_Type and bit positions; mpu_armv8.h
  ARM_MPU_RBAR/ARM_MPU_RLAR/ARM_MPU_SetMemAttr); arm-arm (DDI 0553A).

### 2.2 Alignment and the limit address (ARMv8-M)

- **RULE**: same power-of-two rule as ARMv7-M, minimum granularity 32 bytes
  (BASE/LIMIT fields are [31:5]). The limit address is the last byte:
  LIMIT = base + size - 1. RLAR ones-extends the low five bits, so a region
  always ends at an address whose low bits are 0x1F.
- **WHY AI GETS IT WRONG**: agents compute LIMIT as base + size (exclusive end)
  or use a non-power-of-two size; the region then covers the wrong range.
- **CORRECT REASONING**: validate base-aligned-to-size and power-of-two size;
  compute the limit from the last byte. CMSIS `ARM_MPU_RLAR(Limit, Idx)` does
  exactly this.
- **EXAMPLE** (bad): `MPU->RLAR = base + size;` (exclusive limit, off by one
  region length in the encoded field).
- **COUNTEREXAMPLE** (good): `MPU->RLAR = ARM_MPU_RLAR(base + size - 1U, idx);`
- **VERIFICATION**: host assert `region_ok(base, size)` plus
  `((limit & 0x1FU) == 0x1FU)`.
- **SOURCE**: cmsis (mpu_armv8.h ARM_MPU_RBAR/ARM_MPU_RLAR macros); arm-arm.

### 2.3 There are two MPUs on a TrustZone device

- **RULE**: ARMv8-M has a Secure MPU and a Non-secure MPU (MPU_NS). Secure
  software programs both; non-secure software only ever sees the Non-secure
  MPU. The MPU never changes security attribution — SAU/IDAU does.
- **WHY AI GETS IT WRONG**: "one MPU protects the whole device" or "making the
  NS region restrictive protects secure memory".
- **CORRECT REASONING**: MPU = permissions inside a state; SAU/IDAU = which
  state owns the address. A non-secure MPU region cannot make memory Secure.
- **EXAMPLE** (bad): the secure image configures only `MPU_NS` and skips the
  Secure MPU; unprivileged Secure tasks then run with no protection while the
  designer believes "the MPU is on".
- **COUNTEREXAMPLE** (good): the secure boot image programs both MPUs
  (CMSIS `ARM_MPU_*_NS` aliases for the Non-secure one) and enables each with
  its own policy.
- **VERIFICATION**: use the MPU_NS aliases (CMSIS MPU_NS macros) in the secure
  boot code; verify both CTRL registers on target.
- **SOURCE**: cmsis (mpu_armv8.h ARM_MPU_*_NS functions); arm-arm (DDI 0553A).

---

## Part 3 — ARMv8-M TrustZone

### 3.1 Security states and attribution (SAU/IDAU)

- **RULE**: the processor has Secure (S) and Non-secure (NS) states. Every
  address is attributed S, NS, or NSC by the combination of the IDAU (fixed in
  silicon) and the SAU (software). The SAU default is **Secure**: with the SAU
  disabled or with an address matched by no enabled region, the SAU attributes
  the address Secure. An enabled SAU region marks its range Non-secure (NSC=0)
  or Non-secure-callable (NSC=1). SAU_CTRL.ALLNS=1 forces every address
  Non-secure. Secure state may access NS memory; NS state may not access S
  memory (SecureFault).
- **WHY AI GETS IT WRONG**: "the SAU is optional", "NS code can read the
  secure flash it is mapped next to", "unconfigured memory defaults to
  Non-secure".
- **CORRECT REASONING**: enumerate the final attribution of every address the
  firmware uses (both images) before writing the first line of SAU code. A
  single mis-attributed address means a SecureFault at the first NS access.
- **EXAMPLE** (bad): SAU left disabled at boot; the NS application's flash and
  SRAM stay Secure and its reset handler faults on the first instruction fetch.
- **COUNTEREXAMPLE** (good): secure bootloader configures SAU regions for NS
  flash, NS SRAM, and the NSC veneers before branching to the NS image.
- **VERIFICATION**: assert SAU_CTRL.ENABLE and region contents; on target,
  read SAU_SFSR/SFAR after NS boot and expect them clean.
- **SOURCE**: cmsis (core_cm33.h SAU_* bit definitions); arm-arm (DDI 0553A).

### 3.2 SAU configuration rules

- **RULE**: the SAU is a Secure-only peripheral. Configure it in Secure state
  before the first transition to NS. Regions are base/limit pairs with 32-byte
  granularity (BADDR/LADDR are [31:5]); SAU_RLAR has ENABLE[0] and NSC[1] only
  — there is no S bit, because a region can only make its range NS or NSC and
  everything else defaults to Secure. Order: disable SAU (CTRL=0), program all
  regions, enable SAU.
- **WHY AI GETS IT WRONG**: agents invent an "S bit", or configure the SAU from
  NS code (writes are ignored — the registers are RAZ/WI there), or enable the
  SAU before programming regions (transient unconfigured state).
- **CORRECT REASONING**: treat SAU regions as a "carve-out to Non-secure"
  list. When NS code must call secure services, include at least one NSC
  region; verify it is present before enabling the SAU.
- **EXAMPLE** (bad): `SAU->CTRL = SAU_CTRL_ENABLE_Msk;` with no regions
  programmed, then a branch to the NS image.
- **COUNTEREXAMPLE** (good): disable SAU, program RNR/RBAR/RLAR for each
  region (NSC marked with SAU_RLAR_NSC_Msk), then set SAU_CTRL.ENABLE.
- **VERIFICATION**: host asserts on SAU_CTRL.ENABLE and on the presence of an
  NSC region (RNR=0 read-back); target check that NS firmware runs without
  SecureFault.
- **SOURCE**: cmsis (core_cm33.h SAU_Type, SAU_RLAR_NSC_Pos); arm-arm (DDI 0553A).

### 3.3 NSC regions, the SG instruction, and veneers

- **RULE**: non-secure code enters a secure function by branching (BX) to an
  address inside an NSC region whose first instruction is **SG** (Secure
  Gateway). If the target is NSC but the first instruction is not SG — or the
  target is not inside an NSC region — a fault is taken. The toolchain
  generates the veneer: secure functions marked `cmse_nonsecure_entry` and a
  linker `--cmse-implib` produce the NSC region (`.gnu.sgstubs` section).
- **WHY AI GETS IT WRONG**: "just call the secure function's address directly";
  "the entry point is the secure function itself"; "marking the region
  Non-secure is enough" (it is not — it must be NSC, and each entry needs SG).
- **CORRECT REASONING**: the NSC region is part of Secure memory that NS code
  may *branch into* for the specific purpose of crossing the boundary. It must
  be marked NSC (not NS, not plain S), must be executable, and each entry must
  begin with SG.
- **EXAMPLE** (bad): SAU region covering the veneers programmed with NSC=0
  (Non-secure) — NS code branches there, no SG transition, no secure call.
- **COUNTEREXAMPLE** (good): `SAU->RLAR = limit | SAU_RLAR_NSC_Msk |
  SAU_RLAR_ENABLE_Msk;` and the linker places `.gnu.sgstubs` at that address.
- **VERIFICATION**: objdump the veneer region and confirm SG (0xE97FE97F) at
  each entry; on target, an NS call through a bad entry must fault.
- **SOURCE**: gcc-manual (-mcmse, --cmse-implib); arm-arm (DDI 0553A); cmsis.

### 3.4 Secure code calling non-secure code (BLXNS)

- **RULE**: secure code transitions to NS with BLXNS, not BLX. The compiler
  emits BLXNS when the function-pointer type is annotated
  `cmse_nonsecure_call` and the secure image is built with `-mcmse`. Without
  the annotation the compiler emits a plain BLX: the NS function then executes
  in Secure state (privilege escalation across the trust boundary) and the
  return does not restore the NS state.
- **WHY AI GETS IT WRONG**: "a call is a call"; agents annotate the call site
  instead of the function-pointer type, or annotate but forget `-mcmse`, so
  the attribute is silently ignored.
- **CORRECT REASONING**: put the attribute on the pointer *type*, compile with
  `-mcmse`, and verify the generated code contains BLXNS. All parameters and
  results cross the boundary in AAPCS registers; there is no exception state
  carry-over.
- **EXAMPLE** (bad):
  ```c
  typedef void (*ns_fn)(uint32_t);          /* no cmse_nonsecure_call */
  ns_fn f = get_ns_callback(); f(1U);       /* plain BLX on ARMv8-M */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  typedef void (*ns_fn)(uint32_t) __attribute__((cmse_nonsecure_call));
  ns_fn f = get_ns_callback(); f(1U);       /* BLXNS after -mcmse build */
  ```
- **VERIFICATION**: `arm-none-eabi-objdump -d image.elf | grep -i blxns`
  (expect matches); host: compile both variants and diff the preprocessed
  output.
- **SOURCE**: gcc-manual (Function Attributes: cmse_nonsecure_call; -mcmse);
  arm-arm (DDI 0553A); cmsis.

### 3.5 Non-secure exceptions cannot preempt Secure state

- **RULE**: while the processor is in Secure state, a pending Non-secure
  exception is not taken; it remains pending until control returns to the
  Non-secure state. Secure exceptions can preempt Non-secure code. The NVIC
  ITNS bits select each interrupt's target security state. (NMI/HardFault
  behavior in this corner is configurable via SCB_AIRCR.BFHFNMINS.)
- **WHY AI GETS IT WRONG**: "an ISR is an ISR" — agents assume a Non-secure
  timer or UART interrupt preempts a long secure routine.
- **CORRECT REASONING**: TrustZone-M deliberately protects Secure critical
  sections from NS interrupts. Design secure code with this latency in mind;
  do not rely on an NS ISR to break a Secure loop.
- **EXAMPLE** (bad): a Secure routine busy-waits for an NS ISR to clear a flag
  that will never run until the routine returns — deadlock.
- **COUNTEREXAMPLE** (good): the Secure routine returns to NS (or uses a
  Secure timer/NVIC configuration) instead of spinning on an NS-owned flag.
- **VERIFICATION**: on target, pend an NS interrupt inside a Secure critical
  section and observe deferred dispatch; check ITNS bits match the design.
- **SOURCE**: arm-arm (DDI 0553A exception/security model); cmsis (NVIC ITNS).

### 3.6 FLASH/SRAM partitioning and the NS handover

- **RULE**: partitioning is physical (linker scripts place each image in its
  flash/SRAM range) AND logical (SAU/IDAU attribute those ranges S or NS) AND
  both must agree. Typical split: S flash = bootloader + secure services, S
  SRAM = secure stacks/context; NS flash = application, NS SRAM = app data.
  Handover: secure code configures MPU + SAU, then branches to the NS reset
  handler (BLXNS). Stack sealing (`TZ_STACK_SEALING_SIZE`) protects the secure
  stack from NS overflow.
- **WHY AI GETS IT WRONG**: one linker script for both images, or a linker
  script whose ranges disagree with the SAU regions; the NS image then faults
  at first access.
- **CORRECT REASONING**: draw the memory map once (flash + SRAM, per state),
  drive both the linker scripts and the SAU tables from it, and place the NSC
  `.gnu.sgstubs` section exactly where the SAU NSC region is.
- **EXAMPLE** (bad): NS app linked to run at 0x00080000 while the SAU marks
  0x00080000 Secure — first instruction fetch faults.
- **COUNTEREXAMPLE** (good): NS flash 0x00140000 in both the NS linker script
  and SAU region 1; NSC veneers at 0x00100000 in both the secure linker script
  and SAU region 0.
- **VERIFICATION**: compare `objdump`/`readelf` section addresses against the
  SAU table; on target, boot the NS image and watch for SecureFault.
- **SOURCE**: arm-arm; cmsis (tz_context.h TZ_STACK_SEALING); qemu-docs
  (mps2-an505/mps2-an385 targets).

### 3.7 Volatile MMIO and device attributes

- **RULE**: MMIO must be accessed through volatile-qualified pointers AND the
  covering region must be configured as Device (ARMv8-M MAIR 0b00xx; ARMv7-M
  TEX/C/B device encoding), never as cacheable Normal memory. Otherwise the
  compiler may cache or reorder the access (`-O2`), or the cache serves stale
  data.
- **WHY AI GETS IT WRONG**: agents write `*(uint32_t *)0x40004000U` (no
  volatile) or give the UART region a cacheable attribute "because flash is
  cacheable".
- **CORRECT REASONING**: `static volatile uint32_t *const UART_SR = ...` plus
  a Device attribute slot in MAIR; use read-check-write loops on the volatile
  pointer and DSB where ordering matters.
- **EXAMPLE** (bad): status-polling loop reads a non-volatile pointer; at
  `-O2` the compiler hoists the read and the loop never sees the flag set.
- **COUNTEREXAMPLE** (good): the same loop reads a `volatile uint32_t *` in a
  region whose MAIR attribute is Device nGnRnE.
- **VERIFICATION**: host model asserts the UART region's MAIR slot is a device
  attribute; target: hammer the register under `-O2` and diff asm.
- **SOURCE**: cmsis (mpu_armv8.h ARM_MPU_ATTR_DEVICE_*); arm-arm; gcc-manual.

---

## Failure-mode quick table

| Symptom on target | Likely cause | Rule |
|---|---|---|
| MemManage fault on an address "I configured" | region ENABLE or MPU ENABLE missing | 1.2 |
| Region covers wrong addresses | size not power of two / base not aligned | 1.1, 2.2 |
| Unprivileged task faults on its own data | AP excludes unprivileged access | 1.4 |
| NS image faults at the first instruction | SAU not configured (memory stays Secure) | 3.1, 3.2 |
| NS -> S call faults at the entry | entry not NSC, or no SG at target | 3.3 |
| NS library "runs with too much power" | secure code called it without BLXNS | 3.4 |
| NS interrupt never fires during S code | NS exceptions blocked in Secure state | 3.5 |
| UART polls forever under -O2 | non-volatile pointer or cacheable region | 3.7 |
