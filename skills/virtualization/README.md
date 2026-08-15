# virtualization — Skills

Low-level engineering skills for this domain.

## Skills in this domain

| Skill | What it does | Stability | Path |
|---|---|---|---|
| `hypervisor-vmx-svm-internals` | Use when writing, reading, or reviewing hypervisor code: VT-x VMCS layout, VMXON/VMLAUNCH/VMEXIT state transitions, SVM VMCB/VMRUN, EPT/NPT two-level page tables, APICv and posted interrupts, and VMEXIT reason-code handling. Prevents wrong control fields, misconfigured exits, and guest-escape vulnerabilities. | researched | `skills/virtualization/hypervisor-vmx-svm-internals` |

## How to use

- Read the `SKILL.md` of a skill for its triggers, reasoning rules, and verification commands.
- `references/` hold the deep knowledge; `examples/good|bad` are verified compile-and-run fixtures;
  `evals/` define how the skill is tested.
- Load only the skill you need (see `skills/_meta/meta-routing`); references load on demand.

## Related

- [Back to repository root](../../README.md)
