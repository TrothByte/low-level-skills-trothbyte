/*
 * wrong_database_use.c — intentionally incorrect: treats the KEK as the
 * image-signature database.
 *
 * The author believes that because the KEK (Key Exchange Key) is "the
 * master key that authorizes everything", an image signed by a KEK-held
 * certificate must be accepted by firmware. That is false on real UEFI:
 * image verification consults ONLY db/dbx. PK and KEK authorize updates to
 * the signature databases (SetVariable of db/dbx is signed by PK/KEK);
 * they never authorize a boot image. The naive check below prints
 * VERIFIED; the correct check (db/dbx only) rejects the same image, and on
 * a real machine the boot fails with EFI_SECURITY_VIOLATION.
 *
 * Fix: import the certificate into db (or enroll it as a MOK through shim),
 * never into PK/KEK.
 *
 * Build:  gcc -O2 -o wrong_database_use.exe wrong_database_use.c
 * Run:    ./wrong_database_use.exe
 */

#include <stdio.h>
#include <string.h>

enum key_db { DB_NONE = 0, DB_PK, DB_KEK, DB_DB, DB_DBX };

/* The module signing key "modkey" was wrongly enrolled into KEK only.
   The correct fix is enrollment into db (or as a MOK). */
static int enrolled_in(enum key_db target)
{
    switch (target) {
    case DB_PK:  return 0;
    case DB_KEK: return 1;   /* the mistake: modkey sits in KEK, not db */
    case DB_DB:  return 0;
    case DB_DBX: return 0;
    default:     return 0;
    }
}

/* Naive (wrong) check: assumes KEK membership authorizes a boot image. */
static const char *naive_verify(const char *signer)
{
    if (strcmp(signer, "modkey") != 0)
        return "UNSIGNED";
    return enrolled_in(DB_KEK) ? "VERIFIED (WRONG: KEK is not db)"
                               : "REJECTED";
}

/* Correct UEFI behavior: image verification consults only db and dbx. */
static const char *firmware_verify(const char *signer)
{
    if (enrolled_in(DB_DBX))
        return "REJECTED (dbx: revoked)";
    if (enrolled_in(DB_DB))
        return "VERIFIED (db)";
    return "REJECTED (signer not in db)";
}

int main(void)
{
    printf("wrong_database_use: KEK treated as the image-signature db\n");
    printf("  naive (KEK == db)     : %s\n", naive_verify("modkey"));
    printf("  firmware (db/dbx)     : %s\n", firmware_verify("modkey"));
    printf("  On real hardware this image is REJECTED with EFI_SECURITY_VIOLATION.\n");
    printf("  Fix: import the certificate into db (or enroll it as a MOK).\n");
    return 0;
}
