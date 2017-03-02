/*++

Copyright (c) 2012 Minoca Corp.

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

    Evan Green 13-Oct-2012

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
#include "loader.h"
#include "paging.h"

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

/*++

Structure Description:

    This structure stores information about an allocation of virtual address
    space by the boot environment.

Members:

    PhysicalAddress - Stores the physical address of the memory backing the
        allocation.

    VirtualAddress - Stores the virtual address of the allocation.

--*/

typedef struct _BOOT_ADDRESS_SPACE_ALLOCATION {
    PHYSICAL_ADDRESS PhysicalAddress;
    PVOID VirtualAddress;
} BOOT_ADDRESS_SPACE_ALLOCATION, *PBOOT_ADDRESS_SPACE_ALLOCATION;

//
// ----------------------------------------------- Internal Function Prototypes
//

PVOID
BopImAllocateMemory (
    ULONG Size,
    ULONG Tag
    );

VOID
BopImFreeMemory (
    PVOID Allocation
    );

KSTATUS
BopImOpenFile (
    PVOID SystemContext,
    PCSTR BinaryName,
    PIMAGE_FILE_INFORMATION File
    );

VOID
BopImCloseFile (
    PIMAGE_FILE_INFORMATION File
    );

KSTATUS
BopImLoadFile (
    PIMAGE_FILE_INFORMATION File,
    PIMAGE_BUFFER Buffer
    );

VOID
BopImUnloadBuffer (
    PIMAGE_FILE_INFORMATION File,
    PIMAGE_BUFFER Buffer
    );

KSTATUS
BopImAllocateAddressSpace (
    PLOADED_IMAGE Image
    );

VOID
BopImFreeAddressSpace (
    PLOADED_IMAGE Image
    );

KSTATUS
BopImMapImageSegment (
    HANDLE AddressSpaceHandle,
    PVOID AddressSpaceAllocation,
    PIMAGE_FILE_INFORMATION File,
    ULONGLONG FileOffset,
    PIMAGE_SEGMENT Segment,
    PIMAGE_SEGMENT PreviousSegment
    );

VOID
BopImUnmapImageSegment (
    HANDLE AddressSpaceHandle,
    PIMAGE_SEGMENT Segment
    );

KSTATUS
BopImNotifyImageLoad (
    PLOADED_IMAGE Image
    );

VOID
BopImNotifyImageUnload (
    PLOADED_IMAGE Image
    );

VOID
BopImInvalidateInstructionCacheRegion (
    PVOID Address,
    ULONG Size
    );

PSTR
BopImGetEnvironmentVariable (
    PSTR Variable
    );

