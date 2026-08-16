# Evaluation — framekernel-architecture

Skill: `skills/kernel/framekernel-architecture`. Stability target:
`evaluated`. Current stability: `researched` — host fixture runs recorded
(rustc 1.97.1, python 3.11.9); no framekernel (Asterinas) build or boot was
performed, and its build commands are documented, not run here.

## Synthetic evals

| Case | Fixture | Expected | Recorded |
|---|---|---|---|
| easy/negative | `bad/framework_service_bad.rs` | service bypasses framework API | compiles+runs, reads privileged data (exit 0, MASKED) |
| easy/negative | `bad/address_space_isolation_bad.py` | services read foreign/framework frames | prints bypass, exit 0 (MASKED) |
| medium/positive | `good/framework_service_split.rs` | token-gated direct call | read-only service rejected, exit 0 |
| hard/positive | `good/address_space_isolation.py` | page-table faults on foreign frames | all four foreign accesses fault, exit 0 |
| hard/negative | "microkernel isolation" claimed for the framework | must be corrected to "privileged core in TCB" | covered by reference rule 1/3 |

Detection rule: every component claim must be classified as framework
(privileged, in TCB) or service (isolated by page tables), with the
syscall/call path and the language (Rust) assumption stated.

## False-positive evals (correct claims must NOT be flagged)

- `good/framework_service_split.rs`: token-gated direct calls with a
  read-only service correctly rejected — the intended design, no flag.
- `good/address_space_isolation.py`: MMU-enforced service isolation in a
  shared address space — no flag (this is the correct mental model).
- "Linux ABI compatible" stated as a syscall/ELF-level claim — no flag.

## Historical evals

- The framekernel papers position the design against seL4 (formally
  verified microkernel) and Linux (monolithic, C, no isolation). The agent
  must reproduce the comparison matrix and NOT claim formal verification
  for the framekernel (reference rule 3). KNOWN from apsys24-framekernel
  and arxiv-2506-03876 (fetched via search 2026-08-17); the LWN coverage
  of Asterinas documents the Linux-ABI goal. KNOWN abstract.
- UNVERIFIED: any measured performance claim (the papers' numbers were not
  reproduced on this host).

## Adversarial evals

- `bad/framework_service_bad.rs` compiles (with an explicit `unsafe` block)
  and runs, reading framework-internal data — an agent that accepts the
  output without flagging the bypass certifies a whole-system compromise.
- `bad/address_space_isolation_bad.py` runs and prints foreign-frame reads
  as normal — the expected answer is that the missing page-table check is
  the bug.
- The good fixtures are the counter-run: same operations, faults enforced.

## Verification commands (host, ACTUAL)

```
rustc -O examples/good/framework_service_split.rs -o /tmp/f1.exe
  exit 0
/tmp/f1.exe
  GOOD: privilege token gates the direct framework call; read-only service rejected
                                                                 exit 0
rustc -O examples/bad/framework_service_bad.rs -o /tmp/f2.exe
  exit 0
/tmp/f2.exe
  BAD: service read privileged framework data: 0x... (exit 0, MASKED)
python examples/good/address_space_isolation.py
  isolated: net faulted on blk-cache ...
  GOOD: page-table enforcement contains service bugs ...
                                                                 exit 0
python examples/bad/address_space_isolation_bad.py
  net reads: b'BLOCK'
  block reads: b'PRIVILEGED'
  BAD: single address space without page-table enforcement ...  exit 0 (MASKED)
```

## Verification commands (target, RESEARCHED — not run on this host)

```
git clone https://github.com/asterinas/asterinas
cd asterinas
make build
make run            # boots under QEMU; run unmodified Linux binaries to
                    # check ABI compatibility
# isolation probe: from a service, attempt a foreign-frame access and
# observe the page fault (documented in the book's internals)
```

## Verified facts

- All four fixtures produced the recorded outputs on this host (KNOWN).
- The read-only service is rejected and the unsafe bypass reads privileged
  data (KNOWN, recorded) — the token-gate delta is demonstrated.
- The framekernel definition (single address space, framework/services,
  Rust-only, MMU isolation, Linux-ABI target) — KNOWN from the Asterinas
  book and the framekernel paper (fetched 2026-08-17), cited to proposed
  sources `asterinas-book`, `arxiv-2506-03876`, `apsys24-framekernel`.
- Whether Asterinas actually boots and enforces these properties on this
  host — UNVERIFIED (no build here).

## Scoring

- precision: a claim is accepted only with the framework/service
  classification, the call path, the language assumption, and the guarantee
  class (Rust safety, NOT formal proof).
- recall: boundary placement, page-table enforcement, Rust-only rule, and
  ABI-vs-internals distinction are each demanded.
- FP-rate: the two good fixtures produce zero flags.
- Strongest single fact: the same "shared address space" model either
  faults on every foreign access (`good/address_space_isolation.py`) or
  silently exposes the privileged core (`bad/address_space_isolation_bad.py`)
  — the page-table delta is recorded, not assumed.
