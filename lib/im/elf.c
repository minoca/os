/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    elf.c

Abstract:

    This module implements support for handling the ELF file format.

Author:

    Evan Green 13-Oct-2012

Environment:

    Kernel/Boot/Build

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "imp.h"
#include "elf.h"

//
// ---------------------------------------------------------------- Definitions
//

#define MAX_SECTION_NAME 8

//
// Try some magically built-in library paths.
//

#define ELF_BUILTIN_LIBRARY_PATH "/lib:/usr/lib:/usr/local/lib"

//
// Define an invalid address value for image relocation tracking.
//

#define ELF_INVALID_RELOCATION (PVOID)-1

//
// Define the maximum number of program headers before it's just silly.
//

#define ELF_MAX_PROGRAM_HEADERS 50

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _ELF_LIBRARY_PATH_VARIABLE {
    ElfLibraryPathOrigin,
    ElfLibraryPathLib,
    ElfLibraryPathPlatform
} ELF_LIBRARY_PATH_VARIABLE, *PELF_LIBRARY_PATH_VARIABLE;

/*++

Structure Description:

    This structure stores an entry in the table of variables that can be
    substituted in ELF library paths.

Members:

    Variable - Stores the variable code.

    Name - Stores the variable name.

--*/

typedef struct _ELF_LIBRARY_PATH_VARIABLE_ENTRY {
    ELF_LIBRARY_PATH_VARIABLE Variable;
    PSTR Name;
} ELF_LIBRARY_PATH_VARIABLE_ENTRY, *PELF_LIBRARY_PATH_VARIABLE_ENTRY;

/*++

Structure Description:

    This structure stores state variables used while loading an ELF image.

Members:

    Buffer - Stores the loaded image buffer.

    ElfHeader - Stores a pointer pointing inside the file buffer where the
        main ELF header resides.

    DynamicSection - Stores a pointer pointing inside the loaded image buffer
        where the dynamic section begins.

    DynamicEntryCount - Stores the number of elements in the dynamic section.

    RelocationStart - Stores the lowest address to be modified during image
        relocation.

    RelocationEnd - Stores the address at the end of the highest image
        relocation.

--*/

typedef struct _ELF_LOADING_IMAGE {
    IMAGE_BUFFER Buffer;
    PELF32_HEADER ElfHeader;
    PELF32_DYNAMIC_ENTRY DynamicSection;
    ULONG DynamicEntryCount;
    PVOID RelocationStart;
    PVOID RelocationEnd;
} ELF_LOADING_IMAGE, *PELF_LOADING_IMAGE;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
ImpElfLoadImportsForImage (
    PLIST_ENTRY ListHead,
    PLOADED_IMAGE Image
    );

KSTATUS
ImpElfLoadImport (
    PLOADED_IMAGE Image,
    PLIST_ENTRY ListHead,
    PSTR LibraryName,
    PLOADED_IMAGE *Import
    );

KSTATUS
ImpElfLoadImportWithPath (
    PLOADED_IMAGE Image,
    PLIST_ENTRY ListHead,
    PSTR LibraryName,
    PSTR Path,
    PLOADED_IMAGE *Import
    );

KSTATUS
ImpElfGatherExportInformation (
    PLOADED_IMAGE Image,
    BOOL UseLoadedAddress
    );

KSTATUS
ImpElfGetDynamicEntry (
    PELF_LOADING_IMAGE LoadingImage,
    ULONG Tag,
    PELF32_DYNAMIC_ENTRY *FoundEntry
    );

KSTATUS
ImpElfRelocateImage (
    PLIST_ENTRY ListHead,
    PLOADED_IMAGE Image
    );

KSTATUS
ImpElfProcessRelocateSection (
    PLIST_ENTRY ListHead,
    PLOADED_IMAGE Image,
    PVOID Relocations,
    UINTN RelocationsSize,
    BOOL Addends
    );

VOID
ImpElfAdjustJumpSlots (
    PLOADED_IMAGE Image,
    PVOID Relocations,
    UINTN RelocationsSize,
    BOOL Addends
    );

ULONG
ImpElfGetSymbolValue (
    PLIST_ENTRY ListHead,
    PLOADED_IMAGE Image,
    PELF32_SYMBOL Symbol,
    PLOADED_IMAGE *FoundImage,
    PLOADED_IMAGE SkipImage
    );

PELF32_SYMBOL
ImpElfGetSymbol (
    PLOADED_IMAGE Image,
    ULONG Hash,
    PSTR SymbolName
    );

BOOL
ImpElfApplyRelocation (
    PLIST_ENTRY ListHead,
    PLOADED_IMAGE Image,
    PELF32_RELOCATION_ADDEND_ENTRY RelocationEntry,
    BOOL AddendEntry,
    PVOID *FinalSymbolValue
    );

ULONG
ImpElfOriginalHash (
    PSTR SymbolName
    );

ULONG
ImpElfGnuHash (
    PSTR SymbolName
    );

PSTR
ImpElfGetEnvironmentVariable (
    PSTR Variable
    );

KSTATUS
ImpElfPerformLibraryPathSubstitutions (
    PLOADED_IMAGE Image,
    PSTR *Path,
    PUINTN PathCapacity
    );

VOID
ImpElfFreeContext (
    PLOADED_IMAGE Image
    );

//
// -------------------------------------------------------------------- Globals
//

ELF_LIBRARY_PATH_VARIABLE_ENTRY ElfLibraryPathVariables[] = {
    {ElfLibraryPathOrigin, "ORIGIN"},
    {ElfLibraryPathLib, "LIB"},
    {ElfLibraryPathPlatform, "PLATFORM"}
};

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
ImpElfGetImageSize (
    PLIST_ENTRY ListHead,
    PLOADED_IMAGE Image,
    PIMAGE_BUFFER Buffer,
    PSTR *InterpreterPath
    )

/*++

Routine Description:

    This routine determines the size of an ELF executable image. The image size,
    preferred lowest address, and relocatable flag will all be filled in.

Arguments:

    ListHead - Supplies a pointer to the head of the list of loaded images.

    Image - Supplies a pointer to the image to get the size of.

    Buffer - Supplies a pointer to the loaded image buffer.

    InterpreterPath - Supplies a pointer where the interpreter name will be
        returned if the program is requesting an interpreter.

Return Value:

    Returns the size of the expanded image in memory on success.

    0 on failure.

--*/

{

    PELF32_HEADER ElfHeader;
    PELF32_PROGRAM_HEADER FirstProgramHeader;
    ULONG HeaderSize;
    UINTN HighestVirtualAddress;
    UINTN ImageSize;
    PSTR InterpreterName;
    UINTN LowestVirtualAddress;
    PELF32_PROGRAM_HEADER ProgramHeader;
    BOOL Result;
    UINTN SegmentBase;
    ULONG SegmentCount;
    UINTN SegmentEnd;
    ULONG SegmentIndex;
    KSTATUS Status;

    ImageSize = 0;
    *InterpreterPath = NULL;
    Status = STATUS_UNKNOWN_IMAGE_FORMAT;

    //
    // Get the ELF headers.
    //

    Result = ImpElfGetHeader(Buffer, &ElfHeader);
    if (Result == FALSE) {
        goto GetImageSizeEnd;
    }

    SegmentCount = ElfHeader->ProgramHeaderCount;
    if (SegmentCount > ELF_MAX_PROGRAM_HEADERS) {
        goto GetImageSizeEnd;
    }

    FirstProgramHeader = ImpReadBuffer(
                                  &(Image->File),
                                  Buffer,
                                  ElfHeader->ProgramHeaderOffset,
                                  ElfHeader->ProgramHeaderSize * SegmentCount);

    if (FirstProgramHeader == NULL) {
        goto GetImageSizeEnd;
    }

    if (ElfHeader->ImageType == ELF_IMAGE_SHARED_OBJECT) {
        Image->Flags |= IMAGE_FLAG_RELOCATABLE;

    } else if (ElfHeader->ImageType == ELF_IMAGE_EXECUTABLE) {
        Image->Flags &= ~IMAGE_FLAG_RELOCATABLE;

    } else {
        Status = STATUS_UNKNOWN_IMAGE_FORMAT;
        goto GetImageSizeEnd;
    }

    //
    // Loop through the program headers once to get the image size and base
    // address.
    //

    LowestVirtualAddress = (UINTN)-1;
    HighestVirtualAddress = 0;
    for (SegmentIndex = 0; SegmentIndex < SegmentCount; SegmentIndex += 1) {
        ProgramHeader =
            (PELF32_PROGRAM_HEADER)(((PUCHAR)FirstProgramHeader) +
                                (SegmentIndex * ElfHeader->ProgramHeaderSize));

        //
        // If this image is requesting an interpreter, go load the interpreter
        // instead of this image.
        //

        if ((ProgramHeader->Type == ELF_SEGMENT_TYPE_INTERPRETER) &&
            (ProgramHeader->FileSize != 0) &&
            ((Image->LoadFlags & IMAGE_LOAD_FLAG_IGNORE_INTERPRETER) == 0)) {

            ASSERT(Image->ImportDepth == 0);

            HeaderSize = ProgramHeader->FileSize;
            InterpreterName = ImpReadBuffer(&(Image->File),
                                            Buffer,
                                            ProgramHeader->Offset,
                                            HeaderSize);

            if ((InterpreterName == NULL) ||
                (HeaderSize == 0) ||
                (InterpreterName[HeaderSize - 1] != '\0')) {

                Status = STATUS_UNKNOWN_IMAGE_FORMAT;
                goto GetImageSizeEnd;
            }

            *InterpreterPath = InterpreterName;
        }

        //
        // Skip non-loading segments.
        //

        if (ProgramHeader->Type != ELF_SEGMENT_TYPE_LOAD) {
            continue;
        }

        //
        // Determine where in memory this segment would start and end.
        //

        SegmentBase = ProgramHeader->VirtualAddress;
        SegmentEnd = ProgramHeader->VirtualAddress + ProgramHeader->MemorySize;

        //
        // Update the lowest and highest addresses seen so far.
        //

        if (SegmentBase < LowestVirtualAddress) {
            LowestVirtualAddress = SegmentBase;
        }

        if (SegmentEnd > HighestVirtualAddress) {
            HighestVirtualAddress = SegmentEnd;
        }
    }

    if (LowestVirtualAddress >= HighestVirtualAddress) {
        Status = STATUS_UNKNOWN_IMAGE_FORMAT;
        goto GetImageSizeEnd;
    }

    ImageSize = HighestVirtualAddress - LowestVirtualAddress;
    Image->PreferredLowestAddress = (PVOID)LowestVirtualAddress;
    Status = STATUS_SUCCESS;

GetImageSizeEnd:
    Image->Size = ImageSize;
    return Status;
}

KSTATUS
ImpElfLoadImage (
    PLIST_ENTRY ListHead,
    PLOADED_IMAGE Image,
    PIMAGE_BUFFER Buffer,
    ULONG ImportDepth
    )

/*++

Routine Description:

    This routine loads an ELF image into its executable form.

Arguments:

    ListHead - Supplies a pointer to the head of the list of loaded images.

    Image - Supplies a pointer to the loaded image. This must be partially
        filled out. Notable fields that must be filled out by the caller
        include the loaded virtual address and image size. This routine will
        fill out many other fields.

    Buffer - Supplies a pointer to the image buffer.

    ImportDepth - Supplies the import depth to assign to the image.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_FILE_CORRUPT if the file headers were corrupt or unexpected.

    Other errors on failure.

--*/

