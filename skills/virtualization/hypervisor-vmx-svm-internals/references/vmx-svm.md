# Hypervisor Internals: VT-x / SVM / EPT-NPT / APICv — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE
→ VERIFICATION → SOURCE. Source ids refer to `registry/sources.yaml`.

## 1. VMLAUNCH vs VMRESUME — the VMCS state machine

- **RULE**: a VMCS transitions: `clear` → `launched` after a successful `VMLAUNCH`;
  every later entry must use `VMRESUME`. `VMLAUNCH` on a `launched` VMCS faults with
  VMfailValid and VM-instruction error 1. After a VM exit, execution resumes in host
  mode; re-entry is `VMRESUME`, not `VMLAUNCH`.
- **WHY AI GETS IT WRONG**: treats entry as idempotent; generates a fresh `VMLAUNCH`
  on every loop iteration; or believes `VMXOFF`+`VMXON` is the way to "reset" the
  guest (that tears down the VMCS state and re-launches as a new guest).
- **CORRECT REASONING**: the hardware saves guest state to the VMCS on VM exit and
  restores it on `VMRESUME`. Only the *first* entry after `VMPTRLD` may be
  `VMLAUNCH`. State machines: `VMLAUNCH`/`VMRESUME` are the two distinct entry
  instructions; a valid handler tracks `launched` per VMCS.
- **EXAMPLE** (bad):
  ```c
  // inside the VMEXIT loop: re-enters a launched VMCS
  vmwrite(VMCS_GUEST_RIP, rip);        // if we've already entered once...
  vmlaunch();                          // VMfailValid (error 1)
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  if (!vmcs_launched)
      vmlaunch();          // first entry
  else
      vmresume();          // every subsequent entry
  ```
- **VERIFICATION**: on real HW, execute the bad case and read the VM-instruction
  error field (VMCS 0x4400) == 1; good case enters/returns correctly. Documented —
  no hypervisor-capable host here (KVM demo is in `examples/good/kvm_ioctl_demo.c`).
- **SOURCE**: intel-sdm Vol.3C §26.1, §26.2 (instruction descriptions);
  Vol.3C §24.4.3 (VM-instruction error field).

## 2. VMCS fields are read/written only via VMREAD/VMWRITE

- **RULE**: guest state, host state, controls, exit info, and VM-instruction error
  are VMCS fields accessed with VMREAD/VMWRITE (encoded by field index). Direct
  memory poking of the VMCS is not a supported way to modify state; reserved fields
  fault or are ignored.
- **WHY AI GETS IT WRONG**: treats VMCS as a plain struct in guest memory and does
  `vmcs->guest_rip = x`; or believes setting a control bit is enough without the
  corresponding capability MSR check.
- **CORRECT REASONING**: `VMWRITE` takes (field, value); `VMREAD` takes (field) →
  value. Control fields must be validated against `IA32_VMX_TRUE_*_CTLS` (or the
  non-"true" variants) to confirm allowed bits before writing, else `VMFAIL`
  (`VMfailValid` with error 7 for unsupported bits).
- **EXAMPLE** (bad): `vmwrite(VMCS_SECONDARY_CTLS, 0xFFFFFFFF)` — forces reserved
  bits.
- **COUNTEREXAMPLE** (good):
  ```c
  uint64_t caps = rdmsr(IA32_VMX_TRUE_PROC_CTLS);
  uint64_t v = 0;
  // only set allowed-1 bits (bits where caps bit is 1) and keep 0-clear bits 0
  v = caps;                              // start from allowed mask
  vmwrite(VMCS_PROC_CTLS, v);
  ```
- **VERIFICATION**: bad VMWRITE → VMfailValid error 7 (unsupported bits); good
  VMWRITE succeeds. Documented.
- **SOURCE**: intel-sdm Vol.3C §24 (VMCS), §30 (VMREAD/VMWRITE), §24.6.2
  (capability MSRs); amd64-apm Vol.2 (VMCB is memory-mapped — SVM differs).

## 3. EPT/NPT is a two-stage translation — no stage confusion

- **RULE**: guest memory is translated in two stages: (1) guest virtual address →
  guest physical address via the *guest's own* page tables; (2) guest physical →
  host physical via EPT (Intel) or NPT (AMD), walked by the hardware with the
  hypervisor's tables. Both stages must be present and consistent.
- **WHY AI GETS IT WRONG**: writes a guest physical address into an EPT entry as if
  it were host physical; or treats the EPT walk as "just one more guest page walk"
  and reuses guest page tables for EPT.
- **CORRECT REASONING**: the EPT/NPT tables are *host-side* structures indexed by
  guest physical addresses. Their leaf entries contain host physical addresses
  (frame numbers). Mapping `gpa = 0x1000` to host `hpa = 0x5000` requires the EPT
  entry to contain `0x5000`, NOT `0x1000`. Getting this wrong makes the CPU fetch
  guest memory from the wrong host frame — a corruption, not a fault.
