---
name: secure-boot-chain
description: Use when implementing or reviewing UEFI Secure Boot, shim, MOK enrollment, kernel/module signature verification, or Android Verified Boot 2.0. Covers the chain of trust, PK/KEK/db/dbx databases, vbmeta and rollback indexes, and signature verification flows.
---

# Secure Boot Chain of Trust

## When to use

- Implementing or reviewing UEFI Secure Boot: key enrollment, image
  signing, db/dbx management, or a "Verification failed /
  EFI_SECURITY_VIOLATION" boot failure.
- Working with shim and MOK (Machine Owner Keys) to boot a distro with
  self-signed keys under Secure Boot.
- Reviewing Linux kernel/module signature enforcement
  (`CONFIG_MODULE_SIG_ENFORCE`, built-in x509 keyring, lockdown).
- Implementing or auditing Android Verified Boot 2.0: vbmeta, chain
  descriptors, rollback protection.
- Answering "is Secure Boot actually enforcing anything here?" — the
  answer requires walking the whole chain, not reading one flag.

## When not to use

- Pre-UEFI boot ordering (Power-on → SEC/PEI) with no signature checks —
  use `bootloader-stages`.
- Firmware/edk2 implementation details (protocols, phases, HII) — use
  `bootloader-uefi-firmware`; Secure Boot is one chapter of that domain.
- ACPI/SMBIOS table work on a Secure Boot platform — use
  `bootloader-uefi-acpi-dtb`.
- Crypto primitives themselves (RSA/ECDSA math, TLS) — this skill assumes
  signature verification exists and reasons about the chain layout.
- ELF/module loading mechanics unrelated to signatures — use
  `kernel-loader-elf`.
- Kernel API compatibility questions — use `kernel-api-drift-migration`.
- Reproducible-firmware binary provenance — use
  `reproducible-builds-firmware`.

## What the agent often gets wrong

1. Treating Secure Boot as a single signature. It is a chain:
   firmware → shim → bootloader → kernel → modules, and each link verifies
   the next with its own key database.
2. Confusing PK/KEK/db/dbx roles — e.g. suggesting a kernel module key be
   added to the PK, or proposing to remove dbx entries instead of signing
   properly.
3. Forgetting dbx (revocation): "Secure Boot is secure" is false without a
   working revocation path — dbx is what actually stops exploited signed
   loaders.
4. Assuming signature verification means encryption: a signed image is not
   encrypted, it is still fully readable.
5. Getting the MOK flow wrong: MOK is shim's mechanism, not the firmware's.
   `mokutil --import` alone does not enroll — the enrollment completes in
   the MOK manager on the next boot.
6. Ignoring rollback protection in AVB: without a monotonic rollback-index
   check, an attacker can downgrade to an older vulnerable image.
7. Answering every Secure Boot problem with "disable Secure Boot" instead
   of finding which link in the chain failed.
8. Confusing UEFI authenticated variables (PK/KEK/db/dbx, must be updated
   via a signature-wrapped SetVariable) with ordinary EFI variables.

## How to reason correctly

1. Draw the chain for the platform: which component verifies which, and
   with which database. UEFI firmware verifies images against db/dbx only;
   shim verifies the bootloader against MOK; the kernel verifies modules
   against its built-in x509 keyring.
2. On a boot failure, walk each link: is the image signed? is the signing
   cert in db (or chained to a db CA)? is it in dbx? is enforcement on
   (`SecureBoot` + `SetupMode` off)? Only then conclude which link broke.
3. For the kernel: check `CONFIG_MODULE_SIG_ENFORCE`, the built-in keyring,
   then per module `modinfo -F signer module.ko` and
   `sbverify --cert cert.der module.ko`. Lockdown (`lockdown.verbose=1`)
   reveals which operation was blocked.
4. For AVB: the rollback index stored in RPMB must never decrease; vbmeta
   chain descriptors tie boot/recovery/vendor_boot images to keys the
   bootloader trusts. A downgrade attack fails only if the index is
   actually enforced and monotonic.
