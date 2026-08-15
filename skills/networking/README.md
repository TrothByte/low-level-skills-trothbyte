# networking — Skills

High-performance networking is kernel-adjacent. These skills cover the eBPF verifier model — what programs load and why, from the verifier's perspective.

## Skills in this domain

| Skill | What it does | Stability | Path |
|---|---|---|---|
| `ebpf-verifier-reasoning` | Use when writing, reviewing, or debugging eBPF C programs — map_lookup_elem null checks, bounded loops, pointer arithmetic on packet/map/stack pointers, helper argument constraints, and reading verifier rejection logs. Teaches how the kernel verifier proves safety and which patterns it rejects at load time. | researched | `skills/networking/ebpf-verifier-reasoning` |
| `networking-hardware-rdma-nic-offload` | Use when writing or reviewing RDMA verbs code or NIC offload claims — QP/MR/CQ semantics, transport vs link layer (RC/RoCE/iWARP/InfiniBand), one-sided operations, and DPDK/eSwitch/rte_flow offload. Prevents invented verbs APIs and offload features misattributed to the wrong layer. | researched | `skills/networking/networking-hardware-rdma-nic-offload` |

## How to use

- Read the `SKILL.md` of a skill for its triggers, reasoning rules, and verification commands.
- `references/` hold the deep knowledge; `examples/good|bad` are verified compile-and-run fixtures;
  `evals/` define how the skill is tested.
- Load only the skill you need (see `skills/_meta/meta-routing`); references load on demand.

## Related

- [Back to repository root](../../README.md)