- **EXAMPLE** (bad):
  ```c
  // guest PA used directly as the EPT leaf (stage-2) value
  ept_leaf(hpa_vm, gpa, PAGE_PRESENT | RW);   // wrong: stores gpa as hpa
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  uint64_t hpa = host_phys_alloc(page);
  ept_leaf(hpa_vm, gpa, hpa, PAGE_PRESENT | RW);  // leaf = host frame number
  ```
- **VERIFICATION**: run the guest; a stage-confused mapping produces either an EPT
  violation or wrong data at runtime. Documented — no KVM host here; the EPT walk
  reasoning is exercised in `examples/good/ept_walk_notes.c` (documentary).
- **SOURCE**: intel-sdm Vol.3C §28.2 (EPT), §28.3 (EPT violation/misconfig);
  amd64-apm Vol.2 §15.25 (NPT).

## 4. VMEXIT reason codes differ between Intel and AMD

- **RULE**: VMX records the exit reason in the VMCS exit-reason field (bit 0:
  "basic" vs "VM-entry failure"; bits 15:0 = reason number). SVM records it in the
  VMCB `EXITCODE` (with `EXITINFO1`/`EXITINFO2` carrying per-intercept details).
  The numeric values are NOT interchangeable.
- **WHY AI GETS IT WRONG**: ports an Intel exit handler to AMD and compares against
  VMX numbers, or assumes `EXITINFO1` has a universal meaning; handles an unknown
  reason by resuming, which masks a real fault.
- **CORRECT REASONING**: decode the reason against the platform manual, then the
  intercept/error qualifier for that specific reason. E.g. SVM `EXITCODE=0x400`
  (CR0 read intercept) has a different meaning from a VMX EPT-violation reason (48).
  Always have a default branch that logs and stops rather than blindly resuming.
- **EXAMPLE** (bad): `if (vmcb->exitcode == 48) handle_ept();` on SVM — 48 is a VMX
  value, on SVM it is not EPT.
- **COUNTEREXAMPLE** (good): switch on `exitcode` per the AMD manual's intercept
  table; use `exitinfo1/2` only where the manual defines their meaning.
- **VERIFICATION**: two-rank (or single-rank) KVM run prints decoded reasons
  (documented, KVM demo).
- **SOURCE**: intel-sdm Vol.3C §27.2 (exit reasons); amd64-apm Vol.2 §15.6.1
  (EXITCODE), §15.9 (EXITINFO1/2).

## 5. APICv and posted interrupts need the PID and virtual APIC page

- **RULE**: APICv requires `IA32_APIC_BASE` virtualization + `SECONDARY_PROC_CTLS`
  bit 8 (`Virtualize APIC accesses`) and typically the `APIC-register
  virtualization` + `virtual-interrupt delivery` bits. Posted-interrupt delivery
  needs the posted-interrupt descriptor (PID) at a host physical address in the
  `POSTED_INTR_DESC` VMCS field, with `NV` (notification vector) and `NDST`
  (destination vCPU) set.
- **WHY AI GETS IT WRONG**: enables the secondary control bit but leaves the PID
  address unset/zero; or assumes setting the control bit alone makes interrupts
  delivered, and misses that the PID's `NV` field must point at a valid notification
  vector.
- **CORRECT REASONING**: the flow is: physical IRQ → vCPU's PID (posted via
  `NDST`+`NV`) → notification interrupt → virtual-APIC page → guest vector. If PID
  address or NV is wrong, the guest never sees the interrupt — silently dropped IRQ,
  not an error.
- **EXAMPLE** (bad): `vmwrite(VMCS_SECONDARY_CTLS, sec | (1<<8));` with
  `post_intr_desc_addr = 0`.
- **COUNTEREXAMPLE** (good):
  ```c
  vmwrite(VMCS_POSTED_INTR_DESC, host_phys_of(pid));   // PID is host PA
  pid->ndst = vcpu_apic_id;                            // target vCPU
  pid->nv   = POSTED_NOTIF_VECTOR;                     // valid notification vector
  ```
- **VERIFICATION**: functional — inject an IRQ and confirm the guest handler runs.
  Documented; no APICv-capable host here.
- **SOURCE**: intel-sdm Vol.3C §29.5 (posted interrupts), §29.4 (APIC virtualization);
  kvm-docs (KVM_CAP_X2APIC_API etc.).

## 6. EPT violation vs guest page fault — two different things

- **RULE**: an EPT violation/misconfig exit means the *EPT walk itself* failed
  (missing/faulting host mapping for a guest PA). A guest page fault is a normal
  `#PF` delivered inside the guest (with CR2 = guest VA). They are different exits:
  EPT violation is a hypervisor exit; `#PF` is a guest exception.
