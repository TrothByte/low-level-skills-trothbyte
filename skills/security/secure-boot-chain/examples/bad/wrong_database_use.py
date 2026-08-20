"""wrong_database_use.py — intentionally incorrect: treats the KEK as db.

Demonstrates the UEFI misconception "the KEK is the master key, so an image
signed by a KEK certificate must boot". Firmware image verification consults
only db/dbx; PK/KEK authorize updates to the databases themselves, never a
boot image. The naive check says VERIFIED; the correct check rejects the
same image (on real hardware: EFI_SECURITY_VIOLATION).

Run:  python wrong_database_use.py
"""

DBS = {
    "PK": set(),
    "KEK": {"modkey"},  # the mistake: module key enrolled into KEK only
    "db": set(),
    "dbx": set(),
}


def naive_verify(signer: str) -> str:
    """WRONG: assumes KEK membership is enough to authorize boot."""
    if signer in DBS["KEK"]:
        return "VERIFIED (WRONG: KEK is not db)"
    return "REJECTED"


def firmware_verify(signer: str) -> str:
    """Correct UEFI behavior: image verification consults only db/dbx."""
    if signer in DBS["dbx"]:
        return "REJECTED (dbx: revoked)"
    if signer in DBS["db"]:
        return "VERIFIED (db)"
    return "REJECTED (signer not in db)"


def main() -> int:
    print("wrong_database_use: KEK treated as the image-signature db")
    print("  naive (KEK == db)     : " + naive_verify("modkey"))
    print("  firmware (db/dbx)     : " + firmware_verify("modkey"))
    print("  On real hardware this image is REJECTED with EFI_SECURITY_VIOLATION.")
    print("  Fix: import the certificate into db (or enroll it as a MOK).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
