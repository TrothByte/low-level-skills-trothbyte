/* Correct: Boot Services are used BEFORE ExitBootServices; after the
 * transition, only Runtime Services and Runtime-capable memory are used.
 * The pattern: gather everything needed (memory map, protocols) while the
 * Boot Services are alive, then call ExitBootServices last.
 */
#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

EFI_STATUS UefiMain(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    (void)SystemTable;

    EFI_STATUS Status;
    UINTN MemoryMapSize = 0;
    EFI_MEMORY_DESCRIPTOR *MemoryMap = NULL;
    EFI_UINTN MapKey = 0;
    EFI_UINTN DescriptorSize = 0;
    UINT32 DescriptorVersion = 0;

    // Correct: get the map size first (Boot Service, pre-exit).
    Status = gBS->GetMemoryMap(&MemoryMapSize, NULL, &MapKey,
                               &DescriptorSize, &DescriptorVersion);
    if (Status == EFI_BUFFER_TOO_SMALL) {
        // Correct: allocate and read the full map while BS is alive.
        Status = gBS->AllocatePool(EfiBootServicesData, MemoryMapSize,
                                   (VOID **)&MemoryMap);
        if (EFI_ERROR(Status)) {
            return Status;
        }
        Status = gBS->GetMemoryMap(&MemoryMapSize, MemoryMap, &MapKey,
                                   &DescriptorSize, &DescriptorVersion);
        if (EFI_ERROR(Status)) {
            return Status;
        }
    }

    // Correct: persist the boot count via a Runtime variable (available
    // both before and after exit).
    UINT32 BootCount = 1;
    Status = gRT->SetVariable(
        L"BootCount", &gEfiGlobalVariableGuid,
        EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS |
        EFI_VARIABLE_RUNTIME_ACCESS,
        sizeof(BootCount), &BootCount);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    // Correct: ExitBootServices is the LAST Boot Service call.
    Status = gBS->ExitBootServices(ImageHandle, MapKey);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    // Correct: after exit, use only Runtime Services. The memory map we
    // captured is EfiBootServicesData (still usable after exit while the
    // OS boots the trampoline, per PI spec; typical payload copies it to
    // runtime memory first).
    (void)MemoryMap;
    return EFI_SUCCESS;
}
