/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    bootim.c

Abstract:

    This module implements the underlying support routines for the image
    library to be run in the boot environment.

Author:

    Evan Green 21-Feb-2014

Environment:

    Boot

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/lib/fat/fat.h>
#include "firmware.h"
#include "bootlib.h"
#include "bootman.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores information about an opened file in the boot
    environment.

Members:

    FileName - Stores a pointer to the name of the file.

    FileNameSize - Stores the size of the file name string including the null
        terminator.

    LoadedFileBuffer - Stores a pointer to the buffer containing the file.

    FileSize - Stores the size of the loaded file, in bytes.

--*/

typedef struct _BOOT_FILE_HANDLE {
    PSTR FileName;
    ULONG FileNameSize;
    PVOID LoadedFileBuffer;
    UINTN FileSize;
} BOOT_FILE_HANDLE, *PBOOT_FILE_HANDLE;

//
// ----------------------------------------------- Internal Function Prototypes
//

PVOID
BmpImAllocateMemory (
    ULONG Size,
    ULONG Tag
    );

VOID
BmpImFreeMemory (
    PVOID Allocation
    );

KSTATUS
BmpImOpenFile (
    PVOID SystemContext,
    PCSTR BinaryName,
    PIMAGE_FILE_INFORMATION File
    );

VOID
BmpImCloseFile (
    PIMAGE_FILE_INFORMATION File
    );

KSTATUS
BmpImLoadFile (
    PIMAGE_FILE_INFORMATION File,
    PIMAGE_BUFFER Buffer
    );

VOID
BmpImUnloadBuffer (
    PIMAGE_FILE_INFORMATION File,
    PIMAGE_BUFFER Buffer
    );

KSTATUS
BmpImAllocateAddressSpace (
    PLOADED_IMAGE Image
    );

VOID
BmpImFreeAddressSpace (
    PLOADED_IMAGE Image
    );

KSTATUS
BmpImMapImageSegment (
    HANDLE AddressSpaceHandle,
    PVOID AddressSpaceAllocation,
    PIMAGE_FILE_INFORMATION File,
    ULONGLONG FileOffset,
    PIMAGE_SEGMENT Segment,
    PIMAGE_SEGMENT PreviousSegment
    );

VOID
BmpImUnmapImageSegment (
    HANDLE AddressSpaceHandle,
    PIMAGE_SEGMENT Segment
    );

KSTATUS
BmpImNotifyImageLoad (
    PLOADED_IMAGE Image
    );

VOID
BmpImNotifyImageUnload (
    PLOADED_IMAGE Image
    );

