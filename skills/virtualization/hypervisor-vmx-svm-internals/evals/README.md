# Evaluation — hypervisor-vmx-svm-internals

Skill: `skills/virtualization/hypervisor-vmx-svm-internals`.
Stability: `researched` (source-backed grounding: intel-sdm Vol.3C, amd64-apm
Vol.2, kvm-docs). No `/dev/kvm`, no qemu, no VMX/SVM-capable host on this machine
(win32). All examples are documentary with target commands recorded. No Python
simulation was run here: the VMCS/VMCB/EPT semantics are hardware-defined and a
host simulation would not exercise them (marked accordingly).

## Toolchain status

- `gcc`: available (16.1.0) but the examples target Linux KVM headers and
  `/dev/kvm`; the demo compiles only on Linux. NOT run here.
- `qemu-system` / `/dev/kvm`: absent.
- Historical mitigations (L1TF/MDS/Spectre-v2): documentary; verified against
  intel-sdm mitigation guidance and kvm-docs, marked KNOWN/INFERRED per
  microarchitecture.

Target commands to promote to `verified` (Linux + KVM host, or QEMU/KVM):

```
gcc -Wall -Wextra -Werror -O2 examples/good/kvm_ioctl_demo.c -o kvm_demo
./kvm_demo                        # expect: "KVM ioctl handshake OK", exit 0
gcc -Wall -Wextra -Werror -O2 -c examples/good/good_vmx_launch_resume.c
gcc -Wall -Wextra -Werror -O2 -c examples/good/good_ept_map.c
gcc -Wall -Wextra -Werror -O2 -c examples/good/good_svm_exitcode.c
gcc -Wall -Wextra -Werror -O2 -c examples/good/good_apicv.c
```

## Synthetic evals

| Case | Fixture | Expected | Status |
|---|---|---|---|
| easy/negative | `bad/bad_vmlaunch_resume.c` | VMLAUNCH after first entry = VMfailValid (error 1); use VMRESUME | exit 0, review-time flag |
| medium/negative | `bad/bad_ept_stage.c` | EPT leaf must be host PA, not guest PA | exit 0, review-time flag |
| medium/negative | `bad/bad_exitcode.c` | SVM EXITCODE space ≠ VMX exit reasons | exit 0, review-time flag |
| hard/negative | `bad/bad_apicv_pid.c` | PID address + NV + NDST all required | exit 0, review-time flag |

Detection rule: the reviewer must classify every memory address (guest VA / guest
PA / host PA) and every control field against the manual; a silent configuration
bug (PID=0, wrong exit-code space) is a correctness failure, not a warning.

## False-positive evals (correct code must NOT be flagged)

- `good/good_vmx_launch_resume.c` — VMLAUNCH/VMRESUME split by `launched` flag;
  control-field validation against capability MSR; must NOT be flagged.
- `good/good_ept_map.c` — leaf = host PA | flags; correct two-stage reasoning.
- `good/good_svm_exitcode.c` — AMD exit codes with EXITINFO2 for NPF; unknown
  reason stops instead of resuming.
- `good/good_apicv.c` — PID programmed before enabling virtual-interrupt delivery;
  must NOT be "simplified" to drop NV/NDST.
- `good/kvm_ioctl_demo.c` — a read-only capability query is correct; must NOT be
  flagged for "not running a guest".

## Historical evals

- L1TF (L1 data cache leak across VM/SMT): mitigation = L1D flush on vmentry in
  affected configs; NOT disabling HT alone.
- MDS (store/fill buffer leak): MDS-clear sequences on VM exit paths.
- Spectre-v2 (shared BTB): IBRS/STIBP and BTB isolation.
- These are covered in `references/vmx-svm.md` rule 7 and must be applied per
  microarchitecture, not wholesale. Marked KNOWN (documented in intel-sdm
  mitigation guidance) / INFERRED (exact MSR/sequence per CPU SKU).

## Adversarial evals

- Exit-reason handler that checks only the low byte of the exit-reason field: must
  be caught (reason + qualifier both required).
- VMEXIT storm: the same exit reason repeating (e.g. unhandled NPF, or an
  un-emulated intercept) — the agent must detect the repetition and STOP rather
  than resume blindly; `good_svm_exitcode.c` demonstrates the correct default
  branch.
- A stage-confused EPT mapping that never faults at setup time but corrupts guest
  data at runtime: review-time detection is the only defense here.

## Verified facts

No runtime verification possible on this host (no /dev/kvm, no VMX/SVM). All
claims are source-grounded and marked KNOWN (documented in intel-sdm / amd64-apm /
kvm-docs) or INFERRED (cross-microarch details). No simulation was run; the
`evals/README.md` for `asm-x86-64-registers-and-addressing` shows the
host-toolchain pattern used when such a host is available.

## Scoring (for routing eval)

- recall: all bad fixtures detectable via the reference rules (1-8).
- precision: good fixtures produce zero flags.
- FP-rate: target 0 on the good set; the main risk is flagging legitimate APICv
  setups as "missing PID" when the PID is programmed elsewhere — the reviewer must
  check the whole init sequence, not one file.
