---
name: capability-based-security
description: Use when designing or reviewing access control that must avoid ambient authority — capability systems (seL4 cspace, CHERI capabilities, object capabilities), delegation, confinement, revocation, and least privilege. Teaches what makes a true capability, how to audit capability flows, and how to design unforgeable, revocable, confined grants.
---

# Capability-Based Security

## When to use

- Designing or auditing access-control schemes where ambient authority is the
  threat: microkernel services, plugin systems, browser extension grants,
  fine-grained file/token grants.
- Reviewing seL4 cspace / capability-node (CNode) code or CHERI-ported code.
- Deciding between ACLs and capabilities for a subsystem.
- Implementing "capability tokens" (handles, Unix FDs, sealed objects).
- Auditing delegation and revocation semantics of an existing grants system.

## When not to use

- Systems where the only trust boundary is the kernel and subjects are
  equally trusted (a single-user kernel with no isolation).
- High-level RBAC role assignment without delegation/revocation semantics
  (roles are not capabilities; mixing them causes confusion).
- Purely memory-safety review (CHERI without access control) — use
  `riscv-cheri-capability-safety` / memory-safety skills.
- Legacy Unix DAC-only review where capabilities are not in the threat model.

## What the agent often gets wrong

- Confusing *confinement* with *delegation*: a capability that can be freely
  copied out of its container is not confined; the classic seL4/object-cap
  rule is that only a subject holding a capability can pass it on (B2).
- "ACLs and capabilities are equivalent" — ACLs store access decisions on the
  object (ambient: whoever can reach the object gains its rights);
  capabilities carry authority in the subject's hand (A10).
- Treating Unix `CAP_*` as the capability model — Unix caps are still
  process-wide privilege flags with *file capabilities* extension, not
  object-grained capabilities; `setuid` bypass and `ambient`/`inheritable`
  sets are delegation-by-accident.
- Revocation: "delete the capability" is not atomic revocation if another
  copy was already sent; seL4 uses CNode deletion + MCS and `seL4_CNode_Revoke`
  which recursively revokes all derived caps (B7).
- "A token is a capability" without unforgeability and proof-of-possession —
  a URL with an ID is ambient unless keyed/sealed to the holder.
- Least privilege: granting `Write` when `Append` suffices, or a full cspace
  when a single slot suffices.
- Forgetting that capabilities must be *unforgeable* (tagged pointers in
  CHERI, kernel-held tokens) — a capability stored in memory that user code
  can modify is not a capability.

## How to reason correctly

1. Identify the authority model: who holds a capability, how it was obtained
   (creation vs receipt vs forgery), and what it authorizes.
2. Check unforgeability: can a subject construct a capability it was not
   given? (CHERI: tags prevent forging; seL4: caps live only in kernel-owned
   CNodes; Unix: FDs are kernel-held).
3. Check confinement/delegation: can a subject copy a cap it holds into
   another process without authorization? (seL4: no — caps are moved via
   capability transfers, MCS rules apply).
4. Check revocation: is there a reachable operation that removes the
   authority and all derived copies? (seL4 `Revoke`; Unix: closing FDs in all
   holders; CHERI: unmapping the source and invalidating).
5. Check least privilege per grant: only the minimum rights, on the minimum
   object, for the minimum time.
6. Audit every "ambient" path: anything decided by process identity, UID,
   path, or object location instead of by held capability is ambient
   authority — either convert or justify.
7. Verify with a model/formal tool where available (seL4 proofs; capability
   flow analysis) or at minimum a test that proves a non-holder cannot
   perform the protected operation.

## What to verify

- Every protected operation checks the caller's *capability*, not the
  caller's identity/UID.
- Capabilities cannot be forged (kernel-held, tagged, or sealed).
- Delegation requires holding the capability being delegated.
- Revocation removes the authority (and derived copies) within a bounded time.
- No ambient path bypasses the capability check (no open-by-path, no UID
  shortcut).
- Least privilege: each grant is minimal.

## How to verify

Host-executable model (Python, self-contained):

```
python3 examples/good/capability_model.py   # non-holder denied, revoke works
python3 examples/bad/ambient_authority.py   # ambient check bypasses capability
```

Documented target tools (not installed on this host):

```
# seL4: run the seL4 test suite with an assert that a process without a cap
# cannot perform the operation; proofs via the seL4 proof toolchain
python3 sel4test ...   # target platform only
# CHERI: cheribuild qemu-cheri; test tag/forgery on a morello/qemu-cheri
```

## Where the knowledge comes from

- `cheri-spec` — capability model: bounds, permissions, tag unforgeability
- `cheribsd-docs` — capability userspace and purecap behavior
- `cwe` — CWE-287/269 (improper auth, privilege) as ambient-authority classes
- `linux-namespaces` — capability sets and user namespaces context (compare)
- seL4 manual (not yet in registry — proposed new source; see report)

## Related skills

- `riscv-cheri-capability-safety` — CHERI memory capabilities in practice (require)
- `embedded-mpu-trustzone` — hardware trust boundaries for capability holders (recommend)
- `safe-low-level-from-scratch` — applying least privilege in new code (recommend)
- `rust-unsafe-reasoning` — safe wrappers around capability handles (recommend)

## Evaluation

Synthetic: capability vs ACL classification; seL4 CNode grant/revoke flow;
Unix `CAP_*` vs object-cap comparison; a token that is forgeable must be
flagged. Adversarial: "the user is root so the check passes" — ambient
authority; a capability that is copyable out of its container (no
confinement); an ACL-lookalike presented as a capability system. Historical:
seL4 "Wayward" design (allocation from ambient), the classic Unix `setuid`
delegation accidents, CHERI tag-forgery hardening (CVE-2020-7461-class on
MIPS/CHERI). FP: a legitimate seL4 `Revoke` flow and a correct CHERI
capability-passing pattern must NOT be flagged.