KSTATUS
BopImFinalizeSegments (
    HANDLE AddressSpaceHandle,
    PIMAGE_SEGMENT Segments,
    UINTN SegmentCount
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the boot device.
//

PBOOT_VOLUME BoBootDevice;

//
// Store the IDs of the directories to try when opening an image file.
//

FILE_ID BoDriversDirectoryId;
FILE_ID BoSystemDirectoryId;

//
// Define the image library function table.
//

IM_IMPORT_TABLE BoImageFunctionTable = {
    BopImAllocateMemory,
    BopImFreeMemory,
    BopImOpenFile,
    BopImCloseFile,
    BopImLoadFile,
    NULL,
    BopImUnloadBuffer,
    BopImAllocateAddressSpace,
    BopImFreeAddressSpace,
    BopImMapImageSegment,
    BopImUnmapImageSegment,
    BopImNotifyImageLoad,
    BopImNotifyImageUnload,
    BopImInvalidateInstructionCacheRegion,
    BopImGetEnvironmentVariable,
    BopImFinalizeSegments,
    NULL
};

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
BoInitializeImageSupport (
    PBOOT_VOLUME BootDevice,
    PBOOT_ENTRY BootEntry
    )

/*++

Routine Description:

    This routine initializes the image library for use in the boot
    environment.

Arguments:

    BootDevice - Supplies a pointer to the boot volume token, used for loading
        images from disk.

    BootEntry - Supplies a pointer to the boot entry being launched.

Return Value:

    Status code.

--*/

{

    PSTR DriversDirectoryPath;
    FILE_PROPERTIES Properties;
    KSTATUS Status;
    PCSTR SystemRootPath;

    INITIALIZE_LIST_HEAD(&BoLoadedImageList);

    //
    // Save the boot volume.
    //

    BoBootDevice = BootDevice;

    //
    // Open up the system root.
    //

    SystemRootPath = DEFAULT_SYSTEM_ROOT_PATH;
    if (BootEntry != NULL) {
        SystemRootPath = BootEntry->SystemPath;
    }

    Status = BoLookupPath(BootDevice, NULL, SystemRootPath, &Properties);
    if (!KSUCCESS(Status)) {
        goto InitializeImageSupportEnd;
    }

    BoSystemDirectoryId = Properties.FileId;

    //
    // Open up the drivers directory.
    //

    DriversDirectoryPath = DEFAULT_DRIVERS_DIRECTORY_PATH;
    Status = BoLookupPath(BootDevice,
                          &BoSystemDirectoryId,
                          DriversDirectoryPath,
                          &Properties);

    if (!KSUCCESS(Status)) {
        goto InitializeImageSupportEnd;
    }

    BoDriversDirectoryId = Properties.FileId;
    Status = ImInitialize(&BoImageFunctionTable);
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
BopImAllocateMemory (
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
BopImFreeMemory (
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
BopImOpenFile (
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
    ULONGLONG LocalFileSize;
    FILE_PROPERTIES Properties;
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

    Status = BoLookupPath(BoBootDevice,
                          &BoSystemDirectoryId,
                          BootFileHandle->FileName,
                          &Properties);

    if (Status == STATUS_PATH_NOT_FOUND) {
        Status = BoLookupPath(BoBootDevice,
                              &BoDriversDirectoryId,
                              BootFileHandle->FileName,
                              &Properties);
    }

    if (!KSUCCESS(Status)) {
        goto OpenFileEnd;
    }

    if (Properties.Type != IoObjectRegularFile) {
        Status = STATUS_FILE_IS_DIRECTORY;
        goto OpenFileEnd;
    }

    LocalFileSize = Properties.Size;
    if ((UINTN)LocalFileSize != LocalFileSize) {
        Status = STATUS_FILE_CORRUPT;
        goto OpenFileEnd;
    }

    BootFileHandle->FileSize = (UINTN)LocalFileSize;
    File->Size = LocalFileSize;
    File->ModificationDate = Properties.ModifiedTime.Seconds;
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
BopImCloseFile (
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
BopImLoadFile (
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
        Status = BoLoadFile(BoBootDevice,
                            &BoSystemDirectoryId,
                            BootFileHandle->FileName,
                            &(BootFileHandle->LoadedFileBuffer),
                            NULL,
                            NULL);

        if (Status == STATUS_PATH_NOT_FOUND) {
            Status = BoLoadFile(BoBootDevice,
                                &BoDriversDirectoryId,
                                BootFileHandle->FileName,
                                &(BootFileHandle->LoadedFileBuffer),
                                NULL,
                                NULL);
        }

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
BopImUnloadBuffer (
    PIMAGE_FILE_INFORMATION File,
    PIMAGE_BUFFER Buffer
    )

/*++

Routine Description:

    This routine unloads a file buffer created from either the load file or
    read file function, and frees the buffer.

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
BopImAllocateAddressSpace (
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
    PBOOT_ADDRESS_SPACE_ALLOCATION Allocation;
    UINTN PageOffset;
    UINTN PageSize;
    PVOID PreferredAddress;
    KSTATUS Status;

    Image->AllocatorHandle = INVALID_HANDLE;
    PreferredAddress = Image->PreferredLowestAddress;
    Allocation = BoAllocateMemory(sizeof(BOOT_ADDRESS_SPACE_ALLOCATION));
    if (Allocation == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto AllocateAddressSpaceEnd;
    }

    RtlZeroMemory(Allocation, sizeof(BOOT_ADDRESS_SPACE_ALLOCATION));
    PageSize = MmPageSize();
    PageOffset = (UINTN)PreferredAddress -
                 ALIGN_RANGE_DOWN((UINTN)PreferredAddress, PageSize);

    AlignedSize = ALIGN_RANGE_UP(Image->Size + PageOffset, PageSize);

    //
    // Allocate pages from the boot environment. This memory backs a boot
    // driver image, so it is marked as loader permanent.
    //

    Status = FwAllocatePages(&(Allocation->PhysicalAddress),
                             AlignedSize,
                             PageSize,
                             MemoryTypeLoaderPermanent);

    if (!KSUCCESS(Status)) {
        goto AllocateAddressSpaceEnd;
    }

    //
    // Map the memory to find out where it lands in virtual space.
    //

    Allocation->VirtualAddress = (PVOID)-1;
    Status = BoMapPhysicalAddress(&(Allocation->VirtualAddress),
                                  Allocation->PhysicalAddress,
                                  AlignedSize,
                                  MAP_FLAG_GLOBAL | MAP_FLAG_EXECUTE,
                                  MemoryTypeLoaderPermanent);

    if (!KSUCCESS(Status)) {
        goto AllocateAddressSpaceEnd;
    }

AllocateAddressSpaceEnd:
    if (!KSUCCESS(Status)) {
        if (Allocation != NULL) {
            BoFreeMemory(Allocation);
        }

    } else {
        Image->AllocatorHandle = Allocation;
        Image->LoadedImageBuffer = (PVOID)(UINTN)Allocation->PhysicalAddress +
                                   PageOffset;

        Image->BaseDifference = Allocation->VirtualAddress + PageOffset -
                                Image->PreferredLowestAddress;

        ASSERT((UINTN)Allocation->PhysicalAddress ==
               Allocation->PhysicalAddress);

    }

    return Status;
}

VOID
BopImFreeAddressSpace (
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

    BoFreeMemory((PBOOT_ADDRESS_SPACE_ALLOCATION)(Image->AllocatorHandle));
    return;
}

KSTATUS
BopImMapImageSegment (
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
    PVOID Destination;
    PBOOT_ADDRESS_SPACE_ALLOCATION Region;
    PVOID Source;

    Region = (PBOOT_ADDRESS_SPACE_ALLOCATION)AddressSpaceHandle;
    BootFileHandle = NULL;
    if (File != NULL) {
        BootFileHandle = (PBOOT_FILE_HANDLE)(File->Handle);
    }

    //
    // Copy to the physical address of the buffer plus the offset from the
    // base VA corresponding to that physical address.
    //

    ASSERT((UINTN)Region->PhysicalAddress == Region->PhysicalAddress);

    Destination = (PUCHAR)(UINTN)Region->PhysicalAddress +
                  (UINTN)Segment->VirtualAddress -
                  (UINTN)Region->VirtualAddress;

    //
    // Copy from the file buffer plus the given offset.
    //

    if (Segment->FileSize != 0) {

        ASSERT((BootFileHandle != NULL) &&
               (FileOffset + Segment->FileSize < BootFileHandle->FileSize));

        Source = (PUCHAR)(BootFileHandle->LoadedFileBuffer) + FileOffset;
        RtlCopyMemory(Destination, Source, Segment->FileSize);
    }

    if (Segment->MemorySize > Segment->FileSize) {
        RtlZeroMemory(Destination + Segment->FileSize,
                      Segment->MemorySize - Segment->FileSize);
    }

    return STATUS_SUCCESS;
}

VOID
BopImUnmapImageSegment (
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
BopImNotifyImageLoad (
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
BopImNotifyImageUnload (
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
BopImInvalidateInstructionCacheRegion (
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

PSTR
BopImGetEnvironmentVariable (
    PSTR Variable
    )

/*++

Routine Description:

    This routine gets an environment variable value for the image library.

Arguments:

    Variable - Supplies a pointer to a null terminated string containing the
        name of the variable to get.

Return Value:

    Returns a pointer to the value of the environment variable. The image
    library will not free or modify this value.

    NULL if the given environment variable is not set.

--*/

{

    if (RtlAreStringsEqual(Variable, IMAGE_LOAD_LIBRARY_PATH_VARIABLE, -1) !=
        FALSE) {

        return ".";
    }

    return NULL;
}

KSTATUS
BopImFinalizeSegments (
    HANDLE AddressSpaceHandle,
    PIMAGE_SEGMENT Segments,
    UINTN SegmentCount
    )

/*++

Routine Description:

    This routine applies the final memory protection attributes to the given
    segments. Read and execute bits can be applied at the time of mapping, but
    write protection may be applied here.

Arguments:

    AddressSpaceHandle - Supplies the handle used to claim the overall region
        of address space.

    Segments - Supplies the final array of segments.

    SegmentCount - Supplies the number of segments.

Return Value:

    Status code.

--*/

{

    UINTN End;
    ULONG MapFlags;
    UINTN PageSize;
    PIMAGE_SEGMENT Segment;
    UINTN SegmentIndex;
    UINTN Size;

    End = 0;
    PageSize = MmPageSize();
    for (SegmentIndex = 0; SegmentIndex < SegmentCount; SegmentIndex += 1) {
        Segment = &(Segments[SegmentIndex]);
        if (Segment->Type == ImageSegmentInvalid) {
            continue;
        }

        //
        // If the segment has no protection features, then there's nothing to
        // tighten up.
        //

        if ((Segment->Flags & IMAGE_MAP_FLAG_WRITE) != 0) {
            continue;
        }

        //
        // Compute the region whose protection should actually be changed.
        //

        End = (UINTN)(Segment->VirtualAddress) + Segment->MemorySize;
        End = ALIGN_RANGE_UP(End, PageSize);

        //
        // If the region has a real size, change its protection to read-only.
        //

        if (End > (UINTN)Segment->VirtualAddress) {
            Size = End - (UINTN)Segment->VirtualAddress;
            MapFlags = (MAP_FLAG_READ_ONLY << MAP_FLAG_PROTECT_SHIFT) |
                       MAP_FLAG_READ_ONLY;

            BoChangeMappingAttributes(Segment->VirtualAddress, Size, MapFlags);
        }
    }

    return STATUS_SUCCESS;
}