{

    UINTN BaseDifference;
    PELF32_HEADER ElfHeader;
    PELF32_PROGRAM_HEADER FirstProgramHeader;
    BOOL ImageInserted;
    ULONG ImportIndex;
    PELF_LOADING_IMAGE LoadingImage;
    BOOL NotifyLoadCalled;
    PIMAGE_SEGMENT PreviousSegment;
    PELF32_PROGRAM_HEADER ProgramHeader;
    BOOL Result;
    PIMAGE_SEGMENT Segment;
    UINTN SegmentBase;
    ULONG SegmentCount;
    ULONG SegmentIndex;
    KSTATUS Status;

    ImageInserted = FALSE;
    NotifyLoadCalled = FALSE;
    SegmentCount = 0;
    LoadingImage = ImAllocateMemory(sizeof(ELF_LOADING_IMAGE),
                                    IM_ALLOCATION_TAG);

    if (LoadingImage == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto LoadImageEnd;
    }

    Image->ImageContext = LoadingImage;
    RtlZeroMemory(LoadingImage, sizeof(ELF_LOADING_IMAGE));
    RtlCopyMemory(&(LoadingImage->Buffer), Buffer, sizeof(IMAGE_BUFFER));

    //
    // Get the ELF headers.
    //

    Result = ImpElfGetHeader(Buffer, &ElfHeader);
    if (Result == FALSE) {
        Status = STATUS_FILE_CORRUPT;
        goto LoadImageEnd;
    }

    LoadingImage->ElfHeader = ElfHeader;
    switch (ElfHeader->Machine) {
    case ELF_MACHINE_ARM:
        Image->Machine = ImageMachineTypeArm32;
        break;

    case ELF_MACHINE_I386:
        Image->Machine = ImageMachineTypeX86;
        break;

    default:
        Image->Machine = ImageMachineTypeUnknown;
        break;
    }

    SegmentCount = ElfHeader->ProgramHeaderCount;
    FirstProgramHeader = ImpReadBuffer(
                                  &(Image->File),
                                  Buffer,
                                  ElfHeader->ProgramHeaderOffset,
                                  ElfHeader->ProgramHeaderSize * SegmentCount);

    if (FirstProgramHeader == NULL) {
        Status = STATUS_UNKNOWN_IMAGE_FORMAT;
        goto LoadImageEnd;
    }

    //
    // Reload the ELF header if reading the program headers caused the buffer
    // to change.
    //

    if (Buffer->Data != ElfHeader) {
        Result = ImpElfGetHeader(Buffer, &ElfHeader);
        if (Result == FALSE) {
            Status = STATUS_FILE_CORRUPT;
            goto LoadImageEnd;
        }

        LoadingImage->ElfHeader = ElfHeader;
    }

    //
    // Allocate space for the image segment structures.
    //

    ASSERT(Image->Segments == NULL);

    if (SegmentCount == 0) {
        Status = STATUS_FILE_CORRUPT;
        goto LoadImageEnd;
    }

    Image->SegmentCount = SegmentCount;
    Image->Segments = ImAllocateMemory(SegmentCount * sizeof(IMAGE_SEGMENT),
                                       IM_ALLOCATION_TAG);

    if (Image->Segments == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto LoadImageEnd;
    }

    RtlZeroMemory(Image->Segments, SegmentCount * sizeof(IMAGE_SEGMENT));

    //
    // Loop through and load all program headers.
    //

    PreviousSegment = NULL;
    BaseDifference = Image->LoadedLowestAddress - Image->PreferredLowestAddress;
    ProgramHeader = FirstProgramHeader;
    for (SegmentIndex = 0; SegmentIndex < SegmentCount; SegmentIndex += 1) {
        Segment = &(Image->Segments[SegmentIndex]);

        //
        // Remember the TLS segment.
        //

        if (ProgramHeader->Type == ELF_SEGMENT_TYPE_TLS) {
            Image->TlsImage = (PVOID)(UINTN)(ProgramHeader->VirtualAddress) +
                              BaseDifference;

            Image->TlsImageSize = ProgramHeader->FileSize;
            Image->TlsSize = ProgramHeader->MemorySize;
            Image->TlsAlignment = ProgramHeader->Alignment;
        }

        //
        // Skip non-loading segments.
        //

        if (ProgramHeader->Type != ELF_SEGMENT_TYPE_LOAD) {
            ProgramHeader += 1;
            continue;
        }

        //
        // Determine where in memory this segment will start.
        //

        SegmentBase = ProgramHeader->VirtualAddress;

        //
        // Convert the flags.
        //

        if ((ProgramHeader->Flags & ELF_PROGRAM_HEADER_FLAG_WRITE) != 0) {
            Segment->Flags |= IMAGE_MAP_FLAG_WRITE;
        }

        if ((ProgramHeader->Flags & ELF_PROGRAM_HEADER_FLAG_EXECUTE) != 0) {
            Segment->Flags |= IMAGE_MAP_FLAG_EXECUTE;
        }

        //
        // The mapping is fixed if it's not the first program header or its
        // not an image that can be relocated.
        //

        if ((PreviousSegment != NULL) ||
            ((Image->Flags & IMAGE_FLAG_RELOCATABLE) == 0)) {

            Segment->Flags |= IMAGE_MAP_FLAG_FIXED;
        }

        //
        // Set up and map the segment.
        //

        Segment->VirtualAddress = (PVOID)SegmentBase + BaseDifference;
        Segment->FileSize = ProgramHeader->FileSize;
        Segment->MemorySize = ProgramHeader->MemorySize;

        //
        // The segments should always be in increasing virtual address order.
        //

        if ((PreviousSegment != NULL) &&
            (PreviousSegment->VirtualAddress + PreviousSegment->MemorySize >
             Segment->VirtualAddress)) {

            Status = STATUS_FILE_CORRUPT;
            goto LoadImageEnd;
        }

        Status = ImMapImageSegment(Image->AllocatorHandle,
                                   Image->LoadedLowestAddress,
                                   &(Image->File),
                                   ProgramHeader->Offset,
                                   Segment,
                                   PreviousSegment);

        if (!KSUCCESS(Status)) {
            goto LoadImageEnd;
        }

        //
        // If this was the first section to get slapped down and address space
        // wasn't predetermined, update it now.
        //

        if ((PreviousSegment == NULL) &&
            (Image->AllocatorHandle == INVALID_HANDLE)) {

            Image->LoadedLowestAddress = Segment->VirtualAddress;
            Image->LoadedImageBuffer = Segment->VirtualAddress;
            BaseDifference = Image->LoadedLowestAddress -
                             Image->PreferredLowestAddress;
        }

        Segment->Type = ImageSegmentFileSection;
        PreviousSegment = Segment;
        ProgramHeader = (PELF32_PROGRAM_HEADER)((PUCHAR)ProgramHeader +
                                                ElfHeader->ProgramHeaderSize);
    }

    Image->EntryPoint =
                 (PVOID)(LoadingImage->ElfHeader->EntryPoint + BaseDifference);

    INSERT_BEFORE(&(Image->ListEntry), ListHead);
    ImageInserted = TRUE;
    Status = ImNotifyImageLoad(Image);
    if (!KSUCCESS(Status)) {
        goto LoadImageEnd;
    }

    NotifyLoadCalled = TRUE;

    //
    // If only loading, don't process the dynamic section.
    //

    if ((Image->LoadFlags & IMAGE_LOAD_FLAG_LOAD_ONLY) != 0) {
        ImpElfFreeContext(Image);
        goto LoadImageEnd;
    }

    //
    // Gather information not in the loaded part of the file needed for
    // resolving exports from this image.
    //

    Status = ImpElfGatherExportInformation(Image, FALSE);
    if (!KSUCCESS(Status)) {
        goto LoadImageEnd;
    }

    //
    // If the import count is non-zero, then this is an import being loaded.
    // Do nothing else, as relocations and imports happen at the base level.
    //

    if (ImportDepth != 0) {
        Status = STATUS_SUCCESS;
        goto LoadImageEnd;
    }

    //
    // Load imports for all images.
    //

    Status = ImpElfLoadAllImports(ListHead);
    if (!KSUCCESS(Status)) {
        goto LoadImageEnd;
    }

    if ((Image->LoadFlags & IMAGE_LOAD_FLAG_NO_RELOCATIONS) == 0) {

        //
        // Loop through the list again and perform the final relocations now
        // that the complete symbol table is built.
        //

        Status = ImpElfRelocateImages(ListHead);
        if (!KSUCCESS(Status)) {
            goto LoadImageEnd;
        }
    }

    Status = STATUS_SUCCESS;

LoadImageEnd:
    if (!KSUCCESS(Status)) {
        if (Image->ImageContext != NULL) {
            ImFreeMemory(Image->ImageContext);
            Image->ImageContext = NULL;
        }

        if (NotifyLoadCalled != FALSE) {

            //
            // Unload any imports.
            //

            for (ImportIndex = 0;
                 ImportIndex < Image->ImportCount;
                 ImportIndex += 1) {

                if (Image->Imports[ImportIndex] != NULL) {
                    ImImageReleaseReference(Image->Imports[ImportIndex]);
                }
            }

            if (Image->Imports != NULL) {
                ImFreeMemory(Image->Imports);
            }

            ImNotifyImageUnload(Image);
        }

        if (ImageInserted != FALSE) {
            LIST_REMOVE(&(Image->ListEntry));
        }

        if (Image->Segments != NULL) {

            //
            // Unmap all mapped segments.
            //

            for (SegmentIndex = 0;
                 SegmentIndex < SegmentCount;
                 SegmentIndex += 1) {

                Segment = &(Image->Segments[SegmentIndex]);
                if (Segment->Type != ImageSegmentInvalid) {
                    ImUnmapImageSegment(Image->AllocatorHandle, Segment);
                }
            }

            ImFreeMemory(Image->Segments);
            Image->Segments = NULL;
            Image->SegmentCount = 0;
        }

        if (Image->StaticFunctions != NULL) {
            ImFreeMemory(Image->StaticFunctions);
        }
    }

    return Status;
}

KSTATUS
ImpElfAddImage (
    PIMAGE_BUFFER ImageBuffer,
    PLOADED_IMAGE Image
    )

/*++

Routine Description:

    This routine adds the accounting structures for an image that has already
    been loaded into memory.

Arguments:

    ImageBuffer - Supplies a pointer to the loaded image buffer.

    Image - Supplies a pointer to the image to initialize.

Return Value:

    Status code.

--*/

{

    UINTN BaseDifference;
    PELF32_HEADER ElfHeader;
    PELF32_PROGRAM_HEADER FirstProgramHeader;
    UINTN Index;
    PELF_LOADING_IMAGE LoadingImage;
    UINTN LowestVirtualAddress;
    PELF32_PROGRAM_HEADER ProgramHeader;
    ULONG SegmentCount;
    KSTATUS Status;

    ElfHeader = Image->LoadedLowestAddress;
    Image->LoadedLowestAddress = ImageBuffer->Data;
    Image->Size = ImageBuffer->Size;
    Image->LoadedImageBuffer = Image->LoadedLowestAddress;
    LoadingImage = ImAllocateMemory(sizeof(ELF_LOADING_IMAGE),
                                    IM_ALLOCATION_TAG);

    if (LoadingImage == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto AddImageEnd;
    }

    Image->ImageContext = LoadingImage;
    RtlZeroMemory(LoadingImage, sizeof(ELF_LOADING_IMAGE));
    RtlCopyMemory(&(LoadingImage->Buffer), ImageBuffer, sizeof(IMAGE_BUFFER));
    LoadingImage->ElfHeader = ElfHeader;
    if (ElfHeader->ImageType == ELF_IMAGE_SHARED_OBJECT) {
        Image->Flags |= IMAGE_FLAG_RELOCATABLE;

    } else if (ElfHeader->ImageType == ELF_IMAGE_EXECUTABLE) {
        Image->Flags &= ~IMAGE_FLAG_RELOCATABLE;

    } else {
        Status = STATUS_UNKNOWN_IMAGE_FORMAT;
        goto AddImageEnd;
    }

    switch (ElfHeader->Machine) {
    case ELF_MACHINE_ARM:
        Image->Machine = ImageMachineTypeArm32;
        break;

    case ELF_MACHINE_I386:
        Image->Machine = ImageMachineTypeX86;
        break;

    default:
        Image->Machine = ImageMachineTypeUnknown;
        break;
    }

    SegmentCount = ElfHeader->ProgramHeaderCount;
    FirstProgramHeader = ImpReadBuffer(
                                  &(Image->File),
                                  ImageBuffer,
                                  ElfHeader->ProgramHeaderOffset,
                                  ElfHeader->ProgramHeaderSize * SegmentCount);

    if (FirstProgramHeader == NULL) {
        Status = STATUS_UNKNOWN_IMAGE_FORMAT;
        goto AddImageEnd;
    }

    //
    // Loop through the program headers.
    //

    LowestVirtualAddress = (UINTN)-1;
    ProgramHeader = FirstProgramHeader;
    for (Index = 0; Index < SegmentCount; Index += 1) {

        //
        // Remember the TLS segment.
        //

        if (ProgramHeader->Type == ELF_SEGMENT_TYPE_TLS) {
            Image->TlsImage = (PVOID)(UINTN)(ProgramHeader->VirtualAddress);
            Image->TlsImageSize = ProgramHeader->FileSize;
            Image->TlsSize = ProgramHeader->MemorySize;
            Image->TlsAlignment = ProgramHeader->Alignment;

        } else if (ProgramHeader->Type == ELF_SEGMENT_TYPE_LOAD) {
            if (ProgramHeader->VirtualAddress < LowestVirtualAddress) {
                LowestVirtualAddress = ProgramHeader->VirtualAddress;
            }
        }

        ProgramHeader = (PELF32_PROGRAM_HEADER)((PUCHAR)ProgramHeader +
                                                ElfHeader->ProgramHeaderSize);
    }

    Image->PreferredLowestAddress = (PVOID)LowestVirtualAddress;
    BaseDifference = Image->LoadedLowestAddress - Image->PreferredLowestAddress;
    if (Image->TlsImage != NULL) {
        Image->TlsImage += BaseDifference;
    }

    Image->EntryPoint = (PVOID)(ElfHeader->EntryPoint + BaseDifference);
    Status = ImpElfGatherExportInformation(Image, TRUE);
    if (!KSUCCESS(Status)) {
        goto AddImageEnd;
    }

AddImageEnd:
    if (!KSUCCESS(Status)) {
        if (Image != NULL) {
            if (Image->ImageContext != NULL) {
                ImFreeMemory(Image->ImageContext);
                Image->ImageContext = NULL;
            }

            if (Image->StaticFunctions != NULL) {
                ImFreeMemory(Image->StaticFunctions);
            }
        }
    }

    return Status;
}