VOID
BmpImInvalidateInstructionCacheRegion (
    PVOID Address,
    ULONG Size
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the boot device.
//

PVOID BmBootDevice;

//
// Store a pointer to the selected boot entry.
//

PBOOT_ENTRY BmBootEntry;

//
// Store the IDs of the directories to try when opening an image file.
//

FILE_ID BmSystemDirectoryId;

//
// Define the image library function table.
//

IM_IMPORT_TABLE BmImageFunctionTable = {
    BmpImAllocateMemory,
    BmpImFreeMemory,
    BmpImOpenFile,
    BmpImCloseFile,
    BmpImLoadFile,
    NULL,
    BmpImUnloadBuffer,
    BmpImAllocateAddressSpace,
    BmpImFreeAddressSpace,
    BmpImMapImageSegment,
    BmpImUnmapImageSegment,
    BmpImNotifyImageLoad,
    BmpImNotifyImageUnload,
    BmpImInvalidateInstructionCacheRegion,
    NULL,
    NULL,
    NULL
};

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
BmpInitializeImageSupport (
    PVOID BootDevice,
    PBOOT_ENTRY BootEntry
    )

/*++

Routine Description:

    This routine initializes the image library for use in the boot manager.

Arguments:

    BootDevice - Supplies a pointer to the boot volume token, used for loading
        images from disk.

    BootEntry - Supplies a pointer to the selected boot entry.

Return Value:

    Status code.

--*/

{

    FILE_PROPERTIES Properties;
    KSTATUS Status;

    INITIALIZE_LIST_HEAD(&BmLoadedImageList);
    BmBootDevice = BootDevice;
    BmBootEntry = BootEntry;

    //
    // Open up the directories to search.
    //

    Status = BoLookupPath(BmBootDevice,
                          NULL,
                          BootEntry->SystemPath,
                          &Properties);

    if (!KSUCCESS(Status)) {
        goto InitializeImageSupportEnd;
    }

    BmSystemDirectoryId = Properties.FileId;
    Status = ImInitialize(&BmImageFunctionTable);
    if (!KSUCCESS(Status)) {
        goto InitializeImageSupportEnd;
    }

    Status = STATUS_SUCCESS;

InitializeImageSupportEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

PVOID
BmpImAllocateMemory (
    ULONG Size,
    ULONG Tag
    )

/*++

Routine Description:

    This routine allocates memory from the boot environment for the image
    library.

Arguments:

    Size - Supplies the number of bytes required for the memory allocation.

    Tag - Supplies a 32-bit ASCII identifier used to tag the memroy allocation.

Return Value:

    Returns a pointer to the memory allocation on success.

    NULL on failure.

--*/

{

    return BoAllocateMemory(Size);
}

VOID
BmpImFreeMemory (
    PVOID Allocation
    )

/*++

Routine Description:

    This routine frees memory to the boot environment allocated by the image
    library.

Arguments:

    Allocation - Supplies a pointer the allocation to free.

Return Value:

    None.

--*/

{

    BoFreeMemory(Allocation);
    return;
}

KSTATUS
BmpImOpenFile (
    PVOID SystemContext,
    PCSTR BinaryName,
    PIMAGE_FILE_INFORMATION File
    )

/*++

Routine Description:

    This routine opens a file.

Arguments:

    SystemContext - Supplies the context pointer passed to the load executable
        function.

    BinaryName - Supplies the name of the executable image to open.

    File - Supplies a pointer where the information for the file including its
        open handle will be returned.

Return Value:

    Status code.

--*/

{

    PBOOT_FILE_HANDLE BootFileHandle;
    KSTATUS Status;

    BootFileHandle = BoAllocateMemory(sizeof(BOOT_FILE_HANDLE));
    if (BootFileHandle == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto OpenFileEnd;
    }

    RtlZeroMemory(BootFileHandle, sizeof(BOOT_FILE_HANDLE));
    BootFileHandle->FileNameSize = RtlStringLength(BinaryName) + 1;
    BootFileHandle->FileName = BoAllocateMemory(BootFileHandle->FileNameSize);
    if (BootFileHandle->FileName == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto OpenFileEnd;
    }

    RtlCopyMemory(BootFileHandle->FileName,
                  BinaryName,
                  BootFileHandle->FileNameSize);

    //
    // Open the file enough to make sure it's there, but don't actually load
    // it just now.
    //

    Status = BoLoadFile(BmBootDevice,
                        &BmSystemDirectoryId,
                        BootFileHandle->FileName,
                        NULL,
                        &(BootFileHandle->FileSize),
                        &(File->ModificationDate));

    if (!KSUCCESS(Status)) {
        goto OpenFileEnd;
    }

    File->Size = BootFileHandle->FileSize;
    File->DeviceId = 0;
    File->FileId = 0;

OpenFileEnd:
    if (!KSUCCESS(Status)) {
        if (BootFileHandle != NULL) {
            if (BootFileHandle->FileName != NULL) {
                BoFreeMemory(BootFileHandle->FileName);
            }

            BoFreeMemory(BootFileHandle);
            BootFileHandle = INVALID_HANDLE;
        }
    }

    File->Handle = BootFileHandle;
    return Status;
}

VOID
BmpImCloseFile (
    PIMAGE_FILE_INFORMATION File
    )

/*++

Routine Description:

    This routine closes an open file, invalidating any memory mappings to it.

Arguments:

    File - Supplies a pointer to the file information.

Return Value:

    None.

--*/

{

    PBOOT_FILE_HANDLE BootFileHandle;

    BootFileHandle = (PBOOT_FILE_HANDLE)(File->Handle);

    ASSERT(BootFileHandle->LoadedFileBuffer == NULL);

    if (BootFileHandle->FileName != NULL) {
        BoFreeMemory(BootFileHandle->FileName);
    }

    BoFreeMemory(BootFileHandle);
    return;
}

KSTATUS
BmpImLoadFile (
    PIMAGE_FILE_INFORMATION File,
    PIMAGE_BUFFER Buffer
    )

/*++

Routine Description:

    This routine loads an entire file into memory so the image library can
    access it.

Arguments:

    File - Supplies a pointer to the file information.

    Buffer - Supplies a pointer where the buffer will be returned on success.

Return Value:

    Status code.

--*/

{

    PBOOT_FILE_HANDLE BootFileHandle;
    KSTATUS Status;

    BootFileHandle = (PBOOT_FILE_HANDLE)(File->Handle);
    if (BootFileHandle->LoadedFileBuffer == NULL) {
        Status = BoLoadFile(BmBootDevice,
                            &BmSystemDirectoryId,
                            BootFileHandle->FileName,
                            &(BootFileHandle->LoadedFileBuffer),
                            NULL,
                            NULL);

        if (!KSUCCESS(Status)) {
            goto ImLoadFileEnd;
        }
    }

    Status = STATUS_SUCCESS;

ImLoadFileEnd:
    if (KSUCCESS(Status)) {
        Buffer->Data = BootFileHandle->LoadedFileBuffer;
        Buffer->Size = BootFileHandle->FileSize;
    }

    return Status;
}

VOID
BmpImUnloadBuffer (
    PIMAGE_FILE_INFORMATION File,
    PIMAGE_BUFFER Buffer
    )

/*++

Routine Description:

    This routine unloads a file and frees the buffer associated with a load
    image call.

Arguments:

    File - Supplies a pointer to the file information.

    Buffer - Supplies the buffer returned by the load file function.

Return Value:

    None.

--*/

{

    PBOOT_FILE_HANDLE BootFileHandle;

    BootFileHandle = (PBOOT_FILE_HANDLE)(File->Handle);
    if (BootFileHandle->LoadedFileBuffer != NULL) {
        BoFreeMemory(BootFileHandle->LoadedFileBuffer);
        BootFileHandle->LoadedFileBuffer = NULL;
    }

    return;
}

KSTATUS
BmpImAllocateAddressSpace (
    PLOADED_IMAGE Image
    )

/*++

Routine Description:

    This routine allocates a section of virtual address space that an image
    can be mapped in to.

Arguments:

    Image - Supplies a pointer to the image being loaded. The system context,
        size, file information, load flags, and preferred virtual address will
        be initialized. This routine should set up the loaded image buffer,
        loaded lowest address, and allocator handle if needed.

Return Value:

    Status code.

--*/

{

    UINTN AlignedSize;
    UINTN PageOffset;
    UINTN PageSize;
    PHYSICAL_ADDRESS PhysicalAddress;
    PVOID PreferredAddress;
    KSTATUS Status;

    PreferredAddress = Image->PreferredLowestAddress;
    PageSize = MmPageSize();
    PageOffset = (UINTN)PreferredAddress -
                 ALIGN_RANGE_DOWN((UINTN)PreferredAddress, PageSize);

    AlignedSize = ALIGN_RANGE_UP(Image->Size + PageOffset, PageSize);

    //
    // Allocate pages from the boot environment. This memory backs a boot
    // application image, so it is marked as loader temporary.
    //

    Status = FwAllocatePages(&PhysicalAddress,
                             AlignedSize,
                             PageSize,
                             MemoryTypeLoaderTemporary);

    if (!KSUCCESS(Status)) {
        goto AllocateAddressSpaceEnd;
    }

    ASSERT((UINTN)PhysicalAddress == PhysicalAddress);

    Image->BaseDifference = (PhysicalAddress + PageOffset) -
                            (UINTN)PreferredAddress;

    Image->LoadedImageBuffer = (PVOID)(UINTN)(PhysicalAddress + PageOffset);

AllocateAddressSpaceEnd:
    return Status;
}

VOID
BmpImFreeAddressSpace (
    PLOADED_IMAGE Image
    )

/*++

Routine Description:

    This routine frees a section of virtual address space that was previously
    allocated.

Arguments:

    Image - Supplies a pointer to the loaded (or partially loaded) image.

Return Value:

    None.

--*/

{

    return;
}

KSTATUS
BmpImMapImageSegment (
    HANDLE AddressSpaceHandle,
    PVOID AddressSpaceAllocation,
    PIMAGE_FILE_INFORMATION File,
    ULONGLONG FileOffset,
    PIMAGE_SEGMENT Segment,
    PIMAGE_SEGMENT PreviousSegment
    )

/*++

Routine Description:

    This routine maps a section of the image to the given virtual address.

Arguments:

    AddressSpaceHandle - Supplies the handle used to claim the overall region
        of address space.

    AddressSpaceAllocation - Supplies the original lowest virtual address for
        this image.

    File - Supplies an optional pointer to the file being mapped. If this
        parameter is NULL, then a zeroed memory section is being mapped.

    FileOffset - Supplies the offset from the beginning of the file to the
        beginning of the mapping, in bytes.

    Segment - Supplies a pointer to the segment information to map. On output,
        the virtual address will contain the actual mapped address, and the
        mapping handle may be set.

    PreviousSegment - Supplies an optional pointer to the previous segment
        that was mapped, so this routine can handle overlap appropriately. This
        routine can assume that segments are always mapped in increasing order.

Return Value:

    Status code.

--*/

{

    PBOOT_FILE_HANDLE BootFileHandle;
    PVOID Source;

    BootFileHandle = NULL;
    if (File != NULL) {
        BootFileHandle = (PBOOT_FILE_HANDLE)(File->Handle);
    }

    //
    // Copy from the file buffer plus the given offset.
    //

    if (Segment->FileSize != 0) {

        ASSERT((BootFileHandle != NULL) &&
               (FileOffset + Segment->FileSize < BootFileHandle->FileSize));

        Source = (PUCHAR)(BootFileHandle->LoadedFileBuffer) + FileOffset;
        RtlCopyMemory(Segment->VirtualAddress, Source, Segment->FileSize);
    }

    if (Segment->MemorySize > Segment->FileSize) {
        RtlZeroMemory(Segment->VirtualAddress + Segment->FileSize,
                      Segment->MemorySize - Segment->FileSize);
    }

    return STATUS_SUCCESS;
}

VOID
BmpImUnmapImageSegment (
    HANDLE AddressSpaceHandle,
    PIMAGE_SEGMENT Segment
    )

/*++

Routine Description:

    This routine maps unmaps an image segment.

Arguments:

    AddressSpaceHandle - Supplies the handle used to claim the overall region
        of address space.

    Segment - Supplies a pointer to the segment information to unmap.

Return Value:

    None.

--*/

{

    //
    // Unmapping is currently not implemented.
    //

    return;
}

KSTATUS
BmpImNotifyImageLoad (
    PLOADED_IMAGE Image
    )

/*++

Routine Description:

    This routine notifies the primary consumer of the image library that an
    image has been loaded.

Arguments:

    Image - Supplies the image that has just been loaded. This image should
        be subsequently returned to the image library upon requests for loaded
        images with the given name.

Return Value:

    Status code. Failing status codes veto the image load.

--*/

{

    ULONG AllocationSize;
    PSTR FileName;
    PDEBUG_MODULE LoadedModule;
    ULONG NameSize;
    KSTATUS Status;

    FileName = RtlStringFindCharacterRight(Image->FileName, '/', -1);
    if (FileName != NULL) {
        FileName += 1;

    } else {
        FileName = Image->FileName;
    }

    NameSize = RtlStringLength(FileName) + 1;
    AllocationSize = sizeof(DEBUG_MODULE) +
                     ((NameSize - ANYSIZE_ARRAY) * sizeof(CHAR));

    LoadedModule = BoAllocateMemory(AllocationSize);
    if (LoadedModule == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto NotifyImageLoadEnd;
    }

    RtlZeroMemory(LoadedModule, AllocationSize);

    //
    // Initialize the loaded image parameters.
    //

    RtlStringCopy(LoadedModule->BinaryName, FileName, NameSize);
    LoadedModule->StructureSize = AllocationSize;
    LoadedModule->Timestamp = Image->File.ModificationDate;
    LoadedModule->LowestAddress = Image->PreferredLowestAddress +
                                  Image->BaseDifference;

    LoadedModule->Size = Image->Size;
    LoadedModule->EntryPoint = Image->EntryPoint;
    LoadedModule->Image = Image;
    Image->DebuggerModule = LoadedModule;
    KdReportModuleChange(LoadedModule, TRUE);
    Status = STATUS_SUCCESS;

NotifyImageLoadEnd:
    if (!KSUCCESS(Status)) {
        if (LoadedModule != NULL) {
            BoFreeMemory(LoadedModule);
        }
    }

    return Status;
}

VOID
BmpImNotifyImageUnload (
    PLOADED_IMAGE Image
    )

/*++

Routine Description:

    This routine notifies the primary consumer of the image library that an
    image is about to be unloaded from memory. Once this routine returns, the
    image should not be referenced again as it will be freed.

Arguments:

    Image - Supplies the image that is about to be unloaded.

Return Value:

    None.

--*/

{

    PDEBUG_MODULE UnloadingModule;

    UnloadingModule = Image->DebuggerModule;
    Image->DebuggerModule = NULL;
    KdReportModuleChange(UnloadingModule, FALSE);
    BoFreeMemory(UnloadingModule);
    return;
}

VOID
BmpImInvalidateInstructionCacheRegion (
    PVOID Address,
    ULONG Size
    )

/*++

Routine Description:

    This routine invalidates an instruction cache region after code has been
    modified.

Arguments:

    Address - Supplies the virtual address of the revion to invalidate.

    Size - Supplies the number of bytes to invalidate.

Return Value:

    None.

--*/

{

    return;
}

