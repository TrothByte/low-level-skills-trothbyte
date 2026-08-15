// intentionally incorrect: calling Boot Services after ExitBootServices.
// Once ExitBootServices() returns, Boot Services (memory allocation,
// timer, protocol lookup via LocateProtocol) are gone. This code calls
// AllocatePool AFTER the exit — a guaranteed crash on real firmware.
// The comment even suggests it "should be fine" — it is not.
#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

EFI_STATUS UefiMain(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    (void)ImageHandle;
    (void)SystemTable;

    EFI_STATUS Status = gRT->SetVariable(
        L"BootCount", &gEfiGlobalVariableGuid,
        EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS |
        EFI_VARIABLE_RUNTIME_ACCESS,
        sizeof(UINT32), &(UINT32){1});
    if (EFI_ERROR(Status)) {
        return Status;
    }

    // WRONG: ExitBootServices is called, then a Boot Service is used.
    UINTN MemoryMapSize = 0;
    EFI_MEMORY_DESCRIPTOR *MemoryMap = NULL;
    EFI_UINTN MapKey = 0;
    Status = gBS->GetMemoryMap(&MemoryMapSize, MemoryMap, &MapKey,
                               NULL, NULL);
    if (Status == EFI_BUFFER_TOO_SMALL) {
        // WRONG: AllocatePool is a Boot Service — invalid after exit.
        Status = gBS->AllocatePool(EfiBootServicesData, MemoryMapSize,
                                   (VOID **)&MemoryMap);
        if (EFI_ERROR(Status)) {
            return Status;
        }
    }

    Status = gRT->ResetSystem(EfiResetCold, EFI_SUCCESS, 0, NULL);
    return Status;
}