VOID
ImpElfUnloadImage (
    PLOADED_IMAGE Image
    )

/*++

Routine Description:

    This routine unloads an ELF executable.

Arguments:

    Image - Supplies a pointer to the loaded image.

Return Value:

    None.

--*/

{

    ULONG ImportIndex;
    ULONG SegmentIndex;

    ASSERT((Image->ImportCount == 0) || (Image->Imports != NULL));

    ImpElfFreeContext(Image);

    //
    // Unload all imports.
    //

    for (ImportIndex = 0; ImportIndex < Image->ImportCount; ImportIndex += 1) {

        ASSERT(Image->Imports[ImportIndex] != NULL);

        ImImageReleaseReference(Image->Imports[ImportIndex]);
    }

    if (Image->Imports != NULL) {
        ImFreeMemory(Image->Imports);
    }

    ASSERT((Image->Segments != NULL) || (Image->SegmentCount == 0));

    for (SegmentIndex = 0;
         SegmentIndex < Image->SegmentCount;
         SegmentIndex += 1) {

        if (Image->Segments[SegmentIndex].Type != ImageSegmentInvalid) {
            ImUnmapImageSegment(Image->AllocatorHandle,
                                &(Image->Segments[SegmentIndex]));
        }
    }

    if (Image->Segments != NULL) {
        ImFreeMemory(Image->Segments);
        Image->Segments = NULL;
    }

    if (Image->StaticFunctions != NULL) {
        ImFreeMemory(Image->StaticFunctions);
        Image->StaticFunctions = NULL;
    }

    return;
}

BOOL
ImpElfGetHeader (
    PIMAGE_BUFFER Buffer,
    PELF32_HEADER *ElfHeader
    )

/*++

Routine Description:

    This routine returns a pointer to the ELF image header given a buffer
    containing the executable image mapped in memory.

Arguments:

    Buffer - Supplies a pointer to the loaded image buffer.

    ElfHeader - Supplies a pointer where the location of the ELF header will
        be returned.

Return Value:

    TRUE on success.

    FALSE otherwise.

--*/

{

    PELF32_HEADER Header;

    *ElfHeader = NULL;
    Header = ImpReadBuffer(NULL, Buffer, 0, sizeof(ELF32_HEADER));
    if (Header == NULL) {
        return FALSE;
    }

    if ((Header->Identification[0] != ELF_MAGIC0) ||
        (Header->Identification[1] != ELF_MAGIC1) ||
        (Header->Identification[2] != ELF_MAGIC2) ||
        (Header->Identification[3] != ELF_MAGIC3)) {

        return FALSE;
    }

    //
    // Only relocatable, executable and shared object files are supported.
    //

    if ((Header->ImageType != ELF_IMAGE_RELOCATABLE) &&
        (Header->ImageType != ELF_IMAGE_EXECUTABLE) &&
        (Header->ImageType != ELF_IMAGE_SHARED_OBJECT)) {

        return FALSE;
    }

    //
    // Only i386 and ARM images are supported.
    //

    if ((Header->Machine != ELF_MACHINE_I386) &&
        (Header->Machine != ELF_MACHINE_ARM)) {

        return FALSE;
    }

    //
    // Only 32-bit little endian images are supported.
    //

    if ((Header->Identification[ELF_CLASS_OFFSET] != ELF_32BIT) ||
        (Header->Identification[ELF_ENDIANNESS_OFFSET] != ELF_LITTLE_ENDIAN)) {

        return FALSE;
    }

    //
    // Ensure that the program header and section header sizes are consistent.
    //

    if ((Header->ProgramHeaderSize != sizeof(ELF32_PROGRAM_HEADER)) ||
        (Header->SectionHeaderSize != sizeof(ELF32_SECTION_HEADER))) {

        return FALSE;
    }

    *ElfHeader = Header;
    return TRUE;
}

BOOL
ImpElfGetSection (
    PIMAGE_BUFFER Buffer,
    PSTR SectionName,
    PVOID *Section,
    PULONGLONG VirtualAddress,
    PULONG SectionSizeInFile,
    PULONG SectionSizeInMemory
    )

/*++

Routine Description:

    This routine gets a pointer to the given section in an ELF image given a
    memory mapped file.

Arguments:

    Buffer - Supplies a pointer to the image buffer.

    SectionName - Supplies the name of the desired section.

    Section - Supplies a pointer where the pointer to the section will be
        returned.

    VirtualAddress - Supplies a pointer where the virtual address of the section
        will be returned, if applicable.

    SectionSizeInFile - Supplies a pointer where the size of the section as it
        appears in the file will be returned.

    SectionSizeInMemory - Supplies a pointer where the size of the section as it
        appears after being loaded in memory will be returned.

Return Value:

    TRUE on success.

    FALSE otherwise.

--*/

{

    PSTR CurrentSectionName;
    PELF32_HEADER ElfHeader;
    BOOL Match;
    BOOL Result;
    PVOID ReturnSection;
    ULONG ReturnSectionFileSize;
    ULONG ReturnSectionMemorySize;
    ULONG ReturnSectionVirtualAddress;
    PELF32_SECTION_HEADER SectionHeader;
    ULONG SectionIndex;
    PSTR StringTable;
    PELF32_SECTION_HEADER StringTableHeader;

    ReturnSection = NULL;
    ReturnSectionFileSize = 0;
    ReturnSectionMemorySize = 0;
    ReturnSectionVirtualAddress = (UINTN)NULL;
    if (SectionName == NULL) {
        Result = FALSE;
        goto GetSectionEnd;
    }

    Result = ImpElfGetHeader(Buffer, &ElfHeader);
    if (Result == FALSE) {
        goto GetSectionEnd;
    }

    //
    // Get the beginning of the section array, and then get the string table.
    //

    SectionHeader = ImpReadBuffer(
                 NULL,
                 Buffer,
                 ElfHeader->SectionHeaderOffset,
                 sizeof(ELF32_SECTION_HEADER) * ElfHeader->SectionHeaderCount);

    if (SectionHeader == NULL) {
        Result = FALSE;
        goto GetSectionEnd;
    }

    StringTableHeader = SectionHeader + ElfHeader->StringSectionIndex;
    StringTable = ImpReadBuffer(NULL,
                                Buffer,
                                StringTableHeader->Offset,
                                StringTableHeader->Size);

    if (StringTable == NULL) {
        Result = FALSE;
        goto GetSectionEnd;
    }

    //
    // Loop through all sections looking for the desired one.
    //

    for (SectionIndex = 0;
         SectionIndex < ElfHeader->SectionHeaderCount;
         SectionIndex += 1) {

        //
        // Skip null sections.
        //

        if (SectionHeader->SectionType == ELF_SECTION_TYPE_NULL) {
            SectionHeader += 1;
            continue;
        }

        if (SectionHeader->NameOffset >= StringTableHeader->Size) {
            Result = FALSE;
            goto GetSectionEnd;
        }

        CurrentSectionName = StringTable + SectionHeader->NameOffset;
        Match = RtlAreStringsEqual(CurrentSectionName,
                                   SectionName,
                                   MAX_SECTION_NAME);

        //
        // If the name matches, return that section. Sections have no relevance
        // on what is loaded into memory, so all sections have a memory size of
        // zero.
        //

        if (Match != FALSE) {
            ReturnSection = ImpReadBuffer(NULL,
                                          Buffer,
                                          SectionHeader->Offset,
                                          SectionHeader->Size);

            if (ReturnSection == NULL) {
                Result = FALSE;
                goto GetSectionEnd;
            }

            ReturnSectionFileSize = SectionHeader->Size;
            ReturnSectionMemorySize = 0;
            ReturnSectionVirtualAddress = SectionHeader->VirtualAddress;
            break;
        }

        SectionHeader += 1;
    }

GetSectionEnd:
    if (Section != NULL) {
        *Section = ReturnSection;
    }

    if (VirtualAddress != NULL) {
        *VirtualAddress = ReturnSectionVirtualAddress;
    }

    if (SectionSizeInFile != NULL) {
        *SectionSizeInFile = ReturnSectionFileSize;
    }

    if (SectionSizeInMemory != NULL) {
        *SectionSizeInMemory = ReturnSectionMemorySize;
    }

    return Result;
}

KSTATUS
ImpElfLoadAllImports (
    PLIST_ENTRY ListHead
    )

/*++

Routine Description:

    This routine loads all import libraries for all images.

Arguments:

    ListHead - Supplies a pointer to the head of the list of loaded images.

Return Value:

    Status code.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PLOADED_IMAGE CurrentImage;
    KSTATUS Status;

    //
    // Loop through the list and load imports for each image. This may cause
    // additional images to get added to the end of the list, but traversal of
    // the list won't get corrupted because images never disappear from the
    // list this way.
    //

    CurrentEntry = ListHead->Next;
    while (CurrentEntry != ListHead) {
        CurrentImage = LIST_VALUE(CurrentEntry, LOADED_IMAGE, ListEntry);

        ASSERT((CurrentImage->LoadFlags & IMAGE_LOAD_FLAG_LOAD_ONLY) == 0);

        if ((CurrentImage->Flags & IMAGE_FLAG_IMPORTS_LOADED) == 0) {
            Status = ImpElfLoadImportsForImage(ListHead, CurrentImage);
            if (!KSUCCESS(Status)) {
                goto LoadAllImportsEnd;
            }

            CurrentImage->Flags |= IMAGE_FLAG_IMPORTS_LOADED;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    Status = STATUS_SUCCESS;

LoadAllImportsEnd:
    return Status;
}

KSTATUS
ImpElfRelocateImages (
    PLIST_ENTRY ListHead
    )

/*++

Routine Description:

    This routine relocates all images on the given image list that have not
    yet been relocated.

Arguments:

    ListHead - Supplies a pointer to the head of the list to relocate.

Return Value:

    Status code.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PLOADED_IMAGE CurrentImage;
    KSTATUS Status;

    Status = ImpElfLoadAllImports(ListHead);
    if (!KSUCCESS(Status)) {
        goto RelocateImagesEnd;
    }

    CurrentEntry = ListHead->Next;
    while (CurrentEntry != ListHead) {
        CurrentImage = LIST_VALUE(CurrentEntry, LOADED_IMAGE, ListEntry);
        if ((CurrentImage->Flags & IMAGE_FLAG_RELOCATED) == 0) {
            Status = ImpElfRelocateImage(ListHead, CurrentImage);
            if (!KSUCCESS(Status)) {
                goto RelocateImagesEnd;
            }

            CurrentImage->Flags |= IMAGE_FLAG_RELOCATED;
            if (ImFinalizeSegments != NULL) {
                Status = ImFinalizeSegments(CurrentImage->AllocatorHandle,
                                            CurrentImage->Segments,
                                            CurrentImage->SegmentCount);

                if (!KSUCCESS(Status)) {
                    goto RelocateImagesEnd;
                }
            }

            ImpElfFreeContext(CurrentImage);
        }

        CurrentEntry = CurrentEntry->Next;
    }

    Status = STATUS_SUCCESS;

RelocateImagesEnd:
    return Status;
}

KSTATUS
ImpElfGetSymbolAddress (
    PLIST_ENTRY ListHead,
    PLOADED_IMAGE Image,
    PSTR SymbolName,
    PVOID *Address
    )

/*++

Routine Description:

    This routine attempts to find an exported symbol with the given name in the
    given binary. This routine looks through the image and its imports.

Arguments:

    ListHead - Supplies the head of the list of loaded images.

    Image - Supplies a pointer to the image to query.

    SymbolName - Supplies a pointer to the string containing the name of the
        symbol to search for.

    Address - Supplies a pointer where the address of the symbol will be
        returned on success, or NULL will be returned on failure.

Return Value:

    Status code.

--*/

