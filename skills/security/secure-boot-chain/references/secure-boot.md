# Secure Boot reference — UEFI Secure Boot, shim/MOK, kernel signing, AVB 2.0

KNOWLEDGE QUALITY: statements here are KNOWN from the primary sources
listed in SKILL.md unless marked INFERRED/UNVERIFIED. Statuses track host
verification: models under `examples/` are host-run (see evals/README.md);
anything requiring UEFI hardware or QEMU+OVMF is UNVERIFIED on this host.

## 1. UEFI Secure Boot model

Secure Boot is not one signature check. Each component is verified by the
previous one, and each link has its own authority:

```
UEFI firmware  --db/dbx-->  shim  --MOK-->  bootloader  --kernel keyring-->  modules
```

### 1.1 The four signature databases (authenticated variables)

- **PK** (Platform Key) — root of trust. A certificate whose private key
  signs updates to the PK itself and to the KEK. There is exactly one PK
  variable.
- **KEK** (Key Exchange Key) — signs updates to the db and dbx variables.
- **db** (allowed signatures) — certificates and/or image hashes that
  authorized boot images must match.
- **dbx** (forbidden signatures) — revoked certificates/image hashes;
  checked BEFORE db, and a match always rejects.

Roles that are frequently confused:

- PK and KEK **authorize database updates** (variable writes), never boot
  images.
- db and dbx **authorize/forbid boot images** — the only databases the
  image-verification step consults.

Firmware image verification order (UEFI spec): load the image, check for a
signature entry in dbx first (reject on match), then look for the image's
signature/certificate (or a chain to one) in db (accept on match).
Otherwise reject with `EFI_SECURITY_VIOLATION` and do not transfer control.

### 1.2 Authenticated vs ordinary variables

PK/KEK/db/dbx are *authenticated* variables. Updating them requires a
signature-wrapped `SetVariable` call: the data is a signed
`EFI_VARIABLE_AUTHENTICATION_2` structure. Ordinary EFI variables
(`BootOrder`, vendor variables) have no such protection. Mixing the two is
a common conceptual error.

### 1.3 Setup mode vs User mode

- **SetupMode=1**: PK is empty; db/KEK can be written without
  authentication (this is the enrollment window).
- **SetupMode=0** (User mode): Secure Boot is enforced; database writes
  must be signed per the table above.
- A platform with `SecureBoot=0` but `SetupMode=0` is in "Disabled" state
  — enforcement off, but the databases are still authenticated.

## 2. Enrollment and tooling

- **sbctl** (Arch/Gentoo, Foxboron) — create keys, enroll via firmware,
  sign EFI binaries; `sbctl status`, `sbctl verify`.
- **sbkeysync** — sync a host's local key database files into firmware;
  mishandling it can overwrite PK/KEK/db/dbx wholesale.
- **efi-readvar / efi-updatevar** — read/write authenticated variables
  directly (Linux efivarfs).
- **KeyTool** — in-firmware GUI enrollment tool for db/KEK/PK.

Best-practice enrollment order: PK first (or KEK first when a platform
keeps vendor KEKs), then KEK, then db/dbx; write dbx revocations with the
same care as db.

## 3. shim and MOK

shim is a small first-stage bootloader signed by Microsoft (via the
3rd-party UEFI CA in db). It lets distros ship self-signed bootloaders:

1. Firmware verifies shim's Microsoft signature against db.
2. shim verifies the distro bootloader against the MOK list.
3. MOKs are managed with `mokutil --import cert.der` (stage 1).
4. On the **next boot**, shim's MOK manager (MokManager.efi) asks for
   confirmation; only then is the key enrolled (stage 2). The key is not
   active after step 3 alone.

`shim_lock` protocol protects grub and requires that authenticated images
have a signature in MOK/db; it is the mechanism behind several CVE-2023-40
xxx GRUB mitigations. MOK keys can also be revoked (`mokutil --revoke` or
enroll a `MOK` blacklist) — the MOK list is not immutable.

## 4. Linux kernel module signing

- `CONFIG_MODULE_SIG` — modules carry an appended signature (`modinfo`
  exposes `signer`; the `.module_sig` section holds the PKCS#7 blob).
- `CONFIG_MODULE_SIG_ENFORCE` — unsigned or wrongly-signed modules are
  rejected at load (`module verification failed: ...` in dmesg). Without
  ENFORCE, signed-at-build kernels still warn.
- Keys live in the kernel keyring: `_sig` (built-in x509 certificates
  compiled into the kernel) plus the UEFI platform keyring (`_uefi`,
  sourced from firmware db/MOK when lockdown or the platform keyring
  feature is enabled). Module signatures must chain to one of those.
- Lockdown (`CONFIG_SECURITY_LOCKDOWN_LSM`) blocks unsigned
  modules/kexec/io-writes when lockdown mode is on, even where module
  signing alone would permit it.

Verification commands: `modinfo -F signer module.ko`,
`sbverify --cert key.der module.ko`, `dmesg | grep -i "module verification"`.

## 5. Android Verified Boot 2.0 (AVB)

- A **vbmeta** image is a small metadata blob: hashtree/descriptor of the
  following image, the vbmeta public key, a **rollback index**, flags
  (e.g. `disable_verity`), and **chain descriptors** that delegate
  verification of boot/vendor_boot/recovery/system to keys declared in the
  main vbmeta.
- The bootloader stores the latest rollback index in **RPMB** (Replay
  Protected Memory Block). A candidate image is rejected if its vbmeta
  rollback index is lower than the stored one — this is what stops
  downgrade attacks.
- Key revocation: revoke a compromised key by raising the rollback index
  associated with it (the old key can no longer sign an image with a
  sufficiently high index).
- Commands: `avbtool info_image --image vbmeta.img`,
  `avbtool verify_image --image boot.img`, `avbtool make_vbmeta_image`.

## 6. Historical incidents (reasoning evals)

- **BlackLotus (2023)** — UEFI bootkit that bypassed Secure Boot by
  writing a hidden `boot`/`bootmgfw` variable pair without going through
  normal authenticated-variable signing, then hooking the boot flow. Root
  causes included lax PE parsing in the boot path and the ability to reuse
  an abandoned Microsoft-signed loader (still in db, long after its cert
  family was used). Mitigation relied on dbx revocations + firmware
  updates.
- **CVE-2023-40238 (GRUB2 / shim)** — a shim_lock bypass in GRUB2's
  `grub2-cryptodisk`/loopback handling let attackers run arbitrary code
  before the kernel despite the shim+db signing requirement; fixed in
  GRUB 2.06/2.12 and shim, with dbx updates revoking the affected signed
  GRUB binaries.
- **mok-list-variable / MOK blacklist** — the MOK list is an efivar; the
  blacklist must be handled so a revoked MOK cannot re-enable itself.
  Bugs around MOK variable handling have been used to weaken shim's
  verification.

## 7. Verification cheat sheet

```
sbctl status                       # SecureBoot/SetupMode state + keys
sbctl verify                       # every ESP binary verified
mokutil --list-enrolled            # shim MOK list
efi-readvar -v db                  # raw db contents
modinfo -F signer module.ko        # who signed this module
sbverify --cert cert.der module.ko # check module signature explicitly
avbtool info_image --image vbmeta.img
avbtool verify_image --image boot.img
```
