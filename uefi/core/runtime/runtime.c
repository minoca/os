/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    runtime.c

Abstract:

    This module implements the UEFI runtime driver core.

Author:

    Evan Green 10-Mar-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>
#include "uefifw.h"
#include "runtime.h"
#include "peimage.h"
#include <minoca/uefi/protocol/loadimg.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

EFIAPI
EFI_STATUS
EfiCoreConvertPointer (
    UINTN DebugDisposition,
    VOID **Address
    );

EFIAPI
EFI_STATUS
EfiCoreSetVirtualAddressMap (
    UINTN MemoryMapSize,
    UINTN DescriptorSize,
    UINT32 DescriptorVersion,
    EFI_MEMORY_DESCRIPTOR *VirtualMap
    );

VOID
EfipPeLoaderRelocateImageForRuntime (
    PHYSICAL_ADDRESS ImageBase,
    PHYSICAL_ADDRESS VirtualBase,
    UINTN ImageSize,
    VOID *RelocationData
    );

VOID
EfipRuntimeCalculateHeaderCrc (
    EFI_TABLE_HEADER *Header
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store the EFI runtime architectural protocol instance produced by this
// driver.
//

EFI_RUNTIME_ARCH_PROTOCOL EfiRuntimeProtocol;

//
// These globals are used while switching from physical to virtual mode.
//

EFI_MEMORY_DESCRIPTOR *EfiVirtualMap;
UINTN EfiVirtualMapCount;
UINTN EfiVirtualMapDescriptorSize;

//
// Store the handle onto which the runtime protocol is installed.
//

EFI_HANDLE EfiRuntimeHandle;

//
// Remember the image base of the runtime driver itself to avoid relocating it.
//

VOID *EfiRuntimeImageBase;

EFI_GUID EfiLoadedImageProtocolGuid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
EFI_GUID EfiRuntimeArchProtocolGuid = EFI_RUNTIME_ARCH_PROTOCOL_GUID;

EFI_BOOT_SERVICES *EfiBootServices;
EFI_RUNTIME_SERVICES *EfiRuntimeServices;
EFI_SYSTEM_TABLE *EfiSystemTable;

//
// ------------------------------------------------------------------ Functions
//

__USED
EFIAPI
EFI_STATUS
EfiRuntimeDriverEntry (
    EFI_HANDLE ImageHandle,
    EFI_SYSTEM_TABLE *SystemTable
    )

/*++

Routine Description:

    This routine implements the entry point into the runtime services driver.

Arguments:

    ImageHandle - Supplies the handle associated with this image.

    SystemTable - Supplies a pointer to the EFI system table.

Return Value:

    EFI_SUCCESS if the driver initialized successfully.

    Other status codes on failure.

--*/

{

    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
    EFI_STATUS Status;

    EfiSystemTable = SystemTable;
    EfiBootServices = SystemTable->BootServices;
    EfiRuntimeServices = SystemTable->RuntimeServices;
    EfiSetMem(&EfiRuntimeProtocol, sizeof(EFI_RUNTIME_ARCH_PROTOCOL), 0);
    INITIALIZE_LIST_HEAD(&(EfiRuntimeProtocol.ImageListHead));
    INITIALIZE_LIST_HEAD(&(EfiRuntimeProtocol.EventListHead));

    //
    // Artificially adjust the size of the memory descriptor to catch anybody
    // doing pointer arithmetic directly.
    //

    EfiRuntimeProtocol.MemoryDescriptorSize =
                              sizeof(EFI_MEMORY_DESCRIPTOR) + sizeof(UINT64) -
                              (sizeof(EFI_MEMORY_DESCRIPTOR) % sizeof(UINT64));

    EfiRuntimeProtocol.MemoryDescriptorVersion = EFI_MEMORY_DESCRIPTOR_VERSION;

    //
    // The image needs to be excluded from the list of images to relocate
    // during SetVirtualAddress map, so get the base address of this image
    // now.
    //

    Status = EfiHandleProtocol(ImageHandle,
                               &EfiLoadedImageProtocolGuid,
                               (VOID **)&LoadedImage);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    EfiRuntimeImageBase = LoadedImage->ImageBase;

    //
    // Take over certain boot and runtime services.
    //

    EfiBootServices->CalculateCrc32 = EfiCoreCalculateCrc32;
    EfiRuntimeServices->SetVirtualAddressMap = EfiCoreSetVirtualAddressMap;
    EfiRuntimeServices->ConvertPointer = EfiCoreConvertPointer;

    //
    // Install the Runtime Architectural Protocol onto a new handle.
    //

    EfiRuntimeHandle = NULL;
    Status = EfiInstallMultipleProtocolInterfaces(&EfiRuntimeHandle,
                                                  &EfiRuntimeArchProtocolGuid,
                                                  &EfiRuntimeProtocol,
                                                  NULL);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    return EFI_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

EFIAPI
EFI_STATUS
EfiCoreConvertPointer (
    UINTN DebugDisposition,
    VOID **Address
    )

/*++

Routine Description:

    This routine determines the new virtual address that is to be used on
    subsequent memory accesses.

Arguments:

    DebugDisposition - Supplies type information for the pointer being
        converted.

    Address - Supplies a pointer to a pointer that is to be fixed to be the
        value needed for the new virtual address mappings being applied.

Return Value:

    EFI_SUCCESS if the pointer was modified.

    EFI_INVALID_PARAMETER if the address is NULL or the value of Address is
    NULL and the debug disposition does not have the EFI_OPTIONAL_PTR bit set.

    EFI_NOT_FOUND if the pointer pointed to by the address parameter was not
    found to be part of the current memory map. This is normally fatal.

--*/

{

    UINTN ConvertAddress;
    UINT64 End;
    EFI_MEMORY_DESCRIPTOR *Entry;
    UINTN Index;

    if (Address == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    ConvertAddress = (UINTN)*Address;
    if (ConvertAddress == 0) {
        if ((DebugDisposition & EFI_OPTIONAL_PTR) != 0) {
            return EFI_SUCCESS;
        }

        return EFI_INVALID_PARAMETER;
    }

    Entry = EfiVirtualMap;
    for (Index = 0; Index < EfiVirtualMapCount; Index += 1) {
        if ((Entry->Attribute & EFI_MEMORY_RUNTIME) != 0) {
            if (ConvertAddress >= Entry->PhysicalStart) {
                End = Entry->PhysicalStart +
                      (Entry->NumberOfPages << EFI_PAGE_SHIFT);

                if (ConvertAddress < End) {
                    *Address = (VOID *)(ConvertAddress -
                                        (UINTN)(Entry->PhysicalStart) +
                                        (UINTN)(Entry->VirtualStart));

                    return EFI_SUCCESS;
                }
            }
        }

        Entry = (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)Entry +
                                          EfiVirtualMapDescriptorSize);
    }

    //
    // Bad news bears.
    //

    return EFI_NOT_FOUND;
}

EFIAPI
EFI_STATUS
EfiCoreSetVirtualAddressMap (
    UINTN MemoryMapSize,
    UINTN DescriptorSize,
    UINT32 DescriptorVersion,
    EFI_MEMORY_DESCRIPTOR *VirtualMap
    )

/*++

Routine Description:

    This routine changes the runtime addressing mode of EFI firmware from
    physical to virtual.

Arguments:

    MemoryMapSize - Supplies the size of the virtual map.

    DescriptorSize - Supplies the size of an entry in the virtual map.

    DescriptorVersion - Supplies the version of the structure entries in the
        virtual map.

    VirtualMap - Supplies the array of memory descriptors which contain the
        new virtual address mappings for all runtime ranges.

Return Value:

    EFI_SUCCESS on success.

    EFI_UNSUPPORTED if the firmware is not at runtime, or the firmware is
    already in virtual address mapped mode.

    EFI_INVALID_PARAMETER if the descriptor size or version is invalid.

    EFI_NO_MAPPING if the virtual address was not supplied for a range in the
    memory map that requires a mapping.

    EFI_NOT_FOUND if a virtual address was supplied for an address that is not
    found in the memory map.

--*/

{

    PLIST_ENTRY CurrentEntry;
    UINTN Remainder;
    EFI_RUNTIME_EVENT_ENTRY *RuntimeEvent;
    EFI_RUNTIME_IMAGE_ENTRY *RuntimeImage;
    EFI_PHYSICAL_ADDRESS VirtualBase;

    //
    // The switch to virtual mode can only happen once the memory map is locked
    // down, and it can only be set once.
    //

    if ((EfiRuntimeProtocol.AtRuntime == FALSE) ||
        (EfiRuntimeProtocol.VirtualMode != FALSE)) {

        return EFI_UNSUPPORTED;
    }

    if ((DescriptorVersion != EFI_MEMORY_DESCRIPTOR_VERSION) ||
        (DescriptorSize < sizeof(EFI_MEMORY_DESCRIPTOR))) {

        return EFI_INVALID_PARAMETER;
    }

    EfiRuntimeProtocol.VirtualMode = TRUE;

    //
    // Set up the globals for the convert pointer function. Avoid doing a
    // divide and pulling a bunch more junk in.
    //

    EfiVirtualMapDescriptorSize = DescriptorSize;
    Remainder = MemoryMapSize;
    EfiVirtualMapCount = 0;
    while (Remainder >= DescriptorSize) {
        Remainder -= DescriptorSize;
        EfiVirtualMapCount += 1;
    }

    EfiVirtualMap = VirtualMap;

    //
    // Signal all the virtual address change events.
    //

    CurrentEntry = EfiRuntimeProtocol.EventListHead.Next;
    while (CurrentEntry != &(EfiRuntimeProtocol.EventListHead)) {
        RuntimeEvent = LIST_VALUE(CurrentEntry,
                                  EFI_RUNTIME_EVENT_ENTRY,
                                  ListEntry);

        CurrentEntry = CurrentEntry->Next;
        if ((RuntimeEvent->Type & EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE) != 0) {
            RuntimeEvent->NotifyFunction(RuntimeEvent->Event,
                                         RuntimeEvent->NotifyContext);
        }
    }

    //
    // Relocate all the runtime images.
    //

    CurrentEntry = EfiRuntimeProtocol.ImageListHead.Next;
    while (CurrentEntry != &(EfiRuntimeProtocol.ImageListHead)) {
        RuntimeImage = LIST_VALUE(CurrentEntry,
                                  EFI_RUNTIME_IMAGE_ENTRY,
                                  ListEntry);

        CurrentEntry = CurrentEntry->Next;

        //
        // Skip relocating this image.
        //

        if (RuntimeImage->ImageBase == EfiRuntimeImageBase) {
            continue;
        }

        VirtualBase = (EFI_PHYSICAL_ADDRESS)(UINTN)(RuntimeImage->ImageBase);
        EfiCoreConvertPointer(0, (VOID **)&VirtualBase);
        EfipPeLoaderRelocateImageForRuntime(
                        (EFI_PHYSICAL_ADDRESS)(UINTN)(RuntimeImage->ImageBase),
                        VirtualBase,
                        (UINTN)(RuntimeImage->ImageSize),
                        RuntimeImage->RelocationData);

        EfiCoreInvalidateInstructionCacheRange(
                                             RuntimeImage->ImageBase,
                                             (UINTN)(RuntimeImage->ImageSize));
    }

    //
    // Convert all runtime services except ConvertPointer and
    // SetVirtualAddressMap, and recompute the CRC.
    //

    EfiCoreConvertPointer(0, (VOID **)&(EfiRuntimeServices->GetTime));
    EfiCoreConvertPointer(0, (VOID **)&(EfiRuntimeServices->SetTime));
    EfiCoreConvertPointer(0, (VOID **)&(EfiRuntimeServices->GetWakeupTime));
    EfiCoreConvertPointer(0, (VOID **)&(EfiRuntimeServices->SetWakeupTime));
    EfiCoreConvertPointer(0, (VOID **)&(EfiRuntimeServices->ResetSystem));
    EfiCoreConvertPointer(
                    0,
                    (VOID **)&(EfiRuntimeServices->GetNextHighMonotonicCount));

    EfiCoreConvertPointer(0, (VOID **)&(EfiRuntimeServices->GetVariable));
    EfiCoreConvertPointer(0, (VOID **)&(EfiRuntimeServices->SetVariable));
    EfiCoreConvertPointer(0,
                          (VOID **)&(EfiRuntimeServices->GetNextVariableName));

    EfiCoreConvertPointer(0, (VOID **)&(EfiRuntimeServices->QueryVariableInfo));
    EfiCoreConvertPointer(0, (VOID **)&(EfiRuntimeServices->UpdateCapsule));
    EfiCoreConvertPointer(
                     0,
                     (VOID **)&(EfiRuntimeServices->QueryCapsuleCapabilities));

    EfipRuntimeCalculateHeaderCrc(&(EfiRuntimeServices->Hdr));

    //
    // Convert the runtime fields of the system table and recompute the CRC.
    //

    EfiCoreConvertPointer(0, (VOID **)&(EfiSystemTable->FirmwareVendor));
    EfiCoreConvertPointer(0, (VOID **)&(EfiSystemTable->ConfigurationTable));
    EfiCoreConvertPointer(0, (VOID **)&(EfiSystemTable->RuntimeServices));
    EfiSystemTable->BootServices = NULL;
    EfipRuntimeCalculateHeaderCrc(&(EfiSystemTable->Hdr));
    EfiVirtualMap = NULL;
    return EFI_SUCCESS;
}

VOID
EfipPeLoaderRelocateImageForRuntime (
    PHYSICAL_ADDRESS ImageBase,
    PHYSICAL_ADDRESS VirtualBase,
    UINTN ImageSize,
    VOID *RelocationData
    )

/*++

Routine Description:

    This routine reapplies fixups on a PE32/PE32+ image so that it can be
    called from virtual mode.

Arguments:

    ImageBase - Supplies the base physical address where the PE image has
        been loaded.

    VirtualBase - Supplies the new virtual address where the PE image is
        going to execute in virtual mode.

    ImageSize - Supplies the size in bytes of the image.

    RelocationData - Supplies a pointer to the relocation data that was
        collected when the image was originally relocated.

Return Value:

    None.

--*/

{

    UINTN Adjust;
    EFI_IMAGE_DATA_DIRECTORY *DataDirectory;
    EFI_IMAGE_DOS_HEADER *DosHeader;
    CHAR8 *Fixup;
    UINT16 *Fixup16;
    UINT32 *Fixup32;
    UINT64 *Fixup64;
    CHAR8 *FixupBase;
    CHAR8 *FixupData;
    EFI_IMAGE_OPTIONAL_HEADER_PTR_UNION Header;
    UINT16 Magic;
    CHAR8 *NewBase;
    UINT32 NumberOfRvaAndSizes;
    CHAR8 *OldBase;
    UINT16 *Relocation;
    EFI_IMAGE_BASE_RELOCATION *RelocationBase;
    EFI_IMAGE_BASE_RELOCATION *RelocationBaseEnd;
    EFI_IMAGE_DATA_DIRECTORY *RelocationDirectory;
    UINT16 *RelocationEnd;

    OldBase = (CHAR8 *)((UINTN)ImageBase);
    NewBase = (CHAR8 *)((UINTN)VirtualBase);
    Adjust = (UINTN)NewBase - (UINTN)OldBase;

    //
    // Find the image's relocation directory. Start by finding the PE headers,
    // which are either directly at the beginning or pointed to by the DOS
    // header.
    //

    DosHeader = (EFI_IMAGE_DOS_HEADER *)OldBase;
    if (DosHeader->e_magic == EFI_IMAGE_DOS_SIGNATURE) {
        Header.Pe32 = (EFI_IMAGE_NT_HEADERS32 *)(((CHAR8 *)DosHeader) +
                                                 DosHeader->e_lfanew);

    } else {
        Header.Pe32 = (EFI_IMAGE_NT_HEADERS32 *)OldBase;
    }

    if (Header.Pe32->Signature != EFI_IMAGE_NT_SIGNATURE) {
        return;
    }

    Magic = Header.Pe32->OptionalHeader.Magic;
    if (Magic == EFI_IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        NumberOfRvaAndSizes = Header.Pe32->OptionalHeader.NumberOfRvaAndSizes;
        DataDirectory = &(Header.Pe32->OptionalHeader.DataDirectory[0]);

    } else {
        NumberOfRvaAndSizes =
                           Header.Pe32Plus->OptionalHeader.NumberOfRvaAndSizes;

        DataDirectory = &(Header.Pe32Plus->OptionalHeader.DataDirectory[0]);
    }

    //
    // Find the relocation block. It had better be there.
    //

    if (NumberOfRvaAndSizes <= EFI_IMAGE_DIRECTORY_ENTRY_BASERELOC) {
        return;
    }

    RelocationDirectory = DataDirectory + EFI_IMAGE_DIRECTORY_ENTRY_BASERELOC;
    RelocationBase =
        (EFI_IMAGE_BASE_RELOCATION *)(UINTN)(ImageBase +
                                          RelocationDirectory->VirtualAddress);

    RelocationBaseEnd =
        (EFI_IMAGE_BASE_RELOCATION *)((UINTN)RelocationBase +
                                      RelocationDirectory->Size);

    //
    // Run the whole relocation block. Re-fix up data that has not been
    // modified. The fixup data is used to see if the image has been modified
    // since it was relocated. Data sections that have been updated by code
    // will not be fixed up, since that would set them back to their defaults.
    //

    FixupData = RelocationData;
    while (RelocationBase < RelocationBaseEnd) {
        if ((RelocationBase->SizeOfBlock == 0) ||
            (RelocationBase->SizeOfBlock > RelocationDirectory->Size)) {

            return;
        }

        Relocation = (UINT16 *)((UINT8 *)RelocationBase +
                                sizeof(EFI_IMAGE_BASE_RELOCATION));

        RelocationEnd = (UINT16 *)((UINT8 *)RelocationBase +
                                   RelocationBase->SizeOfBlock);

        FixupBase = (CHAR8 *)((UINTN)ImageBase) +
                              RelocationBase->VirtualAddress;

        //
        // Run this relocation page.
        //

        while (Relocation < RelocationEnd) {
            Fixup = FixupBase + (*Relocation & 0x0FFF);
            switch (*Relocation >> 12) {
            case EFI_IMAGE_REL_BASED_ABSOLUTE:
                break;

            case EFI_IMAGE_REL_BASED_HIGH:
                Fixup16 = (UINT16 *)Fixup;
                if (*(UINT16 *)FixupData == *Fixup16) {
                    *Fixup16 =
                         (UINT16)(*Fixup16 + ((UINT16)((UINT32)Adjust >> 16)));
                }

                FixupData = FixupData + sizeof(UINT16);
                break;

            case EFI_IMAGE_REL_BASED_LOW:
                Fixup16 = (UINT16 *)Fixup;
                if (*(UINT16 *)FixupData == *Fixup16) {
                    *Fixup16 = (UINT16)(*Fixup16 +
                                        ((UINT16)((UINT32)Adjust & 0xFFFF)));
                }

                FixupData = FixupData + sizeof(UINT16);
                break;

            case EFI_IMAGE_REL_BASED_HIGHLOW:
                Fixup32 = (UINT32 *)Fixup;
                FixupData = ALIGN_POINTER(FixupData, sizeof(UINT32));
                if (*(UINT32 *)FixupData == *Fixup32) {
                    *Fixup32 = *Fixup32 + (UINT32)Adjust;
                }

                FixupData = FixupData + sizeof(UINT32);
                break;

            case EFI_IMAGE_REL_BASED_DIR64:
                Fixup64 = (UINT64 *)Fixup;
                FixupData = ALIGN_POINTER(FixupData, sizeof(UINT64));
                if (*(UINT64 *)FixupData == *Fixup64) {
                    *Fixup64 = *Fixup64 + (UINT64)Adjust;
                }

                FixupData = FixupData + sizeof(UINT64);
                break;

            default:
                break;
            }

            //
            // Move to the next relocation.
            //

            Relocation += 1;
        }

        //
        // Move to the next relocation block.
        //

        RelocationBase = (EFI_IMAGE_BASE_RELOCATION *)RelocationEnd;
    }

    return;
}

VOID
EfipRuntimeCalculateHeaderCrc (
    EFI_TABLE_HEADER *Header
    )

/*++

Routine Description:

    This routine recomputes the CRC of the given EFI table.

Arguments:

    Header - Supplies a pointer to the head of the table.

Return Value:

    None.

--*/

{

    UINT32 Crc;

    Header->CRC32 = 0;
    EfiCoreCalculateCrc32((UINT8 *)Header, Header->HeaderSize, &Crc);
    Header->CRC32 = Crc;
    return;
}

