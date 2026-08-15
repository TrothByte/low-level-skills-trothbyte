---
name: hypervisor-vmx-svm-internals
description: Use when writing, reading, or reviewing hypervisor code: VT-x VMCS layout, VMXON/VMLAUNCH/VMEXIT state transitions, SVM VMCB/VMRUN, EPT/NPT two-level page tables, APICv and posted interrupts, and VMEXIT reason-code handling. Prevents wrong control fields, misconfigured exits, and guest-escape vulnerabilities.
---

# Hypervisor Internals: VT-x, SVM, EPT/NPT, APICv

## When to use

- Writing or reviewing KVM/VMM code that manipulates VMCS (VMX) or VMCB (SVM),
  launches VCPUs, or handles VM exits.
- Configuring EPT (Intel) / NPT (AMD) two-stage translation and diagnosing guest
  physical vs host physical confusion.
- Reasoning about APICv / posted interrupts and virtual interrupts.
- Handling VMEXIT reason codes (VMX exit reason; SVM `EXITCODE`) correctly, including
  intercept classes and error status.
- Reviewing mitigations for L1TF/MDS/Spectre-v2 in a hypervisor context.

## When not to use

- User-space virtual device emulation (no VMCS/VMCB) — use `qemu-system-setup`.
- x86 protected-mode segments/flags in general — use `asm-x86-64-registers-and-addressing`.
- Kernel memory management (the *guest's* MM is not the hypervisor's).
- Arm/arm64 virtualization (different VM control structures, IPA stage-2) — out of scope.

## What the agent often gets wrong

- "VMLAUNCH re-enters a running guest." No — after a VM exit, `VMRESUME` is required;
  `VMLAUNCH` in that state faults with `VMfailValid` (error 1: "VMLAUNCH with
  non-clear VMCS").
- "VMCS is a memory structure you can poke." VMREAD/VMWRITE only; direct field
  offsets are allowed but reserved fields are inaccessible and guest/host state is
  context-saved by hardware.
- "EPT is one level of indirection." EPT/NPT is a *two-stage* walk: guest VAs →
  guest physical (guest page tables) → host physical (EPT/NPT). Mismatching the
  stage (e.g., writing guest physical addresses into the EPT as if they were host
  physical) is the classic hypervisor memory bug.
- "VMEXIT reason codes are the same on AMD and Intel." Different encodings; SVM
  `EXITCODE` vs VMX exit-reason field differ, and AMD has `EXITINFO1/2` with
  per-intercept meanings.
- "APICv is just a flag." `APIC_BASE` MSR + `SecondaryProcCtl` bit 8 + posted
  interrupt descriptor + PID address; misconfiguring the PID address silently drops
  or misroutes interrupts.
- "EPT violations mean a guest bug." An EPT violation/Misconfig is a hypervisor
  *page-table* miss — the guest memory access itself is fine; the VMM must walk and
  fill the EPT entry or emulate. Treating it as a guest fault is a correctness bug.

## How to reason correctly

1. Draw the state machine: VMX: `VMXON` → (VMCS clear) → `VMPTRLD` → `VMLAUNCH`
   (first) → `VMEXIT` → `VMRESUME` (subsequent) → `VMXOFF`. SVM: `VMCB` setup →
   `VMRUN` (always full reload) → `VMEXIT` (hardware saves guest state) → re-`VMRUN`.
   SVM has no launch/resume distinction — every `VMRUN` is "resume".
2. For every memory access, label the stage: guest VA → guest PA (guest page tables)
   → host PA (EPT/NPT). The EPT/NPT walk translates guest PA to host PA; only after
   both stages is the physical access complete.
3. For VMEXIT handling, first decode the reason code, then the intercept/error
   qualifiers, then act — and check that the exit reason actually matches what you
   think caused the exit. Never handle "unknown exit" by resuming blindly.
4. For interrupts, trace the path: physical IRQ → posted interrupt descriptor
   (NV+NDST) → virtual APIC page → guest vector. If PID is wrong, the guest never
   sees the interrupt — indistinguishable from a dropped IRQ.
5. For mitigations, treat every cross-VM shared resource as a potential leak:
   L1TF (L1 data cache shared), MDS (store buffers shared), Spectre-v2 (BTB shared).
   The fix is flush/barrier + not sharing sensitive data, per documented hypervisor
   guidance, not "just disable HT".

## What to verify

- VMCS: VMREAD/VMWRITE field encodings; launch/resume state; guest/host RIP/RSP
  saved correctly after exit.
- EPT/NPT: each stage's tables present; memory type and permissions match; no
  stage-confusion (guest PA written into EPT entries).
- VMEXIT: reason decoded against the correct manual (Intel vs AMD); error status
  checked (`VM-instruction error` field / `EXITINFO1`).
- APICv: PID address is a valid host physical address; NV/NDST set; virtual APIC
  page enabled.
- Determinism: exit handling always returns to the guest (`VMRESUME`/`VMRUN`), never
  "returns" into the host by accident.

## How to verify

```
# KVM ioctl demo (documented; /dev/kvm needed — run inside QEMU on a Linux host):
#   the host side of the API in examples/good/kvm_ioctl_demo.c uses only
#   KVM_CREATE_VM / KVM_CREATE_VCPU / KVM_GET_MSRS / KVM_RUN (documented via
#   kvm-docs); on this machine /dev/kvm and qemu are absent, so the demo is
#   documentary, not executed.

gcc -Wall -Wextra -Werror -O2 examples/good/kvm_ioctl_demo.c -o kvm_demo
# on Linux with /dev/kvm: ./kvm_demo   (must not fault; prints vCPU model)
```

Toolchain status: this machine is Windows (win32); no `/dev/kvm`, no qemu. All
examples are documentary (researched — toolchain not available; command:
`gcc ... kvm_ioctl_demo.c && ./kvm_demo` on Linux with `/dev/kvm`, or in QEMU).

## Where the knowledge comes from

- `intel-sdm` — Vol.3C (VMXON/VMLAUNCH/VMEXIT/VMRESUME), Vol.3C §28 (EPT),
  Vol.3C §29 (APICv/posted interrupts), Vol.2 (VMREAD/VMWRITE, exit reason codes).
- `amd64-apm` — Vol.2 (VMCB, VMRUN, intercepts, EXITCODE/EXITINFO1/2, NPT).
- `kvm-docs` — KVM API (`KVM_CREATE_VM`, `KVM_RUN`, MSRs) for the ioctl demo.

## Related skills

- `qemu-system-setup` — the VMM layer that consumes these control structures.
- `kernel-uaccess-safety` — hypervisor page-table handling shares the discipline.
- `asm-x86-64-registers-and-addressing` — canonical addresses, control register bits.

## Evaluation

Synthetic: wrong `VMLAUNCH`-after-exit state (`bad/bad_vmlaunch_resume.c`), EPT
stage-confusion (`bad/bad_ept_stage.c`), misdecoded SVM exit code
(`bad/bad_exitcode.c`), wrong APICv PID (`bad/bad_apicv_pid.c`) — all must be
flagged with the corrected reasoning.
False-positive: correct VMCS init, correct EPT walk, correct APICv setup, and a
host/guest RIP check that correctly uses guest RIP must NOT be flagged.
Adversarial: an exit-reason handler that only checks the low byte, or a VMEXIT storm
(same reason repeating) — the agent must detect the storm and refuse to resume
blindly. Historical: L1TF/MDS/Spectre-v2 mitigations (`references/vmx-svm.md`).
Commands and recorded results: `evals/README.md`.
