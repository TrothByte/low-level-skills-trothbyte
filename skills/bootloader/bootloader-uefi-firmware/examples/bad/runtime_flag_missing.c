// intentionally incorrect: runtime-flag mismatch for a variable.
// A variable that MUST be readable from the OS after ExitBootServices
// needs EFI_VARIABLE_RUNTIME_ACCESS. Setting it only as BOOTSERVICE_ACCESS
// makes GetVariable fail with EFI_NOT_FOUND after the OS takes over —
// the variable silently disappears from the runtime view. This is a
// classic UEFI authoring error (missing RUNTIME_ACCESS bit).
#include <Uefi.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiBootServicesTableLib.h>

EFI_STATUS UefiMain(EFI_Handle ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    (void)ImageHandle;
    (void)SystemTable;

    UINT32 Value = 0xCAFE;
    EFI_STATUS Status = gRT->SetVariable(
        L"PersistentCounter", &gEfiGlobalVariableGuid,
        EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS, // WRONG: no RUNTIME_ACCESS
        sizeof(Value), &Value);
    return Status;
}
