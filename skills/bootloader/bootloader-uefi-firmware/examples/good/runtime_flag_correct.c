/* Correct: runtime variable with RUNTIME_ACCESS so the OS can read it
 * after ExitBootServices. The three attribute bits are non-negotiable for
 * a variable that must survive into the OS runtime environment.
 */
#include <Uefi.h>
#include <Library/UefiRuntimeServicesTableLib.h>

EFI_STATUS UefiMain(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    (void)ImageHandle;
    (void)SystemTable;

    UINT32 Value = 0xCAFE;
    // Correct: NON_VOLATILE persists across boots; RUNTIME_ACCESS keeps it
    // readable by the OS; BOOTSERVICE_ACCESS makes it readable in BS.
    return gRT->SetVariable(
        L"PersistentCounter", &gEfiGlobalVariableGuid,
        EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS |
        EFI_VARIABLE_RUNTIME_ACCESS,
        sizeof(Value), &Value);
}
