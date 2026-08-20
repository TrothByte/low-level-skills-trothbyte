# networking — Skills

High-performance networking is kernel-adjacent.

| Skill | What it does | Type | Stability | Path |
|---|---|---|---|---|
| `bpf-core-relocation` | Use when writing or reviewing eBPF programs meant to run on many kernel versions — BTF-based CO-RE, vmlinux.h, bpf_core_read, field/type/enum relocation, or when direct kernel-struct access breaks across kernels. Teaches Compile-Once-Run-Everywhere BPF portability, distinct from verifier-feedback skills. | improved | researched | `skills/networking/bpf-core-relocation` |
| `ebpf-verifier-opaque-feedback-iteration` | Use when a BPF program fails the verifier with an opaque log: extract the failing instruction from the log tail, minimize the program, bisect register state, and iterate a minimal fix. Teaches persisting where LLMs give up. | improved | researched | `skills/networking/ebpf-verifier-opaque-feedback-iteration` |
| `ebpf-verifier-reasoning` | Use when writing, reviewing, or debugging eBPF C programs — map_lookup_elem null checks, bounded loops, pointer arithmetic on packet/map/stack pointers, helper argument constraints, and reading verifier rejection logs. Teaches how the kernel verifier proves safety and which patterns it rejects at load time. | improved | researched | `skills/networking/ebpf-verifier-reasoning` |
| `napi-network-driver` | Use when writing, reviewing, or debugging Linux network drivers using NAPI — napi_struct, napi_schedule, poll budget, napi_complete, GRO, or IRQ-coalescing and softirq behavior. Teaches the NAPI poll discipline agents get wrong in driver code. | unique | researched | `skills/networking/napi-network-driver` |
| `networking-hardware-rdma-nic-offload` | Use when writing or reviewing RDMA verbs code or NIC offload claims — QP/MR/CQ semantics, transport vs link layer (RC/RoCE/iWARP/InfiniBand), one-sided operations, and DPDK/eSwitch/rte_flow offload. Prevents invented verbs APIs and offload features misattributed to the wrong layer. | unique | researched | `skills/networking/networking-hardware-rdma-nic-offload` |

## How to use

- Read the `SKILL.md` of a skill for its triggers, reasoning rules, and verification commands.
  (`references/` hold deep knowledge; `examples/good` and `examples/bad` are compiled/run
  fixtures; `evals/README.md` defines eval cases.)
- Load only the skill you need (see `skills/_meta/meta-routing`; references load on demand.

## Related

[Back to repository root](../../README.md)
