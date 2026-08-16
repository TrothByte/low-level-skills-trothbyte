# Capability-Based Security — Reference

Sources: `cheri-spec`, `cheribsd-docs`, `cwe` (CWE-287/269), and (proposed
new) the seL4 Manual. The Python model in examples/ implements the three core
properties — unforgeability, confinement, revocation — and was run on this
host.

## 1. What makes a true capability

- **RULE**: a capability is an unforgeable token of authority held by a
  subject, granting access to a specific object with specific rights; the
  *possession* of the token, not the identity of the user, authorizes the
  operation.
- **WHY AI GETS IT WRONG**: calling any access token a "capability" — a
  guessable URL or an object with a global rights table is ambient (B2).
- **CORRECT REASONING**: three tests: (1) unforgeability (cannot be fabricated
  without the authority to create it), (2) confinement (cannot be copied out
  of the subject's container without authorization), (3) non-ambient (the
  operation cannot be reached through identity/UID/path).
- **EXAMPLE**: an seL4 capability slot in a CNode; a CHERI tagged pointer; a
  Unix file descriptor.
- **COUNTEREXAMPLE**: a path string like `/tmp/secret` granting access because
  the check is "file is world-readable".
- **VERIFICATION**: `examples/good/capability_model.py` — a non-holder is
  denied and revocation removes the granted right (run on host).
- **SOURCE**: `cheri-spec` (capability model); seL4 manual (proposed new).

## 2. Confinement vs delegation

- **RULE**: delegation requires holding the capability; an unconfined system
  lets a subject duplicate a capability it merely observed. Confinement means
  authority flows only along explicit grant edges.
- **WHY AI GETS IT WRONG**: assuming that handing out a capability to one
  process also lets that process hand it to others freely ("it's just a
  handle").
- **CORRECT REASONING**: seL4 requires an explicit capability transfer
  (IPC carrying the cap, or `seL4_CNode_Move/Copy`); Unix requires an
  explicit `sendmsg`/`SCM_RIGHTS` of an FD; CHERI allows passing a capability
  but bounds/permissions are preserved and the source can be revoked by
  sealing.
- **EXAMPLE**: `seL4_CNode_Move` moves one capability from one slot to
  another — the source no longer holds it (move, not copy).
- **COUNTEREXAMPLE**: copying the raw bytes of a capability (CHERI: the tag
  bit is lost — forge attempt fails, but copying the 15-byte bounds is a
  design smell).
- **VERIFICATION**: model asserts a holder cannot grant more than it holds.
- **SOURCE**: seL4 manual (proposed); `cheri-spec`.

## 3. Revocation must reach derived copies

- **RULE**: revoking a capability must remove the authority, including every
  copy that was already delegated, within a bounded time — otherwise
  "revocation" is cosmetic.
- **WHY AI GETS IT WRONG**: "just delete the token" — but a copy was already
  handed out (B7).
- **CORRECT REASONING**: seL4 `seL4_CNode_Revoke` recursively revokes derived
  capabilities; Unix: closing an FD in one process does not revoke it in
  another (only a shared kernel object with refcounting does); CHERI:
  unmapping/zeroing the source does not zero copies held elsewhere — you must
  revoke through the object (e.g., a seal) or use monotonic generations.
- **EXAMPLE**: seL4 `Revoke` on the root slot clears all derived cap nodes.
- **COUNTEREXAMPLE**: a token system where "revoke" just invalidates the
  database row but existing tokens still verify for a grace period.
- **VERIFICATION**: `examples/good/capability_model.py` asserts the holder
  loses access after `revoke` (run on host).
- **SOURCE**: seL4 manual (proposed); `cwe` CWE-269 (improper privilege
  management).

## 4. Least privilege and ambient authority audit

- **RULE**: every grant should be the minimum rights on the minimum object
  for the minimum time; every code path that authorizes by identity, UID, or
  path instead of by capability is ambient authority.
- **WHY AI GETS IT WRONG**: "the process is root, so it can do anything" is
  the textbook ambient-authority failure; granting `Write` because it is
  easier than `Append`.
- **CORRECT REASONING**: split privileges into the minimum capabilities;
  convert ambient checks (UID, gid, path, pid) into explicit capability
  checks; for Linux, prefer file capabilities + bounding sets over setuid.
- **EXAMPLE**: `capability_model.py`'s good path: `grant(write)` on the exact
  object, denied to non-holders.
- **COUNTEREXAMPLE**: `examples/bad/ambient_authority.py` — a check that
  trusts `is_admin` (process identity) instead of a held capability.
- **VERIFICATION**: run both Python models and compare deny/allow outcomes.
- **SOURCE**: `cwe` CWE-269/CWE-287; `linux-namespaces` (capability sets).