- **WHY AI GETS IT WRONG**: sees "violation" and injects a guest page fault, or
  treats a guest `#PF` as an EPT violation and tries to fix host tables.
- **CORRECT REASONING**: on an EPT violation (VMX reason 48, or SVM NPF), the VMM
  must either populate the EPT entry for the faulting guest PA or handle the special
  case; the guest's `#PF` (VMX reason 14 / SVM EXITCODE 0x200) is handled by
  injecting/forwarding the exception in the guest context.
- **EXAMPLE** (bad): on EPT violation, inject `#PF` into the guest and resume.
- **COUNTEREXAMPLE** (good): on EPT violation, allocate host page, install EPT
  mapping `gpa→hpa`, resume; on guest `#PF`, forward as guest exception.
- **VERIFICATION**: KVM traces show distinct exit reasons; documented.
- **SOURCE**: intel-sdm Vol.3C §28.3.3; amd64-apm Vol.2 §15.25.6.

## 7. L1TF / MDS / Spectre-v2: cross-VM resource sharing must be treated as leak

- **RULE**: L1TF leaks data from the L1 data cache (shared across SMT siblings and,
  in some configurations, across VM boundaries); MDS leaks from store/fill buffers
  and other internal buffers; Spectre-v2 predicts through the shared BTB. The
  hypervisor-side mitigations are the documented flushes/barriers
  (L1D flush on vmentry in affected configs, MDS-clearing sequences, IBRS/STIBP for
  BTB isolation) — they are *not* optional "paranoid" settings.
- **WHY AI GETS IT WRONG**: "mitigations are only for security researchers"; disables
  HT/IBRS thinking it fixes everything; or assumes L1TF only matters for SGX.
- **CORRECT REASONING**: each of these is a *resource-sharing* leak: the mitigation
  is a combination of (a) not placing sensitive data where it can be read, (b) the
  documented flush/clear sequence on the relevant boundary, and (c) BTB isolation.
  The agent must verify which mitigations the target microarchitecture requires.
- **EXAMPLE** (bad): a VMM that enables no L1D flush and shares a sibling core for
  two tenants.
- **COUNTEREXAMPLE** (good): enable L1D flush on vmentry (where supported), apply
  MDS-clear on VM exit paths, set IBRS/STIBP as required, and document the residual
  risk.
- **VERIFICATION**: on affected hardware, run the documented flush and confirm with
  the vendor's tooling; documentary here.
- **SOURCE**: intel-sdm (VOLUME 3C §"L1TF"/"MDS" mitigations; architecture guidance);
  kvm-docs (KVM_CAP_X86_SMM, MDS/L1TF handling). KNOWN/INFERRED per microarch.

## 8. SVM: every entry is a VMRUN with a full VMCB reload

- **RULE**: SVM has no launch/resume distinction. `VMRUN` loads the VMCB (including
  guest state from the VMCB's save area), runs, and on exit hardware saves guest
  state back into the VMCB and sets `EXITCODE`. The next entry is simply another
  `VMRUN` after the host updates the VMCB (RIP, controls, intercepts).
- **WHY AI GETS IT WRONG**: ports the Intel "first launch, then resume" pattern and
  adds an SVM launch gate; or forgets to re-prime intercepts/ASID between runs.
- **CORRECT REASONING**: `VMRUN` is the only entry point; there is no "resume"
  variant. The VMCB is the single source of truth for guest state, and the host
  must update it before every `VMRUN` (especially guest RIP and intercepts).
- **EXAMPLE** (bad): a `svm_vmrun_once()` that runs VMRUN only the first time and
  returns on later entries.
- **COUNTEREXAMPLE** (good): loop: set `vmcb->save.rip`, `vmcb->control.intercept`,
  then `vmrun(&vmcb)`.
- **VERIFICATION**: functional run; documentary here.
- **SOURCE**: amd64-apm Vol.2 §15.5 (VMRUN), §15.6 (VMCB save/control areas).

## Quick reference table

| Topic | Rule in one line |
|---|---|
| VMX entry | first `VMLAUNCH`, then always `VMRESUME`; VMCS tracks `launched` |
| VMCS access | VMREAD/VMWRITE only; validate control bits vs capability MSRs |
| EPT/NPT | two stages: guest VA→gPA (guest tables), gPA→hPA (host EPT/NPT) |
| Exit reasons | VMX exit-reason field ≠ SVM EXITCODE; decode per manual |
| APICv/PID | PID host PA + NV + NDST + virtual APIC page, all required |
| EPT violation | host-table miss, NOT a guest `#PF`; fill mapping, then resume |
| L1TF/MDS/Spectre | shared-cache/buffer/BTB leaks; documented flush/IBRS/STIBP |
| SVM | every entry is `VMRUN`; VMCB updated before each run |
