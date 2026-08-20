"""chain_verify.py — host-runnable model of the UEFI Secure Boot / shim /
Android Verified Boot 2.0 chain of trust.

The model reproduces the *rules* of the chain, not the crypto:
  - four signature databases (PK, KEK, db, dbx) and their roles;
  - UEFI image verification consults only db/dbx — PK/KEK never authorize
    boot images, they only authorize updates to the databases;
  - dbx (revocation) wins over db membership;
  - a certificate chain that terminates in a db CA is accepted;
  - shim verifies the OS bootloader against MOK keys (firmware db is not
    consulted by shim);
  - AVB 2.0 rollback index is monotonic; a candidate with a lower index is
    a downgrade and must be rejected.

Each scenario prints [PASS] when the model behaves as the specifications
mandate, [FAIL] otherwise. Exit code is 0 only when every scenario passes.

Run:  python chain_verify.py
"""

from __future__ import annotations


class SigDb:
    """One UEFI signature database (PK, KEK, db, dbx) or the shim MOK list."""

    def __init__(self, name: str) -> None:
        self.name = name
        self.keys: set[str] = set()
        self.revoked: set[str] = set()

    def enroll(self, key: str) -> None:
        self.keys.add(key)

    def revoke(self, key: str) -> None:
        """dbx semantics: revoking also records membership so a dbx-only
        certificate is distinguishable from an absent one."""
        self.keys.add(key)
        self.revoked.add(key)


PK = SigDb("PK")
KEK = SigDb("KEK")
DB = SigDb("db")
DBX = SigDb("dbx")
MOK = SigDb("MOK")

DB_CA = "ms-ca"  # a CA certificate enrolled in db (e.g. Microsoft 3rd-party UEFI CA)


def firmware_verify(signer: str, chain_to_db_ca: bool = False) -> str:
    """UEFI image verification (spec order): dbx first, then db. PK and KEK
    are never consulted for image verification."""
    if signer in DBX.revoked:
        return "REJECTED: in dbx (revoked)"
    if signer in DB.keys:
        return "ACCEPTED: in db"
    if chain_to_db_ca and DB_CA in DB.keys:
        return "ACCEPTED: cert chains to db CA"
    return "REJECTED: not in db"


def shim_verify(signer: str) -> str:
    """shim boots after firmware. It verifies the OS bootloader with the MOK
    list, not with the firmware db/dbx."""
    if signer in MOK.revoked:
        return "REJECTED: MOK revoked"
    if signer in MOK.keys:
        return "ACCEPTED: MOK enrolled"
    return "REJECTED: not enrolled in MOK"


def avb_verify(image_key: str, trusted_key: str, image_rollback: int,
               stored_rollback: int) -> str:
    """AVB 2.0: the bootloader verifies the image vbmeta against a trusted
    public key and a rollback index stored in RPMB. The index must never
    decrease (downgrade protection)."""
    if image_rollback < stored_rollback:
        return "REJECTED: rollback index regression"
    if image_key != trusted_key:
        return "REJECTED: vbmeta key mismatch"
    return "ACCEPTED"


results: list[tuple[str, bool]] = []


def scenario(name: str, expected: str, got: str) -> None:
    passed = expected == got
    results.append((name, passed))
    print(f"[{'PASS' if passed else 'FAIL'}] {name}")
    print(f"      expected: {expected}")
    print(f"      got     : {got}")


# --- scenario a: authorized key in db ---
DB.enroll("modkey")
scenario("a: kernel module signed by a db key boots",
         "ACCEPTED: in db", firmware_verify("modkey"))

# --- scenario a2: chain to a db CA (shim signed by Microsoft) ---
DB.enroll(DB_CA)
scenario("a2: shim cert chains to a CA enrolled in db",
         "ACCEPTED: cert chains to db CA",
         firmware_verify("shim", chain_to_db_ca=True))

# --- scenario b: KEK membership does not authorize images ---
KEK.enroll("third-party-loader")
scenario("b: KEK-only key does NOT authorize boot (KEK != db)",
         "REJECTED: not in db", firmware_verify("third-party-loader"))

# --- scenario b2: PK membership does not authorize images ---
PK.enroll("rootkits-key")
scenario("b2: PK-only key does NOT authorize boot (PK != db)",
         "REJECTED: not in db", firmware_verify("rootkits-key"))

# --- scenario c: dbx revocation wins over db membership ---
DBX.revoke("modkey")  # modkey is in db (scenario a) and now also revoked
scenario("c: key in db but revoked in dbx is rejected",
         "REJECTED: in dbx (revoked)", firmware_verify("modkey"))

# --- scenario c2: dbx-only certificate is rejected ---
DBX.revoke("legacy-driver")  # enrolled into dbx only, never in db
scenario("c2: cert present only in dbx is rejected",
         "REJECTED: in dbx (revoked)", firmware_verify("legacy-driver"))

# --- scenario d: shim / MOK flow ---
scenario("d: bootloader not yet enrolled in MOK is rejected",
         "REJECTED: not enrolled in MOK", shim_verify("distro-bootloader"))
MOK.enroll("distro-bootloader")
scenario("d2: MOK enrollment (mokutil --import + reboot) allows boot",
         "ACCEPTED: MOK enrolled", shim_verify("distro-bootloader"))
MOK.revoke("distro-bootloader")
scenario("d3: revoked MOK key is rejected",
         "REJECTED: MOK revoked", shim_verify("distro-bootloader"))

# --- scenario e: AVB 2.0 rollback protection ---
scenario("e: image rollback index >= stored RPMB index is accepted",
         "ACCEPTED",
         avb_verify("vbmeta-key-1", "vbmeta-key-1", 8, 7))
scenario("e2: older image (rollback 5) is a downgrade and is rejected",
         "REJECTED: rollback index regression",
         avb_verify("vbmeta-key-1", "vbmeta-key-1", 5, 7))
scenario("e3: image vbmeta key not trusted by the bootloader is rejected",
         "REJECTED: vbmeta key mismatch",
         avb_verify("attacker-key", "vbmeta-key-1", 9, 7))

# --- wrong-database-use cross-check (examples/bad/wrong_database_use) ---
naive = "VERIFIED" if "third-party-loader" in KEK.keys else "REJECTED"
print("\n[FLAG] naive 'KEK == db' check would claim:", naive,
      "— contradicts firmware_verify; that belief is the flaw in",
      "examples/bad/wrong_database_use")

failed = [n for n, p in results if not p]
print(f"\nSummary: {len(results) - len(failed)}/{len(results)} scenarios pass")
for n in failed:
    print("  FAILED:", n)
raise SystemExit(1 if failed else 0)
