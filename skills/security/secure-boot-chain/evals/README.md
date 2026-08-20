# Evaluation — secure-boot-chain

Skill: `skills/security/secure-boot-chain`. Stability: `researched`.
The chain-of-trust rules (db/dbx-only image verification, PK/KEK database
update roles, shim/MOK flow, AVB rollback) are KNOWN from the primary
sources listed in SKILL.md. Host-executable models in `examples/` were run
on this Windows host (Python 3.11.9, gcc 16.1.0 MSYS2/UCRT64) and their
output is recorded below; everything requiring a UEFI machine or
QEMU+OVMF is UNVERIFIED on this host.

## Verified facts (host, recorded 2026-08-20)

`python examples/tools/chain_verify.py` — 12/12 scenarios PASS, exit 0:

```
[PASS] a:  kernel module signed by a db key boots
[PASS] a2: shim cert chains to a CA enrolled in db
[PASS] b:  KEK-only key does NOT authorize boot (KEK != db)
[PASS] b2: PK-only key does NOT authorize boot (PK != db)
[PASS] c:  key in db but revoked in dbx is rejected
[PASS] c2: cert present only in dbx is rejected
[PASS] d:  bootloader not yet enrolled in MOK is rejected
[PASS] d2: MOK enrollment (mokutil --import + reboot) allows boot
[PASS] d3: revoked MOK key is rejected
[PASS] e:  image rollback index >= stored RPMB index is accepted
[PASS] e2: older image (rollback 5) is a downgrade and is rejected
[PASS] e3: image vbmeta key not trusted by the bootloader is rejected
[FLAG] naive 'KEK == db' check would claim: VERIFIED — contradicts
       firmware_verify; that belief is the flaw in examples/bad/
       wrong_database_use
Summary: 12/12 scenarios pass
```

C and Python hash chains produce IDENTICAL digests (SHA-256 over the same
inputs), cross-checking the inline C implementation against hashlib:

```
root_key_hash      = 53ee2a345a1cbf8f01c840e98e2c72e8826b89578da737d03dd600c68bb67f83
intermediate_sig   = b339f8643a83bf29266d4f9f8615c97651210d2a5fe5cd48733fc3ca591d0280
leaf_sig           = b140e6f6f6618239eb31b1484e5918612f6cad4e87935bc34b157fe99ae354c8
link 1 (root -> intermediate)     : VERIFY OK
link 2 (intermediate -> leaf)     : VERIFY OK
link 2 with modified leaf          : VERIFY FAIL (expected)
```

`python examples/bad/wrong_database_use.py` and the compiled C twin both
print the flaw:

```
  naive (KEK == db)     : VERIFIED (WRONG: KEK is not db)
  firmware (db/dbx)     : REJECTED (signer not in db)
```

## Synthetic evals

| Case | Fixture | Expected | Status |
|------|---------|----------|--------|
| authorized key in db accepted | `tools/chain_verify.py` a | ACCEPTED | runs (2026-08-20) |
| chain to a db CA accepted | `tools/chain_verify.py` a2 | ACCEPTED | runs |
| KEK-only key rejected | `tools/chain_verify.py` b | REJECTED | runs |
| PK-only key rejected | `tools/chain_verify.py` b2 | REJECTED | runs |
| dbx beats db | `tools/chain_verify.py` c | REJECTED | runs |
| dbx-only cert rejected | `tools/chain_verify.py` c2 | REJECTED | runs |
| MOK unenrolled / enrolled / revoked | `tools/chain_verify.py` d–d3 | REJECTED / ACCEPTED / REJECTED | runs |
| AVB rollback monotonic / downgrade / key mismatch | `tools/chain_verify.py` e–e3 | ACCEPTED / REJECTED / REJECTED | runs |
| hash chain links verify, tampered child fails | `good/signature_chain.c` + `.py` | VERIFY OK / FAIL | runs |

## False-positive evals

- A key that is in KEK or PK (but not db) must NOT be treated as a boot
  authorizer — "the platform key signed it" is not enough (verified in
  scenarios b/b2).
- A cert present only in dbx must be rejected, not treated as "unknown"
  (scenario c2).
- A MOK key that was revoked must not pass shim verification (scenario
  d3).
- An AVB image with rollback index equal to or higher than RPMB is
  legitimate even if it is an *older release* with a higher index — only a
  regression is a violation.
- Correct positive flow: a db-signed module (scenario a), a Microsoft-CA
  shim chain (a2), a freshly enrolled MOK (d2) — none of these are flaws.

## Historical evals

- **BlackLotus (2023)** — explain why writing a hidden `boot` variable
  with an abandoned-but-still-in-db Microsoft-signed loader bypassed
  Secure Boot, and why dbx revocation + firmware updates are the fix.
  Agent must reference the db/dbx-only verification rule, not "the chain
  was broken by crypto".
- **CVE-2023-40238 (GRUB2 / shim shim_lock bypass)** — explain the chain
  link that failed (shim trusts grub; grub's shim_lock bypass runs code
  before kernel) and the mitigation flow (grub update + dbx of signed
  grub binaries). Reasoning eval.
- **mok-list-variable issues** — the MOK list is an authenticated efivar;
  a revoked MOK must not be re-enabled through variable handling bugs.
  Reasoning eval.
- Status: these are reasoning evals on this host (no affected firmware
  available); named-incident claims are UNVERIFIED as hardware runs.

## Adversarial evals

- "This module is signed with the KEK key, so it is authorized" — the
  model must REJECT (scenario b; naive belief printed by the bad
  fixture).
- "Lowering the rollback index to the vulnerable firmware is fine" — the
  model must REJECT the downgrade (scenario e2).
- "The image verifies in a simulator with a permissive verification
  routine" — the unconditional-pass trap: the C/Python models above use
  the db/dbx-only rule; any implementation that consults PK/KEK for image
  authorization is wrong regardless of what a stub test returns.
- "Secure Boot is on, therefore everything loaded is safe" — must be
  countered with the dbx revocation question and the "which link
  verifies which" walk.

## Verification commands (target — UEFI machine or QEMU+OVMF)

Documented, NOT run on this host (no UEFI toolchain):

```
sbctl status
sbctl verify
mokutil --list-enrolled
efi-readvar -v db
modinfo -F signer module.ko
avbtool info_image --image vbmeta.img
avbtool verify_image --image boot.img
```

For QEMU: `qemu-system-x86_64 -bios OVMF.fd` with OVMF's Secure Boot build
(enrolled keys baked in or via edk2 enrollment), then reproduce each
scenario against real firmware.

## Scoring

- Precision: the model encodes the spec rules (db/dbx-only image
  verification, PK/KEK update-only roles, dbx precedence, MOK flow, AVB
  rollback) — false positives on the guarded positive flows are measured
  by scenarios a/a2/d2/e.
- Recall: all modeled failure classes are detected (b, b2, c, c2, d3, e2,
  e3) — 12/12 on the host run.
- FP-rate: zero on the host run (guarded positives all PASS).
- Decisive test: "which database authorizes a boot image, and which
  databases authorize updates?" — if the answer conflates PK/KEK with db,
  the skill has not been applied.