{

    ULONG Hash;
    PELF32_SYMBOL Symbol;
    UINTN Value;

    if ((Image->Flags & IMAGE_FLAG_GNU_HASH) != 0) {
        Hash = ImpElfGnuHash(SymbolName);

    } else {
        Hash = ImpElfOriginalHash(SymbolName);
    }

    Symbol = ImpElfGetSymbol(Image, Hash, SymbolName);
    if (Symbol == NULL) {
        return STATUS_NOT_FOUND;
    }

    Value = ImpElfGetSymbolValue(ListHead, Image, Symbol, NULL, NULL);
    if (Value == MAX_ULONG) {
        return STATUS_NOT_FOUND;
    }

    *Address = (PVOID)Value;
    return STATUS_SUCCESS;
}

PVOID
ImpElfResolvePltEntry (
    PLIST_ENTRY ListHead,
    PLOADED_IMAGE Image,
    ULONG RelocationOffset
    )

/*++

Routine Description:

    This routine implements the slow path for a Procedure Linkable Table entry
    that has not yet been resolved to its target function address. This routine
    is only called once for each PLT entry, as subsequent calls jump directly
    to the destination function address. It resolves the appropriate GOT
    relocation and returns a pointer to the function to jump to.

Arguments:

    ListHead - Supplies a pointer to the head of the list of images to use for
        symbol resolution.

    Image - Supplies a pointer to the loaded image whose PLT needs resolution.
        This is really whatever pointer is in GOT + 4.

    RelocationOffset - Supplies the byte offset from the start of the
        relocation section where the relocation for this PLT entry resides, or
        the PLT index, depending on the architecture.

Return Value:

    Returns a pointer to the function to jump to (in addition to writing that
    address in the GOT at the appropriate spot).

--*/