5. Verify the databases, not the story: `sbctl status`, `mokutil
   --list-enrolled`, `efi-readvar -v db` show what is really enrolled.
   Remember the one thing that is *authorized* and the *image verification*
   databases are different sets.

## What to verify

- Each link's signature verifies and its signer is authorized: in db / in
  MOK / in the kernel keyring, and NOT in dbx.
- No accidental PK/KEK/db/dbx overwrite (no `sbkeysync` mishaps, no
  `SetupMode` left on after enrollment).
- Kernel modules are signed and enforcement is on; no unsigned module
  loads (`dmesg` for `module verification failed`).
- AVB rollback index is monotonic and the vbmeta keys match the
  bootloader's trusted keys.

## How to verify

Host models (run on this Windows host, real output recorded in
`evals/README.md`):

```
python examples/tools/chain_verify.py      # 12 scenarios, all PASS
python examples/good/signature_chain.py    # hashlib chain, VERIFY OK
gcc -O2 -o sig.exe examples/good/signature_chain.c && ./sig.exe
python examples/bad/wrong_database_use.py  # naive KEK==db check is the flaw
gcc -O2 -o wdb.exe examples/bad/wrong_database_use.c && ./wdb.exe
```

`chain_verify.py` covers: (a) image signed by an authorized db key boots,
(b) key only in KEK/PK does NOT authorize boot, (c) a key revoked in dbx is
rejected even if present in db, (d) shim/MOK flow before and after
enrollment and revocation, (e) AVB rollback index monotonicity and key
mismatch.

Target commands (UEFI machine or QEMU+OVMF; documented, not run here):

```
sbctl status                              # setup/enrolled state
sbctl verify                              # verify signed binaries in ESP
mokutil --list-enrolled                   # MOK list shim will accept
efi-readvar -v db                         # dump signature database
modinfo -F signer module.ko               # kernel module signer
avbtool info_image --image vbmeta.img
avbtool verify_image --image boot.img
```

## Where the knowledge comes from

- UEFI Specification — Secure Boot chapter (https://uefi.org/specifications)
- shim — first-stage UEFI bootloader (https://github.com/rhboot/shim)
- mokutil / MOK documentation (https://github.com/lkundrak/mokutil)
- Android Verified Boot 2.0 (https://source.android.com/docs/security/features/verifiedboot)
- Linux kernel signature verification docs (https://docs.kernel.org/admin-guide/module-signing.html)
- sbctl (https://github.com/Foxboron/sbctl)

## Related skills

- `bootloader-uefi-firmware` — edk2/firmware side of Secure Boot, variable
  attributes, ExitBootServices (require).
- `bootloader-stages` — the pre-UEFI boot path where the chain starts
  (recommend).
- `bootloader-uefi-acpi-dtb` — firmware-side platform tables on the same
  UEFI platform (recommend).
- `kernel-loader-elf` — how the kernel image/module is loaded after
  signature verification passes (recommend).
- `kernel-api-drift-migration` — kernel-side stability concerns around
  module signing keyring changes (recommend).
- `reproducible-builds-firmware` — binary provenance for firmware images
  that the chain verifies (recommend).

## Evaluation

Host-run models are the ground truth on this machine: `chain_verify.py`
(12/12 PASS), the C and Python hash chains (identical digests), and the
bad fixture showing the KEK-as-db flaw. Synthetic: scenarios a–e; each
must produce the documented verdict. False-positive: a KEK/PK-only key must
NOT be accepted as a boot authorizer; a dbx-only cert must be rejected.
Adversarial: "the module signed with the KEK key boots" — the model must
reject it; "rollback index lowered" — must reject. Historical:
BlackLotus (bootkit writing the hidden `boot` variable over dbx
protection), CVE-2023-40238 (GRUB/shim shim_lock bypass), and
mok-list-variable handling are reasoning evals. Full matrix:
`evals/README.md`.