{

    PVOID FunctionAddress;
    PVOID RelocationEntry;
    UINTN RelocationSize;
    BOOL Result;

    FunctionAddress = NULL;

    //
    // On ARM, what's passed in is a PLT index. Convert that to an offset based
    // on the size of each PLT entry.
    //

    if (Image->Machine == ImageMachineTypeArm32) {
        RelocationSize = sizeof(ELF32_RELOCATION_ENTRY);
        if (Image->PltRelocationsAddends != FALSE) {
            RelocationSize = sizeof(ELF32_RELOCATION_ADDEND_ENTRY);
        }

        RelocationOffset *= RelocationSize;
    }

    RelocationEntry = Image->PltRelocations + RelocationOffset;
    Result = ImpElfApplyRelocation(ListHead,
                                   Image,
                                   RelocationEntry,
                                   Image->PltRelocationsAddends,
                                   &FunctionAddress);

    ASSERT(Result != FALSE);

    return FunctionAddress;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
ImpElfLoadImportsForImage (
    PLIST_ENTRY ListHead,
    PLOADED_IMAGE Image
    )

/*++

Routine Description:

    This routine loads all import libraries for the given image. It does not
    perform relocations or load imports of the imports.

Arguments:

    ListHead - Supplies a pointer to the head of the list of loaded images.

    Image - Supplies a pointer to the loaded image structure.

Return Value:

    Status code.

--*/

{

    PELF32_DYNAMIC_ENTRY DynamicEntry;
    UINTN DynamicIndex;
    PLOADED_IMAGE Import;
    ULONG ImportCount;
    ULONG ImportIndex;
    PSTR ImportName;
    PELF_LOADING_IMAGE LoadingImage;
    KSTATUS Status;
    ULONG StringTableOffset;

    LoadingImage = Image->ImageContext;

    ASSERT(LoadingImage != NULL);

    DynamicEntry = LoadingImage->DynamicSection;
    Status = STATUS_SUCCESS;
    if (LoadingImage->DynamicEntryCount == 0) {
        goto LoadImportsForImageEnd;
    }

    //
    // Loop over all dynamic entries once to count the number of import
    // libraries needed.
    //

    ImportCount = 0;
    for (DynamicIndex = 0;
         DynamicIndex < LoadingImage->DynamicEntryCount;
         DynamicIndex += 1) {

        //
        // A null entry indicates the end of the symbol table.
        //

        if (DynamicEntry[DynamicIndex].Tag == ELF_DYNAMIC_NULL) {
            break;
        }

        //
        // A "needed" entry indicates a required import library.
        //

        if (DynamicEntry[DynamicIndex].Tag == ELF_DYNAMIC_NEEDED) {
            ImportCount += 1;
        }
    }

    if (ImportCount == 0) {
        Status = STATUS_SUCCESS;
        goto LoadImportsForImageEnd;
    }

    //
    // Allocate space to hold the imports.
    //

    ASSERT(Image->Imports == NULL);

    Image->Imports = ImAllocateMemory(ImportCount * sizeof(PLOADED_IMAGE),
                                      IM_ALLOCATION_TAG);

    if (Image->Imports == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto LoadImportsForImageEnd;
    }

    RtlZeroMemory(Image->Imports, ImportCount * sizeof(PLOADED_IMAGE));
    Image->ImportCount = ImportCount;
    ImportIndex = 0;
    for (DynamicIndex = 0;
         DynamicIndex < LoadingImage->DynamicEntryCount;
         DynamicIndex += 1) {

        //
        // A null entry indicates the end of the symbol table.
        //

        if (DynamicEntry[DynamicIndex].Tag == ELF_DYNAMIC_NULL) {
            break;
        }

        //
        // A "needed" entry indicates a required import library.
        //

        if (DynamicEntry[DynamicIndex].Tag == ELF_DYNAMIC_NEEDED) {

            //
            // If no string table was found, the image is crazy.
            //

            if (Image->ExportStringTable == NULL) {
                Status = STATUS_FILE_CORRUPT;
                goto LoadImportsForImageEnd;
            }

            StringTableOffset = DynamicEntry[DynamicIndex].Value;

            ASSERT(StringTableOffset < Image->ExportStringTableSize);

            ImportName = (PSTR)Image->ExportStringTable + StringTableOffset;

            //
            // Load the import. Dynamic libraries shouldn't have interpreters
            // specified.
            //

            Status = ImpElfLoadImport(Image, ListHead, ImportName, &Import);
            if (!KSUCCESS(Status)) {
                goto LoadImportsForImageEnd;
            }

            Image->Imports[ImportIndex] = Import;
            ImportIndex += 1;
        }
    }

LoadImportsForImageEnd:
    if (!KSUCCESS(Status)) {
        if (Image->Imports != NULL) {
            ImFreeMemory(Image->Imports);
            Image->Imports = NULL;
            Image->ImportCount = 0;
        }
    }

    return Status;
}

KSTATUS
ImpElfLoadImport (
    PLOADED_IMAGE Image,
    PLIST_ENTRY ListHead,
    PSTR LibraryName,
    PLOADED_IMAGE *Import
    )

/*++

Routine Description:

    This routine attempts to load a needed library for an ELF image.

Arguments:

    Image - Supplies a pointer to the image that needs the library.

    ListHead - Supplies a pointer to the head of the list of loaded images.

    LibraryName - Supplies the name of the library to load.

    Import - Supplies a pointer where a pointer to the loaded image will be
        returned.

Return Value:

    Status code.

--*/

{

    PELF32_DYNAMIC_ENTRY Entry;
    PSTR Path;
    PELF32_DYNAMIC_ENTRY RunPath;
    PSTR Slash;
    KSTATUS Status;

    //
    // If there's a slash, then just load the library without paths.
    //

    Slash = RtlStringFindCharacter(LibraryName, '/', -1);
    if (Slash != NULL) {
        Status = ImpElfLoadImportWithPath(Image,
                                          ListHead,
                                          LibraryName,
                                          "/",
                                          Import);

        goto ElfLoadImportEnd;
    }

    //
    // First find a DT_RUNPATH. If both DT_RUNPATH and DT_RPATH are found,
    // the spec says that only DT_RUNPATH should be run.
    //

    RunPath = NULL;
    if (Image->ImageContext != NULL) {
        Status = ImpElfGetDynamicEntry(Image->ImageContext,
                                       ELF_DYNAMIC_RUN_PATH,
                                       &RunPath);

        if (!KSUCCESS(Status)) {

            //
            // Search for DT_RPATH, which takes precedences over
            // LD_LIBRARY_PATH.
            //

            Status = ImpElfGetDynamicEntry(Image->ImageContext,
                                           ELF_DYNAMIC_RPATH,
                                           &Entry);

            if ((KSUCCESS(Status)) &&
                (Entry->Value < Image->ExportStringTableSize)) {

                Path = Image->ExportStringTable + Entry->Value;
                Status = ImpElfLoadImportWithPath(Image,
                                                  ListHead,
                                                  LibraryName,
                                                  Path,
                                                  Import);

                if (KSUCCESS(Status)) {
                    goto ElfLoadImportEnd;
                }
            }
        }
    }

    //
    // Get the library search path and use that.
    //

    Path = ImpElfGetEnvironmentVariable(IMAGE_DYNAMIC_LIBRARY_PATH_VARIABLE);
    if (Path != NULL) {
        Status = ImpElfLoadImportWithPath(Image,
                                          ListHead,
                                          LibraryName,
                                          Path,
                                          Import);

        if (KSUCCESS(Status)) {
            goto ElfLoadImportEnd;
        }
    }

    //
    // Try DT_RUNPATH.
    //

    if ((RunPath != NULL) && (RunPath->Value < Image->ExportStringTableSize)) {
        Path = Image->ExportStringTable + RunPath->Value;
        Status = ImpElfLoadImportWithPath(Image,
                                          ListHead,
                                          LibraryName,
                                          Path,
                                          Import);

        if (KSUCCESS(Status)) {
            goto ElfLoadImportEnd;
        }
    }

    //
    // Try some hard coded paths.
    //

    Status = ImpElfLoadImportWithPath(Image,
                                      ListHead,
                                      LibraryName,
                                      ELF_BUILTIN_LIBRARY_PATH,
                                      Import);

    if (KSUCCESS(Status)) {
        goto ElfLoadImportEnd;
    }

    RtlDebugPrint("%s: Failed to find import '%s'.\n",
                  Image->BinaryName,
                  LibraryName);

    Status = STATUS_MISSING_IMPORT;

ElfLoadImportEnd:
    return Status;
}

KSTATUS
ImpElfLoadImportWithPath (
    PLOADED_IMAGE Image,
    PLIST_ENTRY ListHead,
    PSTR LibraryName,
    PSTR Path,
    PLOADED_IMAGE *Import
    )

/*++

Routine Description:

    This routine attempts to load a needed library for an ELF image.

Arguments:

    Image - Supplies a pointer to the image that needs the library.

    ListHead - Supplies a pointer to the head of the list of loaded images.

    LibraryName - Supplies the name of the library to load.

    Path - Supplies a colon-separated list of paths to try.

    Import - Supplies a pointer where a pointer to the loaded image will be
        returned.

Return Value:

    Status code.

--*/

{

    PSTR CompletePath;
    UINTN CompletePathCapacity;
    UINTN CompletePathSize;
    PSTR CurrentPath;
    UINTN LibraryLength;
    ULONG LoadFlags;
    PSTR NextSeparator;
    UINTN PrefixLength;
    KSTATUS Status;

    LibraryLength = RtlStringLength(LibraryName);
    LoadFlags = Image->LoadFlags | IMAGE_LOAD_FLAG_IGNORE_INTERPRETER;
    LoadFlags &= ~IMAGE_LOAD_FLAG_PRIMARY_EXECUTABLE;
    CompletePath = NULL;
    CompletePathCapacity = 0;
    CurrentPath = Path;
    while (TRUE) {
        NextSeparator = RtlStringFindCharacter(CurrentPath, ':', -1);
        if (NextSeparator == NULL) {
            PrefixLength = RtlStringLength(CurrentPath);

        } else {
            PrefixLength = (UINTN)NextSeparator - (UINTN)CurrentPath;
        }

        //
        // The complete path is "prefix/library". Reallocate the buffer if
        // needed.
        //

        CompletePathSize = PrefixLength + LibraryLength + 2;
        if (CompletePathSize > CompletePathCapacity) {
            if (CompletePath != NULL) {
                ImFreeMemory(CompletePath);
            }

            CompletePath = ImAllocateMemory(CompletePathSize,
                                            IM_ALLOCATION_TAG);

            if (CompletePath == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                break;
            }
        }

        if (PrefixLength != 0) {
            RtlCopyMemory(CompletePath, CurrentPath, PrefixLength);
            if (CompletePath[PrefixLength - 1] != '/') {
                CompletePath[PrefixLength] = '/';
                PrefixLength += 1;
            }
        }

        RtlCopyMemory(CompletePath + PrefixLength,
                      LibraryName,
                      LibraryLength);

        CompletePath[PrefixLength + LibraryLength] = '\0';
        Status = ImpElfPerformLibraryPathSubstitutions(Image,
                                                       &CompletePath,
                                                       &CompletePathCapacity);

        if (!KSUCCESS(Status)) {
            break;
        }

        Status = ImLoadExecutable(ListHead,
                                  CompletePath,
                                  NULL,
                                  NULL,
                                  Image->SystemContext,
                                  LoadFlags,
                                  Image->ImportDepth + 1,
                                  Import,
                                  NULL);

        if (KSUCCESS(Status)) {
            break;
        }

        if (NextSeparator == NULL) {
            Status = STATUS_MISSING_IMPORT;
            break;
        }

        CurrentPath = NextSeparator + 1;
    }

    if (CompletePath != NULL) {
        ImFreeMemory(CompletePath);
    }

    return Status;
}

KSTATUS
ImpElfGatherExportInformation (
    PLOADED_IMAGE Image,
    BOOL UseLoadedAddress
    )

/*++

Routine Description:

    This routine gathers necessary pointers from the non-loaded portion of the
    image file needed to retrieve exports from the image.

Arguments:

    Image - Supplies a pointer to the loaded image.

    UseLoadedAddress - Supplies a boolean indicating whether to use the file
        offset (FALSE) or the final virtual address (TRUE).

Return Value:

    Status code.

--*/

{

    PVOID Address;
    UINTN BaseDifference;
    PELF32_DYNAMIC_ENTRY DynamicEntry;
    ULONG DynamicIndex;
    PVOID DynamicSymbols;
    PVOID DynamicSymbolStrings;
    UINTN DynamicSymbolStringsSize;
    PUINTN Got;
    PULONG HashTable;
    ULONG HashTag;
    ULONG HeaderCount;
    UINTN Index;
    UINTN LibraryNameOffset;
    PELF_LOADING_IMAGE LoadingImage;
    UINTN MaxDynamic;
    PVOID PltRelocations;
    BOOL PltRelocationsAddends;
    PELF32_PROGRAM_HEADER ProgramHeader;
    PIMAGE_STATIC_FUNCTIONS StaticFunctions;
    KSTATUS Status;
    ULONG Tag;
    UINTN Value;

    DynamicSymbols = NULL;
    DynamicSymbolStrings = NULL;
    DynamicSymbolStringsSize = 0;
    HashTable = NULL;
    HashTag = 0;
    LibraryNameOffset = 0;
    LoadingImage = Image->ImageContext;
    HeaderCount = LoadingImage->ElfHeader->ProgramHeaderCount;
    PltRelocations = NULL;
    PltRelocationsAddends = FALSE;
    ProgramHeader = NULL;
    StaticFunctions = NULL;

    //
    // This function is not using the read buffer functions because it should
    // only be called in the actual runtime address space, in which case the
    // image is fully loaded.
    //

    ASSERT(LoadingImage->Buffer.Size == Image->File.Size);

    //
    // Loop through the program headers to find the dynamic section.
    //

    for (Index = 0; Index < HeaderCount; Index += 1) {
        ProgramHeader = LoadingImage->Buffer.Data +
                        LoadingImage->ElfHeader->ProgramHeaderOffset +
                        (Index * LoadingImage->ElfHeader->ProgramHeaderSize);

        if (ProgramHeader->Type == ELF_SEGMENT_TYPE_DYNAMIC) {
            break;
        }
    }

    if (Index == LoadingImage->ElfHeader->ProgramHeaderCount) {
        Status = STATUS_SUCCESS;
        goto GatherExportInformationEnd;
    }

    //
    // Allocate the static functions structure if needed.
    //

    StaticFunctions = Image->StaticFunctions;
    if ((StaticFunctions == NULL) &&
        ((Image->LoadFlags & IMAGE_LOAD_FLAG_NO_STATIC_CONSTRUCTORS) == 0)) {

        StaticFunctions = ImAllocateMemory(sizeof(IMAGE_STATIC_FUNCTIONS),
                                           IM_ALLOCATION_TAG);

        if (StaticFunctions == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto GatherExportInformationEnd;
        }

        RtlZeroMemory(StaticFunctions, sizeof(IMAGE_STATIC_FUNCTIONS));
        Image->StaticFunctions = StaticFunctions;
    }

    if (UseLoadedAddress != FALSE) {
        BaseDifference = (UINTN)(Image->LoadedLowestAddress) -
                         (UINTN)(Image->PreferredLowestAddress);

        DynamicEntry = (PVOID)(UINTN)(ProgramHeader->VirtualAddress +
                                      BaseDifference);

    } else {
        BaseDifference = Image->LoadedImageBuffer -
                         Image->PreferredLowestAddress;

        DynamicEntry = LoadingImage->Buffer.Data + ProgramHeader->Offset;
    }

    MaxDynamic = ProgramHeader->FileSize / sizeof(ELF32_DYNAMIC_ENTRY);

    //
    // Save the pointer to the dynamic section header and dynamic symbol count.
    // This is used by the load import routine.
    //

    LoadingImage->DynamicSection = DynamicEntry;
    LoadingImage->DynamicEntryCount = MaxDynamic;
    for (DynamicIndex = 0; DynamicIndex < MaxDynamic; DynamicIndex += 1) {
        if (DynamicEntry[DynamicIndex].Tag == ELF_DYNAMIC_NULL) {
            break;
        }

        Tag = DynamicEntry[DynamicIndex].Tag;
        Value = DynamicEntry[DynamicIndex].Value;
        Address = (PVOID)(Value + BaseDifference);
        switch (Tag) {
        case ELF_DYNAMIC_LIBRARY_NAME:
            LibraryNameOffset = Value;
            break;

        //
        // Remember the string table.
        //

        case ELF_DYNAMIC_STRING_TABLE:
            DynamicSymbolStrings = Address;
            break;

        //
        // Remember the string table size.
        //

        case ELF_DYNAMIC_STRING_TABLE_SIZE:
            DynamicSymbolStringsSize = Value;
            break;

        //
        // Remember the symbol table.
        //

        case ELF_DYNAMIC_SYMBOL_TABLE:
            DynamicSymbols = Address;
            break;

        //
        // Remember the hash table.
        //

        case ELF_DYNAMIC_HASH_TABLE:
        case ELF_DYNAMIC_GNU_HASH_TABLE:
            HashTable = Address;
            HashTag = Tag;
            break;

        //
        // Remember static constructor and destructor array boundaries, and the
        // old _init and _fini functions.
        //

        case ELF_DYNAMIC_PREINIT_ARRAY:
            if (StaticFunctions != NULL) {
                StaticFunctions->PreinitArray = Address;
            }

            break;

        case ELF_DYNAMIC_INIT_ARRAY:
            if (StaticFunctions != NULL) {
                StaticFunctions->InitArray = Address;
            }

            break;

        case ELF_DYNAMIC_FINI_ARRAY:
            if (StaticFunctions != NULL) {
                StaticFunctions->FiniArray = Address;
            }

            break;

        case ELF_DYNAMIC_INIT:
            if (StaticFunctions != NULL) {
                StaticFunctions->InitFunction = Address;
            }

            break;

        case ELF_DYNAMIC_FINI:
            if (StaticFunctions != NULL) {
                StaticFunctions->FiniFunction = Address;
            }

            break;

        case ELF_DYNAMIC_PREINIT_ARRAY_SIZE:
            if (StaticFunctions != NULL) {
                StaticFunctions->PreinitArraySize = Value;
            }

            break;

        case ELF_DYNAMIC_INIT_ARRAY_SIZE:
            if (StaticFunctions != NULL) {
                StaticFunctions->InitArraySize = Value;
            }

            break;

        case ELF_DYNAMIC_FINI_ARRAY_SIZE:
            if (StaticFunctions != NULL) {
                StaticFunctions->FiniArraySize = Value;
            }

            break;

        case ELF_DYNAMIC_FLAGS:
            if ((Value & ELF_DYNAMIC_FLAG_STATIC_TLS) != 0) {
                Image->Flags |= IMAGE_FLAG_STATIC_TLS;

                //
                // Images with a static TLS model cannot be loaded dynamically,
                // the must be loaded with the initial executable.
                //

                if ((Image->LoadFlags & IMAGE_LOAD_FLAG_PRIMARY_LOAD) == 0) {
                    Status = STATUS_TOO_LATE;
                    goto GatherExportInformationEnd;
                }
            }

            break;

        //
        // Upon finding the GOT, save the image and the resolution address in
        // the second and third entries of the GOT.
        //

        case ELF_DYNAMIC_PLT_GOT:
            Got = Address;
            Got[1] = (UINTN)Image;
            Got[2] = (UINTN)(ImImportTable->ResolvePltEntry);
            break;

        //
        // Remember where the Procedure Linkage Table relocations are for
        // lazy binding.
        //

        case ELF_DYNAMIC_JUMP_RELOCATIONS:
            PltRelocations = Address;
            break;

        case ELF_DYNAMIC_PLT_RELOCATION_TYPE:
            if (DynamicEntry->Value == ELF_DYNAMIC_RELA_TABLE) {
                PltRelocationsAddends = TRUE;
            }

            break;

        case ELF_DYNAMIC_TEXT_RELOCATIONS:
            Image->Flags |= IMAGE_FLAG_TEXT_RELOCATIONS;
            break;

        default:
            break;
        }
    }

    //
    // If one of the required elements was not found, then as far as this
    // library is concerned there is no export information.
    //

    if ((DynamicSymbols == NULL) || (DynamicSymbolStrings == NULL) ||
        (DynamicSymbolStringsSize == 0) || (HashTable == NULL)) {

        Status = STATUS_SUCCESS;
        goto GatherExportInformationEnd;
    }

    Image->ExportSymbolTable = DynamicSymbols;
    Image->ExportStringTable = DynamicSymbolStrings;
    Image->ExportStringTableSize = DynamicSymbolStringsSize;
    Image->ExportHashTable = HashTable;
    Image->PltRelocations = PltRelocations;
    Image->PltRelocationsAddends = PltRelocationsAddends;
    if (HashTag == ELF_DYNAMIC_GNU_HASH_TABLE) {
        Image->Flags |= IMAGE_FLAG_GNU_HASH;
    }

    if ((Image->BinaryName == NULL) && (LibraryNameOffset != 0)) {
        Image->BinaryName = DynamicSymbolStrings + LibraryNameOffset;
    }

    Status = STATUS_SUCCESS;

GatherExportInformationEnd:
    return Status;
}

KSTATUS
ImpElfGetDynamicEntry (
    PELF_LOADING_IMAGE LoadingImage,
    ULONG Tag,
    PELF32_DYNAMIC_ENTRY *FoundEntry
    )

/*++

Routine Description:

    This routine attempts to find a dynamic entry with the given tag.

Arguments:

    LoadingImage - Supplies a pointer to the loading image context.

    Tag - Supplies the desired tag.

    FoundEntry - Supplies a pointer where a pointer to the entry will be
        returned on success.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NOT_FOUND on failure.

--*/

{

    PELF32_DYNAMIC_ENTRY Entry;
    UINTN Index;

    Entry = LoadingImage->DynamicSection;
    for (Index = 0; Index < LoadingImage->DynamicEntryCount; Index += 1) {
        if (Entry->Tag == Tag) {
            *FoundEntry = Entry;
            return STATUS_SUCCESS;
        }

        if (Entry->Tag == ELF_DYNAMIC_NULL) {
            break;
        }
    }

    return STATUS_NOT_FOUND;
}

KSTATUS
ImpElfRelocateImage (
    PLIST_ENTRY ListHead,
    PLOADED_IMAGE Image
    )

/*++

Routine Description:

    This routine relocates a loaded image.

Arguments:

    ListHead - Supplies a pointer to the head of the list of loaded images.

    Image - Supplies a pointer to the loaded image.

Return Value:

    Status code.

--*/

{

    PVOID Address;
    UINTN BaseDifference;
    PELF32_DYNAMIC_ENTRY DynamicEntry;
    UINTN DynamicIndex;
    PELF_LOADING_IMAGE LoadingImage;
    UINTN PltRelocationAddends;
    PVOID PltRelocations;
    UINTN PltRelocationsSize;
    PVOID Relocations;
    PVOID RelocationsAddends;
    UINTN RelocationsAddendsSize;
    UINTN RelocationsSize;
    UINTN Size;
    KSTATUS Status;

    LoadingImage = Image->ImageContext;

    ASSERT(LoadingImage != NULL);
    ASSERT((Image->LoadFlags & IMAGE_LOAD_FLAG_PLACEHOLDER) == 0);

    PltRelocationAddends = FALSE;
    PltRelocations = NULL;
    PltRelocationsSize = 0;
    Relocations = NULL;
    RelocationsAddends = NULL;
    RelocationsSize = 0;
    RelocationsAddendsSize = 0;
    LoadingImage->RelocationStart = ELF_INVALID_RELOCATION;
    LoadingImage->RelocationEnd = ELF_INVALID_RELOCATION;
    BaseDifference = (UINTN)(Image->LoadedImageBuffer -
                             Image->PreferredLowestAddress);

    DynamicEntry = LoadingImage->DynamicSection;
    if (DynamicEntry == NULL) {
        Status = STATUS_SUCCESS;
        goto RelocateImageEnd;
    }

    for (DynamicIndex = 0;
         DynamicIndex < LoadingImage->DynamicEntryCount;
         DynamicIndex += 1) {

        Address = (PVOID)(DynamicEntry->Value + BaseDifference);
        switch (DynamicEntry->Tag) {
        case ELF_DYNAMIC_REL_TABLE:
            Relocations = Address;
            break;

        case ELF_DYNAMIC_REL_TABLE_SIZE:
            RelocationsSize = DynamicEntry->Value;
            break;

        case ELF_DYNAMIC_RELA_TABLE:
            RelocationsAddends = Address;
            break;

        case ELF_DYNAMIC_RELA_TABLE_SIZE:
            RelocationsAddendsSize = DynamicEntry->Value;
            break;

        case ELF_DYNAMIC_JUMP_RELOCATIONS:
            PltRelocations = Address;
            break;

        case ELF_DYNAMIC_PLT_REL_SIZE:
            PltRelocationsSize = DynamicEntry->Value;
            break;

        case ELF_DYNAMIC_PLT_RELOCATION_TYPE:
            if (DynamicEntry->Value == ELF_DYNAMIC_RELA_TABLE) {
                PltRelocationAddends = TRUE;
            }

            break;

        default:
            break;
        }

        DynamicEntry += 1;
    }

    if ((Relocations != NULL) && (RelocationsSize != 0)) {
        Status = ImpElfProcessRelocateSection(ListHead,
                                              Image,
                                              Relocations,
                                              RelocationsSize,
                                              FALSE);

        if (!KSUCCESS(Status)) {
            goto RelocateImageEnd;
        }
    }

    if ((RelocationsAddends != NULL) && (RelocationsAddendsSize != 0)) {
        Status = ImpElfProcessRelocateSection(ListHead,
                                              Image,
                                              RelocationsAddends,
                                              RelocationsAddendsSize,
                                              TRUE);

        if (!KSUCCESS(Status)) {
            goto RelocateImageEnd;
        }
    }

    //
    // Only process the PLT relocations now if lazy binding is disabled.
    // Otherwise, these relocations get patched up when they're called, but
    // need to be adjusted by the base difference. This is much faster as
    // symbol lookups don't need to be done.
    //

    if ((PltRelocations != NULL) && (PltRelocationsSize != 0)) {
        if ((Image->LoadFlags & IMAGE_LOAD_FLAG_BIND_NOW) != 0) {
            Status = ImpElfProcessRelocateSection(ListHead,
                                                  Image,
                                                  PltRelocations,
                                                  PltRelocationsSize,
                                                  PltRelocationAddends);

            if (!KSUCCESS(Status)) {
                goto RelocateImageEnd;
            }

        } else {
            ImpElfAdjustJumpSlots(Image,
                                  PltRelocations,
                                  PltRelocationsSize,
                                  PltRelocationAddends);
        }
    }

    Status = STATUS_SUCCESS;

RelocateImageEnd:
    if (LoadingImage->RelocationStart != ELF_INVALID_RELOCATION) {

        ASSERT((Image->Flags & IMAGE_FLAG_TEXT_RELOCATIONS) != 0);
        ASSERT(LoadingImage->RelocationEnd != ELF_INVALID_RELOCATION);
        ASSERT(LoadingImage->RelocationEnd > LoadingImage->RelocationStart);

        Size = LoadingImage->RelocationEnd - LoadingImage->RelocationStart;
        ImInvalidateInstructionCacheRegion(LoadingImage->RelocationStart, Size);
    }

    return Status;
}

KSTATUS
ImpElfProcessRelocateSection (
    PLIST_ENTRY ListHead,
    PLOADED_IMAGE Image,
    PVOID Relocations,
    UINTN RelocationsSize,
    BOOL Addends
    )

/*++

Routine Description:

    This routine processes the contents of a single relocation section.

Arguments:

    ListHead - Supplies a pointer to the head of the list of loaded images.

    Image - Supplies a pointer to the loaded image structure.

    Relocations - Supplies a pointer to the relocations to apply.

    RelocationsSize - Supplies the size of the relocation section in bytes.

    Addends - Supplies a boolean indicating whether these are relocations with
        explicit addends (TRUE) or implicit addends (FALSE).

Return Value:

    Status code.

--*/

{

    PELF32_RELOCATION_ENTRY Relocation;
    PELF32_RELOCATION_ADDEND_ENTRY RelocationAddend;
    ULONG RelocationCount;
    ULONG RelocationIndex;
    BOOL Result;

    RelocationAddend = Relocations;
    Relocation = Relocations;
    if (Addends != FALSE) {
        RelocationCount = RelocationsSize /
                          sizeof(ELF32_RELOCATION_ADDEND_ENTRY);

    } else {
        RelocationCount = RelocationsSize / sizeof(ELF32_RELOCATION_ENTRY);
    }

    //
    // Process each relocation in the table.
    //

    for (RelocationIndex = 0;
         RelocationIndex < RelocationCount;
         RelocationIndex += 1) {

        if (Addends != FALSE) {
            Result = ImpElfApplyRelocation(ListHead,
                                           Image,
                                           RelocationAddend,
                                           TRUE,
                                           NULL);

            ASSERT(Result != FALSE);

            if (Result == FALSE) {
                return STATUS_INVALID_PARAMETER;
            }

            RelocationAddend += 1;

        } else {
            Result = ImpElfApplyRelocation(ListHead,
                                           Image,
                                           (PVOID)Relocation,
                                           FALSE,
                                           NULL);

            ASSERT(Result != FALSE);

            if (Result == FALSE) {
                return STATUS_INVALID_PARAMETER;
            }

            Relocation += 1;
        }
    }

    return STATUS_SUCCESS;
}

VOID
ImpElfAdjustJumpSlots (
    PLOADED_IMAGE Image,
    PVOID Relocations,
    UINTN RelocationsSize,
    BOOL Addends
    )

/*++

Routine Description:

    This routine iterates over all the relocations in the PLT section and adds
    the base difference to them.

Arguments:

    Image - Supplies a pointer to the loaded image structure.

    Relocations - Supplies a pointer to the relocations to apply.

    RelocationsSize - Supplies the size of the relocation section in bytes.

    Addends - Supplies a boolean indicating whether these are relocations with
        explicit addends (TRUE) or implicit addends (FALSE).

Return Value:

    Status code.

--*/

{

    UINTN BaseDifference;
    ULONG Information;
    UINTN Offset;
    PELF32_RELOCATION_ENTRY Relocation;
    PELF32_RELOCATION_ADDEND_ENTRY RelocationAddend;
    ULONG RelocationCount;
    ULONG RelocationIndex;
    PUINTN RelocationPlace;
    ELF32_RELOCATION_TYPE RelocationType;

    BaseDifference = Image->LoadedLowestAddress - Image->PreferredLowestAddress;
    if (BaseDifference == 0) {
        return;
    }

    RelocationAddend = Relocations;
    Relocation = Relocations;
    if (Addends != FALSE) {
        RelocationCount = RelocationsSize /
                          sizeof(ELF32_RELOCATION_ADDEND_ENTRY);

    } else {
        RelocationCount = RelocationsSize / sizeof(ELF32_RELOCATION_ENTRY);
    }

    //
    // Process each relocation in the table.
    //

    for (RelocationIndex = 0;
         RelocationIndex < RelocationCount;
         RelocationIndex += 1) {

        if (Addends != FALSE) {
            Offset = RelocationAddend->Offset;
            Information = RelocationAddend->Information;
            RelocationAddend += 1;

        } else {
            Offset = Relocation->Offset;
            Information = Relocation->Information;
            Relocation += 1;
        }

        RelocationType = Information & 0xFF;

        //
        // If this is a jump slot relocation, bump it up by the base difference.
        //

        if (((Image->Machine == ImageMachineTypeArm32) &&
             (RelocationType == ElfArmRelocationJumpSlot)) ||
            ((Image->Machine == ImageMachineTypeX86) &&
             (RelocationType == Elf386RelocationJumpSlot))) {

            RelocationPlace = (PUINTN)((PUCHAR)Image->LoadedImageBuffer +
                               (Offset - (UINTN)Image->PreferredLowestAddress));

            *RelocationPlace += BaseDifference;
        }
    }

    return;
}

ULONG
ImpElfGetSymbolValue (
    PLIST_ENTRY ListHead,
    PLOADED_IMAGE Image,
    PELF32_SYMBOL Symbol,
    PLOADED_IMAGE *FoundImage,
    PLOADED_IMAGE SkipImage
    )

/*++

Routine Description:

    This routine determines the address of the given symbol.

Arguments:

    ListHead - Supplies a pointer to the head of the list of loaded images.

    Image - Supplies a pointer to the loaded image.

    Symbol - Supplies a pointer to the symbol whose value should be computed.

    FoundImage - Supplies an optional pointer where the image where the symbol
        is defined will be returned on success.

    SkipImage - Supplies an optional pointer to an image to ignore when
        searching for a symbol definition. This is used in copy relocations
        when looking for the shared object version of a symbol defined in
        both the executable and a shared object.

Return Value:

    Returns the symbol's value on success.

    MAX_ULONG on failure.

--*/

{

    UINTN BaseDifference;
    ELF32_SYMBOL_BIND BindType;
    PLIST_ENTRY CurrentEntry;
    PLOADED_IMAGE CurrentImage;
    ULONG Hash;
    ULONG OriginalHash;
    PELF32_SYMBOL Potential;
    CHAR PrintSymbolName[50];
    PSTR SymbolName;
    ELF32_SYMBOL_TYPE SymbolType;
    ULONG Value;

    CurrentImage = NULL;
    BaseDifference = Image->LoadedLowestAddress - Image->PreferredLowestAddress;
    BindType = ELF32_EXTRACT_SYMBOL_BIND(Symbol->Information);
    if (Symbol->NameOffset != 0) {
        SymbolName = Image->ExportStringTable + Symbol->NameOffset;
        if ((Image->Flags & IMAGE_FLAG_GNU_HASH) != 0) {
            Hash = ImpElfGnuHash(SymbolName);

        } else {
            Hash = ImpElfOriginalHash(SymbolName);
        }

        OriginalHash = Hash;
        CurrentEntry = ListHead->Next;
        if (BindType == ElfBindLocal) {

            ASSERT(SkipImage == NULL);

            CurrentEntry = &(Image->ListEntry);
        }

        while (CurrentEntry != ListHead) {
            CurrentImage = LIST_VALUE(CurrentEntry, LOADED_IMAGE, ListEntry);
            if (CurrentImage == SkipImage) {
                CurrentEntry = CurrentEntry->Next;
                continue;
            }

            //
            // If the two images disagree on hashing algorithms, then recompute
            // the hash for the image.
            //

            if (((CurrentImage->Flags ^ Image->Flags) &
                IMAGE_FLAG_GNU_HASH) != 0) {

                if ((CurrentImage->Flags & IMAGE_FLAG_GNU_HASH) != 0) {
                    Hash = ImpElfGnuHash(SymbolName);

                } else {
                    Hash = ImpElfOriginalHash(SymbolName);
                }
            }

            Potential = ImpElfGetSymbol(CurrentImage, Hash, SymbolName);
            Hash = OriginalHash;

            //
            // Don't match against the same undefined symbol in another image.
            //

            if ((Potential != NULL) && (Potential->SectionIndex != 0)) {

                //
                // TLS symbols are relative to their section base, and are
                // not adjusted.
                //

                SymbolType = ELF32_EXTRACT_SYMBOL_TYPE(Symbol->Information);
                if (SymbolType == ElfSymbolTls) {
                    Value = Potential->Value;
                    goto ElfGetSymbolValueEnd;

                } else if (Potential->SectionIndex >=
                           ELF_SECTION_RESERVED_LOW) {

                    Value = MAX_ULONG;
                    if (Potential->SectionIndex == ELF_SECTION_ABSOLUTE) {
                        Value = Potential->Value;
                    }

                    goto ElfGetSymbolValueEnd;
                }

                BaseDifference = CurrentImage->LoadedLowestAddress -
                                 CurrentImage->PreferredLowestAddress;

                Value = Potential->Value + BaseDifference;
                goto ElfGetSymbolValueEnd;
            }

            //
            // Don't look in other images if it's a local symbol.
            //

            if (BindType == ElfBindLocal) {
                break;
            }

            CurrentEntry = CurrentEntry->Next;
        }

        if (CurrentEntry == ListHead) {
            CurrentImage = NULL;
        }

        //
        // This symbol is not defined. If it's weak, that's okay. Otherwise,
        // that's a problem.
        //

        if (BindType != ElfBindWeak) {

            //
            // Copy the symbol name to the stack to avoid deadly faults during
            // debug print.
            //

            RtlStringCopy(PrintSymbolName, SymbolName, sizeof(PrintSymbolName));
            RtlDebugPrint("Warning: Unresolved reference to symbol %s.\n",
                          PrintSymbolName);

            Value = MAX_ULONG;

        } else {
            Value = 0;
        }

        goto ElfGetSymbolValueEnd;
    }

    //
    // If the section index is a reserved value, check to see if it's an
    // absolute symbol. If it is, just return the symbol value. Other reserved
    // values usually mean the symbol's value cannot be computed at this time.
    //

    if ((Symbol->SectionIndex == 0) ||
        (Symbol->SectionIndex >= ELF_SECTION_RESERVED_LOW)) {

        Value = MAX_ULONG;
        if (Symbol->SectionIndex == ELF_SECTION_ABSOLUTE) {
            Value = Symbol->Value;
        }

        goto ElfGetSymbolValueEnd;
    }

    Value = Symbol->Value + BaseDifference;

ElfGetSymbolValueEnd:
    if (FoundImage != NULL) {
        *FoundImage = CurrentImage;
    }

    return Value;
}

PELF32_SYMBOL
ImpElfGetSymbol (
    PLOADED_IMAGE Image,
    ULONG Hash,
    PSTR SymbolName
    )

/*++

Routine Description:

    This routine attempts to find an exported symbol with the given name in the
    given binary. This routine looks through the image and its imports.

Arguments:

    Image - Supplies a pointer to the image to query.

    Hash - Supplies the hashed symbol name.

    SymbolName - Supplies a pointer to the string containing the name of the
        symbol to search for.

Return Value:

    Returns a pointer to the symbol on success.

    NULL if no such symbol was found.

--*/

{

    ULONG BucketCount;
    ULONG BucketIndex;
    BOOL Equal;
    PUINTN Filter;
    UINTN FilterMask;
    UINTN FilterWord;
    ULONG FilterWords;
    PULONG HashBuckets;
    PULONG HashChains;
    PULONG HashTable;
    PELF32_SYMBOL Potential;
    ULONG PotentialHash;
    PSTR PotentialName;
    ULONG RemainingSize;
    ULONG Shift;
    ULONG SymbolBase;
    ULONG SymbolIndex;
    ULONG WordIndex;

    if (Image->ExportSymbolTable == NULL) {
        return NULL;
    }

    //
    // Handle GNU-style hashing.
    //

    if ((Image->Flags & IMAGE_FLAG_GNU_HASH) != 0) {
        HashTable = Image->ExportHashTable;
        BucketCount = *HashTable;
        HashTable += 1;
        SymbolBase = *HashTable;
        HashTable += 1;
        FilterWords = *HashTable;
        HashTable += 1;
        Shift = *HashTable;
        HashTable += 1;
        BucketIndex = Hash % BucketCount;

        //
        // Check the Bloom filter first. The Bloom filter indicates that a
        // symbol is definitely not there, or is maybe there. It basically
        // represents a quick "no".
        // TODO: 64-bit: the word size is 32 or 64 bits.
        //

        Filter = (PUINTN)HashTable;
        HashTable = (PULONG)(Filter + FilterWords);
        WordIndex = (Hash >> ELF_WORD_SIZE_SHIFT) & (FilterWords - 1);

        ASSERT(POWER_OF_2(FilterWords));

        FilterWord = Filter[WordIndex];
        FilterMask = (1 << (Hash & ELF_WORD_SIZE_MASK)) |
                     (1 << ((Hash >> Shift) & ELF_WORD_SIZE_MASK));

        if ((FilterWord & FilterMask) != FilterMask) {
            return NULL;
        }

        //
        // The buckets contain the index of the first symbol with the given
        // hash.
        //

        SymbolIndex = HashTable[BucketIndex];
        if (SymbolIndex == 0) {
            return NULL;
        }

        if (SymbolIndex < SymbolBase) {

            ASSERT(FALSE);

            return NULL;
        }

        //
        // The chains then contain the hashes of each of the symbols, with the
        // low bit cleared. If the low bit is set, then the chain has ended.
        //

        HashChains = HashTable + BucketCount;
        do {
            PotentialHash = HashChains[SymbolIndex - SymbolBase];

            //
            // If the hash matches (ignoring the low bit), then do a full
            // comparison.
            //

            if (((PotentialHash ^ Hash) & ~0x1) == 0) {
                Potential = (PELF32_SYMBOL)Image->ExportSymbolTable +
                            SymbolIndex;

                PotentialName = Image->ExportStringTable +
                                Potential->NameOffset;

                RemainingSize = Image->ExportStringTableSize -
                                Potential->NameOffset;

                Equal = RtlAreStringsEqual(SymbolName,
                                           PotentialName,
                                           RemainingSize);

                if (Equal != FALSE) {
                    return Potential;
                }
            }

            SymbolIndex += 1;

        } while ((PotentialHash & 0x1) == 0);

    //
    // Handle traditional SVR hashing.
    //

    } else {
        BucketCount = *((PULONG)Image->ExportHashTable);
        HashBuckets = (PULONG)Image->ExportHashTable + 2;
        HashChains = (PULONG)Image->ExportHashTable + 2 + BucketCount;
        BucketIndex = Hash % BucketCount;
        SymbolIndex = *(HashBuckets + BucketIndex);
        while (SymbolIndex != 0) {
            Potential = (PELF32_SYMBOL)Image->ExportSymbolTable + SymbolIndex;
            PotentialName = Image->ExportStringTable + Potential->NameOffset;
            RemainingSize = Image->ExportStringTableSize -
                            Potential->NameOffset;

            Equal = RtlAreStringsEqual(SymbolName,
                                       PotentialName,
                                       RemainingSize);

            if (Equal != FALSE) {
                return Potential;
            }

            //
            // Get the next entry in the chain.
            //

            SymbolIndex = *(HashChains + SymbolIndex);
        }
    }

    return NULL;
}

BOOL
ImpElfApplyRelocation (
    PLIST_ENTRY ListHead,
    PLOADED_IMAGE Image,
    PELF32_RELOCATION_ADDEND_ENTRY RelocationEntry,
    BOOL AddendEntry,
    PVOID *FinalSymbolValue
    )

/*++

Routine Description:

    This routine applies a relocation entry to a loaded image.

Arguments:

    ListHead - Supplies a pointer to the head of the list of loaded images.

    Image - Supplies a pointer to the loaded image structure.

    RelocationEntry - Supplies a pointer to the relocation entry. This should
        either be of type PELF32_RELOCATION_ENTRY or
        PELF32_RELOCATION_ADDEND_ENTRY depending on the Addends parameter.

    AddendEntry - Supplies a flag indicating that the entry if of type
        ELF32_RELOCATION_ADDEND_ENTRY, not ELF32_RELOCATION_ENTRY.

    FinalSymbolValue - Supplies an optional pointer where the symbol value will
        be returned on success. This is used by PLT relocations that are being
        fixed up on the fly and also need to jump directly to the symbol
        address.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    ULONG Addend;
    BOOL AddendNeeded;
    UINTN Address;
    UINTN BaseDifference;
    BOOL Copy;
    ULONG Information;
    PELF_LOADING_IMAGE LoadingImage;
    ULONG Offset;
    ULONG Place;
    PVOID RelocationEnd;
    BOOL RelocationNeeded;
    PULONG RelocationPlace;
    ELF32_RELOCATION_TYPE RelocationType;
    PLOADED_IMAGE SymbolImage;
    ULONG SymbolIndex;
    PELF32_SYMBOL Symbols;
    ULONG SymbolValue;

    Address = 0;
    LoadingImage = Image->ImageContext;
    BaseDifference = Image->LoadedLowestAddress - Image->PreferredLowestAddress;
    Offset = RelocationEntry->Offset;
    Information = RelocationEntry->Information;
    Addend = 0;
    if (AddendEntry != FALSE) {
        Addend = RelocationEntry->Addend;
    }

    //
    // The place is the actual VA of the relocation.
    //

    Place = Image->LoadedLowestAddress + Offset - Image->PreferredLowestAddress;

    //
    // The Information field contains both the symbol index to the
    // relocation as well as the type of relocation to apply.
    //

    Symbols = Image->ExportSymbolTable;
    SymbolIndex = Information >> 8;
    RelocationType = Information & 0xFF;

    //
    // Compute the symbol value.
    //

    SymbolValue = ImpElfGetSymbolValue(ListHead,
                                       Image,
                                       &(Symbols[SymbolIndex]),
                                       &SymbolImage,
                                       NULL);

    if (SymbolValue == MAX_ULONG) {
        SymbolValue = 0;
    }

    if (FinalSymbolValue != NULL) {
        *FinalSymbolValue = (PVOID)(UINTN)SymbolValue;
    }

    //
    // Based on the type of relocation, compute the relocated value.
    //

    Copy = FALSE;
    AddendNeeded = TRUE;
    RelocationNeeded = TRUE;
    if (Image->Machine == ImageMachineTypeArm32) {
        switch (RelocationType) {

        //
        // None is a no-op.
        //

        case ElfRelocationNone:
            RelocationNeeded = FALSE;
            break;

        //
        // The "copy" relocations copy data from a shared object into the
        // program's BSS segment. It is used by programs that reference
        // variables defined in a shared object (like stdin/out/err, environ,
        // etc). Note that copy can only work if this loader has access to the
        // source image address. As a clue assert that at least this image's
        // final address is the same as the address this code is accessing it
        // at.
        //

        case ElfArmRelocationCopy:

            ASSERT(Image->LoadedLowestAddress == Image->LoadedImageBuffer);

            //
            // Find the shared object version, not the executable version.
            //

            SymbolValue = ImpElfGetSymbolValue(ListHead,
                                               Image,
                                               &(Symbols[SymbolIndex]),
                                               NULL,
                                               Image);

            if (SymbolValue == MAX_ULONG) {
                SymbolValue = 0;
            }

            Copy = TRUE;
            AddendNeeded = FALSE;
            Address = SymbolValue;
            break;

        //
        // Absolute uses only the symbol's value.
        //

        case ElfArmRelocationAbsolute32:
            Address = SymbolValue + Addend;
            break;

        //
        // Global relocations just use the symbol value. Jump slots are entries
        // in the Procedure Linkage Table (PLT), and also just use the symbol
        // value.
        //

        case ElfArmRelocationGlobalData:
            Address = SymbolValue;
            AddendNeeded = FALSE;
            break;

        case ElfArmRelocationJumpSlot:
            Address = SymbolValue;
            AddendNeeded = FALSE;
            break;

        //
        // Relative relocations just adjust for the new base.
        //

        case ElfArmRelocationRelative:
            Address = BaseDifference + Addend;
            break;

        //
        // This is the module ID.
        //

        case ElfArmRelocationTlsDtpMod32:
            if (SymbolImage == NULL) {
                SymbolImage = Image;
            }

            Address = SymbolImage->ModuleNumber;

            ASSERT(Address != 0);

            AddendNeeded = FALSE;
            break;

        //
        // This is the offset from the start of the TLS image to the given
        // symbol.
        //

        case ElfArmRelocationTlsDtpOff32:
            Address = SymbolValue + Addend;
            AddendNeeded = FALSE;
            break;

        //
        // This is the total offset from the thread pointer to the symbol, as
        // a positive value to be added to the thread pointer.
        //

        case ElfArmRelocationTlsTpOff32:
            if (SymbolImage == NULL) {
                SymbolImage = Image;
            }

            ASSERT((SymbolImage != NULL) &&
                   (SymbolImage->TlsOffset != (UINTN)-1));

            //
            // The TLS offset is a positive value that indicates a negative
            // offset from the thread pointer, hence the subtraction here.
            //

            Address = SymbolValue - SymbolImage->TlsOffset + Addend;
            break;

        //
        // Unknown relocation type.
        //

        default:

            ASSERT(FALSE);

            return FALSE;
        }

    //
    // Handle x86 images.
    //

    } else if (Image->Machine == ImageMachineTypeX86) {
        switch (RelocationType) {

        //
        // None is a no-op.
        //

        case ElfRelocationNone:
            RelocationNeeded = FALSE;
            break;

        //
        // Absolute uses only the symbol's value.
        //

        case Elf386Relocation32:
            Address = SymbolValue + Addend;
            break;

        //
        // PC32 is Symbol + Addend - Place
        //

        case Elf386RelocationPc32:
            Address = SymbolValue + Addend - Place;
            break;

        //
        // The "copy" relocations copy data from a shared object into the
        // program's BSS segment. It is used by programs that reference
        // variables defined in a shared object (like stdin/out/err, environ,
        // etc). Note that copy can only work if this loader has access to the
        // source image address. As a clue assert that at least this image's
        // final address is the same as the address this code is accessing it
        // at.
        //

        case Elf386RelocationCopy:

            ASSERT(Image->LoadedLowestAddress == Image->LoadedImageBuffer);

            //
            // Find the shared object version, not the executable version.
            //

            SymbolValue = ImpElfGetSymbolValue(ListHead,
                                               Image,
                                               &(Symbols[SymbolIndex]),
                                               NULL,
                                               Image);

            if (SymbolValue == MAX_ULONG) {
                SymbolValue = 0;
            }

            Copy = TRUE;
            AddendNeeded = FALSE;
            Address = SymbolValue;
            break;

        //
        // Global relocations just use the symbol value. Jump slots are entries
        // in the Procedure Linkage Table (PLT), and also just use the symbol
        // value.
        //

        case Elf386RelocationGlobalData:
            Address = SymbolValue;
            AddendNeeded = FALSE;
            break;

        case Elf386RelocationJumpSlot:
            Address = SymbolValue;
            AddendNeeded = FALSE;
            break;

        //
        // Relative relocations just adjust for the new base.
        //

        case Elf386RelocationRelative:
            Address = BaseDifference + Addend;
            break;

        //
        // This is the module ID.
        //

        case Elf386RelocationTlsDtpMod32:
            if (SymbolImage == NULL) {
                SymbolImage = Image;
            }

            Address = SymbolImage->ModuleNumber;

            ASSERT(Address != 0);

            AddendNeeded = FALSE;
            break;

        //
        // This is the offset from the start of the TLS image to the given
        // symbol.
        //

        case Elf386RelocationTlsDtpOff32:
            Address = SymbolValue + Addend;
            AddendNeeded = FALSE;
            break;

        //
        // This is the total offset from the thread pointer to the symbol, as
        // a negative value to be added to the thread pointer.
        //

        case Elf386RelocationTlsTpOff:
            if (SymbolImage == NULL) {
                SymbolImage = Image;
            }

            ASSERT((SymbolImage != NULL) &&
                   (SymbolImage->TlsOffset != (UINTN)-1));

            Address = SymbolValue - SymbolImage->TlsOffset + Addend;
            break;

        //
        // This is the total offset from the thread pointer to the symbol, as
        // a positive value to be subtracted from the thread pointer.
        //

        case Elf386RelocationTlsTpOff32:
            if (SymbolImage == NULL) {
                SymbolImage = Image;
            }

            ASSERT((SymbolImage != NULL) &&
                   (SymbolImage->TlsOffset != (UINTN)-1));

            Address = SymbolImage->TlsOffset - SymbolValue + Addend;
            break;

        //
        // Unknown relocation type.
        //

        default:

            ASSERT(FALSE);

            return FALSE;
        }

    } else {

        ASSERT(FALSE);

        return FALSE;
    }

    //
    // If a relocation is needed, apply it now.
    //

    if (RelocationNeeded != FALSE) {
        RelocationPlace = (PULONG)((PUCHAR)Image->LoadedImageBuffer +
                           (Offset - (UINTN)Image->PreferredLowestAddress));

        if (AddendNeeded != FALSE) {
            Address += *RelocationPlace;
        }

        if (Copy != FALSE) {
            RtlCopyMemory(RelocationPlace,
                          (PVOID)Address,
                          Symbols[SymbolIndex].Size);

            RelocationEnd = (PVOID)RelocationPlace +
                            Symbols[SymbolIndex].Size;

        } else {
            *RelocationPlace = Address;
            RelocationEnd = RelocationPlace + 1;
        }

        if ((LoadingImage != NULL) &&
            ((Image->Flags & IMAGE_FLAG_TEXT_RELOCATIONS) != 0)) {

            if ((LoadingImage->RelocationStart == ELF_INVALID_RELOCATION) ||
                (LoadingImage->RelocationStart > (PVOID)RelocationPlace)) {

                LoadingImage->RelocationStart = RelocationPlace;
            }

            if ((LoadingImage->RelocationEnd == ELF_INVALID_RELOCATION) ||
                (LoadingImage->RelocationEnd < RelocationEnd)) {

                LoadingImage->RelocationEnd = RelocationEnd;
            }
        }
    }

    return TRUE;
}

ULONG
ImpElfOriginalHash (
    PSTR SymbolName
    )

/*++

Routine Description:

    This routine hashes a symbol name to get the index into the ELF hash table.

Arguments:

    SymbolName - Supplies a pointer to the name to hash.

Return Value:

    Returns the hash of the name.

--*/

{

    ULONG Hash;
    ULONG Temporary;

    Hash = 0;
    while (*SymbolName != '\0') {
        Hash = (Hash << 4) + *SymbolName;
        Temporary = Hash & 0xF0000000;
        if (Temporary != 0) {
            Hash ^= Temporary >> 24;
        }

        Hash &= ~Temporary;
        SymbolName += 1;
    }

    return Hash;
}

ULONG
ImpElfGnuHash (
    PSTR SymbolName
    )

/*++

Routine Description:

    This routine hashes a symbol name to get the index into the ELF hash table
    using the GNU style hash function.

Arguments:

    SymbolName - Supplies a pointer to the name to hash.

Return Value:

    Returns the hash of the name.

--*/

{

    ULONG Hash;

    Hash = 5381;
    while (*SymbolName != '\0') {

        //
        // It's really hash * 33 + Character, but multiply by 33 is expanded
        // into multiply by 32 plus one.
        //

        Hash = ((Hash << 5) + Hash) + (UCHAR)*SymbolName;
        SymbolName += 1;
    }

    return Hash;
}

PSTR
ImpElfGetEnvironmentVariable (
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

    if (ImGetEnvironmentVariable != NULL) {
        return ImGetEnvironmentVariable(Variable);
    }

    return NULL;
}

KSTATUS
ImpElfPerformLibraryPathSubstitutions (
    PLOADED_IMAGE Image,
    PSTR *Path,
    PUINTN PathCapacity
    )

/*++

Routine Description:

    This routine performs any variable substitutions in a library path.

Arguments:

    Image - Supplies a pointer to the image loading the library (not the
        library itself obviously, that hasn't been loaded yet).

    Path - Supplies a pointer that on input contains the complete path. On
        output this will contain the complete path with variables expanded.

    PathCapacity - Supplies a pointer that on input contains the size of the
        path allocation. This will be updated on output if the string is
        reallocated.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES if memory could not be allocated.

--*/

{

    PSTR CurrentPath;
    PSTR CurrentVariable;
    UINTN Delta;
    PELF_LIBRARY_PATH_VARIABLE_ENTRY Entry;
    UINTN EntryCount;
    UINTN EntryIndex;
    UINTN Index;
    PSTR Name;
    UINTN NameLength;
    PSTR NewBuffer;
    PSTR Replacement;
    UINTN ReplacementLength;
    UINTN ReplaceSize;
    UINTN ReplaceStart;
    KSTATUS Status;

    EntryCount = sizeof(ElfLibraryPathVariables) /
                 sizeof(ElfLibraryPathVariables[0]);

    CurrentVariable = RtlStringFindCharacter(*Path, '$', -1);
    while (CurrentVariable != NULL) {

        //
        // Find the name of the variable and the size of the region to replace.
        //

        ReplaceStart = (UINTN)CurrentVariable - (UINTN)(*Path);
        CurrentVariable += 1;
        if (*CurrentVariable == '{') {
            CurrentVariable += 1;
            Name = CurrentVariable;
            while ((*CurrentVariable != '\0') && (*CurrentVariable != '}')) {
                CurrentVariable += 1;
            }

            if (*CurrentVariable != '}') {
                RtlDebugPrint("ELF: Missing closing brace on %s.\n", *Path);
                Status = STATUS_INVALID_SEQUENCE;
                goto ElfPerformLibraryPathSubstitutionsEnd;
            }

            NameLength = (UINTN)CurrentVariable - (UINTN)Name;
            CurrentVariable += 1;

        } else {
            Name = CurrentVariable;
            while (RtlIsCharacterAlphabetic(*CurrentVariable) != FALSE) {
                CurrentVariable += 1;
            }

            NameLength = (UINTN)CurrentVariable - (UINTN)Name;
        }

        ReplaceSize = (UINTN)CurrentVariable - (UINTN)ReplaceStart;

        //
        // Decode the variable.
        //

        for (EntryIndex = 0; EntryIndex < EntryCount; EntryIndex += 1) {
            Entry = &(ElfLibraryPathVariables[EntryIndex]);
            if ((RtlAreStringsEqual(Name, Entry->Name, NameLength) != FALSE) &&
                (Entry->Name[NameLength] == '\0')) {

                break;
            }
        }

        if (EntryIndex == EntryCount) {
            RtlDebugPrint("ELF: Warning: Unknown variable starting at %s.\n",
                          Name);

        } else {

            //
            // TODO: Get the correct variable values.
            //

            ASSERT(FALSE);

            switch (Entry->Variable) {
            case ElfLibraryPathOrigin:
                Replacement = ".";
                break;

            case ElfLibraryPathLib:
                Replacement = "lib";
                break;

            case ElfLibraryPathPlatform:
                Replacement = "i386";
                break;

            default:

                ASSERT(FALSE);

                Replacement = ".";
                break;
            }

            ReplacementLength = RtlStringLength(Replacement);

            //
            // If the replacement is shorter than the original, then just
            // copy the replacement over followed by the rest.
            //

            if (ReplacementLength <= ReplaceSize) {
                CurrentPath = *Path;
                RtlCopyMemory(CurrentPath + ReplaceStart,
                              Replacement,
                              ReplacementLength);

                Delta = ReplaceSize - ReplacementLength;
                if (Delta != 0) {
                    for (Index = ReplaceStart + ReplaceSize;
                         Index < *PathCapacity - Delta;
                         Index += 1) {

                        CurrentPath[Index] = CurrentPath[Index + Delta];
                    }

                    CurrentVariable -= Delta;
                }

            //
            // The replacement is bigger than the region it's replacing.
            //

            } else {
                Delta = ReplacementLength - ReplaceSize;
                NewBuffer = ImAllocateMemory(*PathCapacity + Delta,
                                             IM_ALLOCATION_TAG);

                if (NewBuffer == NULL) {
                    Status = STATUS_INSUFFICIENT_RESOURCES;
                    goto ElfPerformLibraryPathSubstitutionsEnd;
                }

                RtlCopyMemory(NewBuffer, *Path, ReplaceStart);
                RtlCopyMemory(NewBuffer + ReplaceStart,
                              Replacement,
                              ReplacementLength);

                RtlCopyMemory(NewBuffer + ReplaceStart + ReplacementLength,
                              *Path + ReplaceSize,
                              *PathCapacity - (ReplaceStart + ReplaceSize));

                CurrentVariable = (PSTR)((UINTN)CurrentVariable -
                                         (UINTN)(*Path) +
                                         (UINTN)NewBuffer);

                ImFreeMemory(*Path);
                *PathCapacity += Delta;
            }
        }

        //
        // Find the next variable.
        //

        CurrentVariable = RtlStringFindCharacter(CurrentVariable, '$', -1);
    }

    Status = STATUS_SUCCESS;

ElfPerformLibraryPathSubstitutionsEnd:
    return Status;
}

VOID
ImpElfFreeContext (
    PLOADED_IMAGE Image
    )

/*++

Routine Description:

    This routine frees the loading image context, and closes and unloads the
    file.

Arguments:

    Image - Supplies a pointer to the image that has finished loading.

Return Value:

    None.

--*/

{

    PELF_LOADING_IMAGE LoadingImage;

    if (Image->ImageContext != NULL) {
        LoadingImage = Image->ImageContext;
        if (Image->File.Handle != INVALID_HANDLE) {
            if (LoadingImage->Buffer.Data != NULL) {
                ImUnloadBuffer(&(Image->File), &(LoadingImage->Buffer));
            }

            ImCloseFile(&(Image->File));
            Image->File.Handle = INVALID_HANDLE;
        }

        LoadingImage->Buffer.Data = NULL;
        LoadingImage = NULL;
        ImFreeMemory(Image->ImageContext);
        Image->ImageContext = NULL;
    }

    return;
}

