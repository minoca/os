/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    elf.c

Abstract:

    This module implements support for handling the ELF file format.

Author:

    Evan Green 13-Oct-2012

Environment:

    Any

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "imp.h"
#include "elf.h"
#include "elfn.h"
#include "elfcomm.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the function names here to 32 and 64 bit specific function names,
// since this file may be compiled twice in the same executable.
//

#if defined(WANT_ELF64)

//
// Define structure aliases.
//

#define ELF_LOADING_IMAGE ELF64_LOADING_IMAGE
#define _ELF_LOADING_IMAGE _ELF64_LOADING_IMAGE

#define PELF_LOADING_IMAGE PELF64_LOADING_IMAGE

//
// Define function aliases.
//

#define ImpElfLoadImportsForImage ImpElf64LoadImportsForImage
#define ImpElfLoadImport ImpElf64LoadImport
#define ImpElfGatherExportInformation ImpElf64GatherExportInformation
#define ImpElfGetDynamicEntry ImpElf64GetDynamicEntry
#define ImpElfRelocateImage ImpElf64RelocateImage
#define ImpElfProcessRelocateSection ImpElf64ProcessRelocateSection
#define ImpElfAdjustJumpSlots ImpElf64AdjustJumpSlots
#define ImpElfGetSymbolValue ImpElf64GetSymbolValue
#define ImpElfGetSymbolInScope ImpElf64GetSymbolInScope
#define ImpElfGetSymbol ImpElf64GetSymbol
#define ImpElfApplyRelocation ImpElf64ApplyRelocation
#define ImpElfFreeContext ImpElf64FreeContext

#else

//
// Define structure aliases.
//

#define ELF_LOADING_IMAGE ELF32_LOADING_IMAGE
#define _ELF_LOADING_IMAGE _ELF32_LOADING_IMAGE

#define PELF_LOADING_IMAGE PELF32_LOADING_IMAGE

//
// Define function aliases.
//

#define ImpElfLoadImportsForImage ImpElf32LoadImportsForImage
#define ImpElfLoadImport ImpElf32LoadImport
#define ImpElfGatherExportInformation ImpElf32GatherExportInformation
#define ImpElfGetDynamicEntry ImpElf32GetDynamicEntry
#define ImpElfRelocateImage ImpElf32RelocateImage
#define ImpElfProcessRelocateSection ImpElf32ProcessRelocateSection
#define ImpElfAdjustJumpSlots ImpElf32AdjustJumpSlots
#define ImpElfGetSymbolValue ImpElf32GetSymbolValue
#define ImpElfGetSymbolInScope ImpElf32GetSymbolInScope
#define ImpElfGetSymbol ImpElf32GetSymbol
#define ImpElfApplyRelocation ImpElf32ApplyRelocation
#define ImpElfFreeContext ImpElf32FreeContext

#endif

//
// Try some magically built-in library paths.
//

#define ELF_BUILTIN_LIBRARY_PATH "/lib:/usr/lib:/usr/local/lib"

//
// Define an invalid address value for image relocation tracking.
//

#define ELF_INVALID_RELOCATION (PVOID)-1
#define ELF_INVALID_ADDRESS (ELF_ADDR)-1ULL

//
// Define the maximum number of program headers before it's just silly.
//

#define ELF_MAX_PROGRAM_HEADERS 50

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores state variables used while loading an ELF image.

Members:

    Buffer - Stores the loaded image buffer.

    ElfHeader - Stores a pointer pointing inside the file buffer where the
        main ELF header resides.

    RelocationStart - Stores the lowest address to be modified during image
        relocation.

    RelocationEnd - Stores the address at the end of the highest image
        relocation.

--*/

typedef struct _ELF_LOADING_IMAGE {
    IMAGE_BUFFER Buffer;
    PELF_HEADER ElfHeader;
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
ImpElfGatherExportInformation (
    PLOADED_IMAGE Image,
    PIM_RESOLVE_PLT_ENTRY PltResolver,
    BOOL UseLoadedAddress
    );

PELF_DYNAMIC_ENTRY
ImpElfGetDynamicEntry (
    PLOADED_IMAGE Image,
    ELF_SXWORD Tag
    );

KSTATUS
ImpElfRelocateImage (
    PLOADED_IMAGE Image
    );

KSTATUS
ImpElfProcessRelocateSection (
    PLOADED_IMAGE Image,
    PVOID Relocations,
    ELF_XWORD RelocationsSize,
    BOOL Addends
    );

VOID
ImpElfAdjustJumpSlots (
    PLOADED_IMAGE Image,
    PVOID Relocations,
    ELF_XWORD RelocationsSize,
    BOOL Addends
    );

ELF_ADDR
ImpElfGetSymbolValue (
    PLOADED_IMAGE Image,
    PELF_SYMBOL Symbol,
    PLOADED_IMAGE *FoundImage,
    PLOADED_IMAGE SkipImage
    );

PELF_SYMBOL
ImpElfGetSymbolInScope (
    PLOADED_IMAGE ScopeImage,
    PLOADED_IMAGE Skip,
    PCSTR SymbolName,
    PLOADED_IMAGE *FoundImage
    );

PELF_SYMBOL
ImpElfGetSymbol (
    PLOADED_IMAGE Image,
    ULONG Hash,
    PCSTR SymbolName
    );

BOOL
ImpElfApplyRelocation (
    PLOADED_IMAGE Image,
    PELF_RELOCATION_ADDEND_ENTRY RelocationEntry,
    BOOL AddendEntry,
    PVOID *FinalSymbolValue
    );

VOID
ImpElfFreeContext (
    PLOADED_IMAGE Image
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
ImpElfOpenLibrary (
    PLIST_ENTRY ListHead,
    PLOADED_IMAGE Parent,
    PCSTR LibraryName,
    PIMAGE_FILE_INFORMATION File,
    PSTR *Path
    )

/*++

Routine Description:

    This routine attempts to open a dynamic library.

Arguments:

    ListHead - Supplies an optional pointer to the head of the list of loaded
        images.

    Parent - Supplies a pointer to the parent image requiring this image for
        load.

    LibraryName - Supplies the name of the library to open.

    File - Supplies a pointer where the information for the file including its
        open handle will be returned.

    Path - Supplies a pointer where the real path to the opened file will be
        returned. The caller is responsible for freeing this memory.

Return Value:

    Status code.

--*/

{

    PELF_DYNAMIC_ENTRY ParentRunPath;
    PSTR PathList;
    PLOADED_IMAGE PrimaryExecutable;
    PELF_DYNAMIC_ENTRY RPath;
    PLOADED_IMAGE RPathParent;
    PLOADED_IMAGE RPathRoot;
    PELF_DYNAMIC_ENTRY RunPath;
    PSTR Slash;
    KSTATUS Status;

    //
    // If there's a slash, then just load the library without paths.
    //

    Slash = RtlStringFindCharacter(LibraryName, '/', -1);
    if (Slash != NULL) {
        Status = ImpElfOpenWithPathList(Parent, LibraryName, "", File, Path);
        goto OpenLibraryEnd;
    }

    //
    // First find a DT_RUNPATH. If both DT_RUNPATH and DT_RPATH are found,
    // ignore the older DT_RPATH. DT_RPATH goes up the chain of imports.
    //

    ParentRunPath = ImpElfGetDynamicEntry(Parent, ELF_DYNAMIC_RUN_PATH);
    if (ParentRunPath == NULL) {
        RPathRoot = NULL;
        RPathParent = Parent;
        while (RPathParent != NULL) {
            RunPath = ImpElfGetDynamicEntry(RPathParent, ELF_DYNAMIC_RUN_PATH);
            if (RunPath == NULL) {
                RPath = ImpElfGetDynamicEntry(RPathParent, ELF_DYNAMIC_RPATH);
                if (RPath != NULL) {
                    PathList = RPathParent->ExportStringTable + RPath->Value;
                    Status = ImpElfOpenWithPathList(RPathParent,
                                                    LibraryName,
                                                    PathList,
                                                    File,
                                                    Path);

                    if (KSUCCESS(Status)) {
                        goto OpenLibraryEnd;
                    }
                }
            }

            RPathRoot = RPathParent;
            RPathParent = RPathParent->Parent;
        }

        //
        // Try the DT_RPATH of the primary executable if provided and not
        // already searched.
        //

        PrimaryExecutable = ImPrimaryExecutable;
        if ((PrimaryExecutable != NULL) &&
            (PrimaryExecutable != RPathRoot) &&
            (PrimaryExecutable->DynamicSection != NULL)) {

            RunPath = ImpElfGetDynamicEntry(PrimaryExecutable,
                                            ELF_DYNAMIC_RUN_PATH);

            if (RunPath == NULL) {
                RPath = ImpElfGetDynamicEntry(PrimaryExecutable,
                                              ELF_DYNAMIC_RPATH);

                if (RPath != NULL) {
                    PathList = PrimaryExecutable->ExportStringTable +
                               RPath->Value;

                    Status = ImpElfOpenWithPathList(PrimaryExecutable,
                                                    LibraryName,
                                                    PathList,
                                                    File,
                                                    Path);

                    if (KSUCCESS(Status)) {
                        goto OpenLibraryEnd;
                    }
                }
            }
        }
    }

    //
    // Get the library search path variable and use that.
    //

    PathList = ImpElfGetEnvironmentVariable(IMAGE_LOAD_LIBRARY_PATH_VARIABLE);
    if (PathList != NULL) {
        Status = ImpElfOpenWithPathList(Parent,
                                        LibraryName,
                                        PathList,
                                        File,
                                        Path);

        if (KSUCCESS(Status)) {
            goto OpenLibraryEnd;
        }
    }

    //
    // Try DT_RUNPATH.
    //

    if (ParentRunPath != NULL) {
        PathList = Parent->ExportStringTable + ParentRunPath->Value;
        Status = ImpElfOpenWithPathList(Parent,
                                        LibraryName,
                                        PathList,
                                        File,
                                        Path);

        if (KSUCCESS(Status)) {
            goto OpenLibraryEnd;
        }
    }

    //
    // Try some hard coded paths.
    //

    PathList = ELF_BUILTIN_LIBRARY_PATH;
    Status = ImpElfOpenWithPathList(Parent, LibraryName, PathList, File, Path);
    if (KSUCCESS(Status)) {
        goto OpenLibraryEnd;
    }

OpenLibraryEnd:
    return Status;
}

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

    PELF_HEADER ElfHeader;
    PELF_PROGRAM_HEADER FirstProgramHeader;
    ELF_WORD HeaderSize;
    ELF_ADDR HighestVirtualAddress;
    ELF_ADDR ImageSize;
    PSTR InterpreterName;
    ELF_ADDR LowestVirtualAddress;
    PELF_PROGRAM_HEADER ProgramHeader;
    BOOL Result;
    ELF_ADDR SegmentBase;
    ELF_OFF SegmentCount;
    ELF_ADDR SegmentEnd;
    UINTN SegmentIndex;
    KSTATUS Status;

    ImageSize = 0;
    if (InterpreterPath != NULL) {
        *InterpreterPath = NULL;
    }

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

    Image->Format = ImageElfNative;
    switch (ElfHeader->Machine) {
    case ELF_MACHINE_ARM:
        Image->Machine = ImageMachineTypeArm32;
        break;

    case ELF_MACHINE_I386:
        Image->Machine = ImageMachineTypeX86;
        break;

    case ELF_MACHINE_X86_64:
        Image->Machine = ImageMachineTypeX64;
        break;

    case ELF_MACHINE_AARCH64:
        Image->Machine = ImageMachineTypeArm64;
        break;

    default:
        Image->Machine = ImageMachineTypeUnknown;
        break;
    }

    Image->EntryPoint = (PVOID)(UINTN)(ElfHeader->EntryPoint);

    //
    // Loop through the program headers once to get the image size and base
    // address.
    //

    LowestVirtualAddress = (ELF_ADDR)-1ULL;
    HighestVirtualAddress = 0;
    for (SegmentIndex = 0; SegmentIndex < SegmentCount; SegmentIndex += 1) {
        ProgramHeader =
            (PELF_PROGRAM_HEADER)(((PUCHAR)FirstProgramHeader) +
                                (SegmentIndex * ElfHeader->ProgramHeaderSize));

        //
        // If this image is requesting an interpreter, go load the interpreter
        // instead of this image.
        //

        if ((ProgramHeader->Type == ELF_SEGMENT_TYPE_INTERPRETER) &&
            (ProgramHeader->FileSize != 0) &&
            (InterpreterPath != NULL) &&
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
    Image->PreferredLowestAddress = (PVOID)(UINTN)LowestVirtualAddress;
    Status = STATUS_SUCCESS;

GetImageSizeEnd:
    Image->Size = ImageSize;
    return Status;
}

KSTATUS
ImpElfLoadImage (
    PLIST_ENTRY ListHead,
    PLOADED_IMAGE Image,
    PIMAGE_BUFFER Buffer
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

Return Value:

    STATUS_SUCCESS on success.

    STATUS_FILE_CORRUPT if the file headers were corrupt or unexpected.

    Other errors on failure.

--*/

{

    ELF_ADDR BaseDifference;
    PELF_HEADER ElfHeader;
    PELF_PROGRAM_HEADER FirstProgramHeader;
    BOOL ImageInserted;
    ULONG ImportIndex;
    PELF_LOADING_IMAGE LoadingImage;
    BOOL NotifyLoadCalled;
    PIMAGE_SEGMENT PreviousSegment;
    PELF_PROGRAM_HEADER ProgramHeader;
    BOOL Result;
    PIMAGE_SEGMENT Segment;
    ELF_ADDR SegmentBase;
    ELF_HALF SegmentCount;
    ELF_HALF SegmentIndex;
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
    BaseDifference = Image->BaseDifference;
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

        Segment->VirtualAddress = (PVOID)(UINTN)SegmentBase + BaseDifference;
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

        Status = ImMapImageSegment(
                                Image->AllocatorHandle,
                                Image->PreferredLowestAddress + BaseDifference,
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

            Image->BaseDifference = Segment->VirtualAddress -
                                    Image->PreferredLowestAddress;

            Image->LoadedImageBuffer = Segment->VirtualAddress;
            BaseDifference = Image->BaseDifference;
        }

        Segment->Type = ImageSegmentFileSection;
        PreviousSegment = Segment;
        ProgramHeader = (PELF_PROGRAM_HEADER)((PUCHAR)ProgramHeader +
                                              ElfHeader->ProgramHeaderSize);
    }

    Image->EntryPoint =
          (PVOID)(UINTN)(LoadingImage->ElfHeader->EntryPoint + BaseDifference);

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
    // If this image is really being loaded and is the primary executable,
    // set it in the global.
    //

    if ((Image->LoadFlags & IMAGE_LOAD_FLAG_PRIMARY_EXECUTABLE) != 0) {

        ASSERT(ImPrimaryExecutable == NULL);

        ImPrimaryExecutable = Image;
    }

    //
    // Gather information not in the loaded part of the file needed for
    // resolving exports from this image.
    //

    Status = ImpElfGatherExportInformation(Image,
                                           ImImportTable->ResolvePltEntry,
                                           FALSE);

    if (!KSUCCESS(Status)) {
        goto LoadImageEnd;
    }

    //
    // If the import count is non-zero, then this is an import being loaded.
    // Do nothing else, as relocations and imports happen at the base level.
    //

    if (Image->ImportDepth != 0) {
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

    ELF_ADDR BaseDifference;
    PELF_HEADER ElfHeader;
    PELF_PROGRAM_HEADER FirstProgramHeader;
    ELF_ADDR HighestVirtualAddress;
    ELF_ADDR ImageSize;
    UINTN Index;
    PELF_LOADING_IMAGE LoadingImage;
    ELF_ADDR LowestVirtualAddress;
    PELF_PROGRAM_HEADER ProgramHeader;
    ELF_HALF SegmentCount;
    ELF_ADDR SegmentEnd;
    KSTATUS Status;

    ASSERT(Image->Format == ImageElfNative);

    ElfHeader = Image->LoadedImageBuffer;
    Image->Size = ImageBuffer->Size;
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

    case ELF_MACHINE_X86_64:
        Image->Machine = ImageMachineTypeX64;
        break;

    case ELF_MACHINE_AARCH64:
        Image->Machine = ImageMachineTypeArm64;
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

    LowestVirtualAddress = (ELF_ADDR)-1ULL;
    HighestVirtualAddress = 0;
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

            SegmentEnd = ProgramHeader->VirtualAddress +
                         ProgramHeader->MemorySize;

            if (SegmentEnd > HighestVirtualAddress) {
                HighestVirtualAddress = SegmentEnd;
            }
        }

        ProgramHeader = (PELF_PROGRAM_HEADER)((PUCHAR)ProgramHeader +
                                              ElfHeader->ProgramHeaderSize);
    }

    if (LowestVirtualAddress >= HighestVirtualAddress) {
        Status = STATUS_UNKNOWN_IMAGE_FORMAT;
        goto AddImageEnd;
    }

    ImageSize = HighestVirtualAddress - LowestVirtualAddress;

    ASSERT((Image->Size == MAX_UINTN) || (Image->Size == ImageSize));

    Image->Size = ImageSize;
    Image->PreferredLowestAddress = (PVOID)(UINTN)LowestVirtualAddress;
    BaseDifference = Image->LoadedImageBuffer - Image->PreferredLowestAddress;
    Image->BaseDifference = BaseDifference;
    if (Image->TlsImage != NULL) {
        Image->TlsImage += BaseDifference;
    }

    Image->EntryPoint = (PVOID)(UINTN)(ElfHeader->EntryPoint + BaseDifference);
    Status = ImpElfGatherExportInformation(Image,
                                           ImImportTable->ResolvePltEntry,
                                           TRUE);

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

    UINTN ImportIndex;
    PLOADED_IMAGE PrimaryExecutable;
    UINTN SegmentIndex;

    ASSERT((Image->ImportCount == 0) || (Image->Imports != NULL));
    ASSERT(Image->Format == ImageElfNative);

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

    //
    // Remove the image from the global scope if it was there.
    //

    PrimaryExecutable = ImPrimaryExecutable;
    if (PrimaryExecutable != NULL) {
        for (ImportIndex = 0;
             ImportIndex < PrimaryExecutable->ScopeSize;
             ImportIndex += 1) {

            //
            // If found, copy everything else down.
            //

            if (PrimaryExecutable->Scope[ImportIndex] == Image) {
                while (ImportIndex < PrimaryExecutable->ScopeSize - 1) {
                    PrimaryExecutable->Scope[ImportIndex] =
                                     PrimaryExecutable->Scope[ImportIndex + 1];

                    ImportIndex += 1;
                }

                PrimaryExecutable->ScopeSize -= 1;
                break;
            }
        }
    }

    return;
}

BOOL
ImpElfGetHeader (
    PIMAGE_BUFFER Buffer,
    PELF_HEADER *ElfHeader
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

    UCHAR Class;
    PELF_HEADER Header;

    *ElfHeader = NULL;
    Header = ImpReadBuffer(NULL, Buffer, 0, sizeof(ELF_HEADER));
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
    // Check that the 32/64 bitness agrees.
    //

    Class = Header->Identification[ELF_CLASS_OFFSET];
    if (sizeof(ELF_HEADER) == sizeof(ELF64_HEADER)) {
        if (Class != ELF_64BIT) {
            return FALSE;
        }

    } else {
        if (Class != ELF_32BIT) {
            return FALSE;
        }
    }

    //
    // Only little endian images are supported.
    //

    if (Header->Identification[ELF_ENDIANNESS_OFFSET] != ELF_LITTLE_ENDIAN) {
        return FALSE;
    }

    //
    // Ensure that the program header and section header sizes are consistent.
    //

    if ((Header->ProgramHeaderSize != sizeof(ELF_PROGRAM_HEADER)) ||
        (Header->SectionHeaderSize != sizeof(ELF_SECTION_HEADER))) {

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
    PELF_HEADER ElfHeader;
    BOOL Match;
    BOOL Result;
    PVOID ReturnSection;
    ELF_WORD ReturnSectionFileSize;
    ELF_WORD ReturnSectionMemorySize;
    ELF_ADDR ReturnSectionVirtualAddress;
    PELF_SECTION_HEADER SectionHeader;
    ELF_HALF SectionIndex;
    PSTR StringTable;
    PELF_SECTION_HEADER StringTableHeader;

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
                 sizeof(ELF_SECTION_HEADER) * ElfHeader->SectionHeaderCount);

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
        Match = RtlAreStringsEqual(CurrentSectionName, SectionName, -1);

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

        ASSERT((CurrentImage->Format == ImageElfNative) &&
               ((CurrentImage->LoadFlags & IMAGE_LOAD_FLAG_LOAD_ONLY) == 0));

        if ((CurrentImage->Flags & IMAGE_FLAG_IMPORTS_LOADED) == 0) {
            Status = ImpElfLoadImportsForImage(ListHead, CurrentImage);
            if (!KSUCCESS(Status)) {
                goto LoadAllImportsEnd;
            }

            CurrentImage->Flags |= IMAGE_FLAG_IMPORTS_LOADED;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    //
    // Loop through again and create the depth first scope for this image.
    //

    CurrentEntry = ListHead->Next;
    while (CurrentEntry != ListHead) {
        CurrentImage = LIST_VALUE(CurrentEntry, LOADED_IMAGE, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        Status = ImpAddImageToScope(CurrentImage, CurrentImage);
        if (!KSUCCESS(Status)) {
            goto LoadAllImportsEnd;
        }

        //
        // If it's a global image, add this image and its dependencies to
        // the global scope as well.
        //

        if ((CurrentImage->LoadFlags & IMAGE_LOAD_FLAG_GLOBAL) != 0) {
            if (ImPrimaryExecutable != NULL) {
                Status = ImpAddImageToScope(ImPrimaryExecutable,
                                            CurrentImage);

                if (!KSUCCESS(Status)) {
                    goto LoadAllImportsEnd;
                }
            }
        }
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

    //
    // Iterate backwards because a copy relocation in the executable might
    // copy a portion of a shared library that has relocations inside it. So
    // the relocations in that region need to be fixed up before the copy.
    //

    CurrentEntry = ListHead->Previous;
    while (CurrentEntry != ListHead) {
        CurrentImage = LIST_VALUE(CurrentEntry, LOADED_IMAGE, ListEntry);

        ASSERT(CurrentImage->Format == ImageElfNative);

        if ((CurrentImage->Flags & IMAGE_FLAG_RELOCATED) == 0) {
            Status = ImpElfRelocateImage(CurrentImage);
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

        CurrentEntry = CurrentEntry->Previous;
    }

    Status = STATUS_SUCCESS;

RelocateImagesEnd:
    return Status;
}

VOID
ImpElfRelocateSelf (
    PIMAGE_BUFFER Buffer,
    PIM_RESOLVE_PLT_ENTRY PltResolver,
    PLOADED_IMAGE Image
    )

/*++

Routine Description:

    This routine relocates the currently running image.

Arguments:

    Buffer - Supplies a pointer to the image buffer.

    PltResolver - Supplies a pointer to the function used to resolve PLT
        entries.

    Image - Supplies a pointer to the zeroed but otherwise uninitialized
        image buffer.

Return Value:

    None.

--*/

{

    LIST_ENTRY FakeList;
    PSTR InterpreterPath;
    ELF_LOADING_IMAGE LoadingImage;
    KSTATUS Status;

    //
    // Create a fake image list, and a fake ELF loading image context.
    //

    INITIALIZE_LIST_HEAD(&FakeList);
    RtlZeroMemory(&LoadingImage, sizeof(ELF_LOADING_IMAGE));
    RtlCopyMemory(&(LoadingImage.Buffer), Buffer, sizeof(IMAGE_BUFFER));

    //
    // Set the "no static constructors" flag so that gather export information
    // doesn't try to allocate a static functions structure.
    //

    Image->LoadFlags = IMAGE_LOAD_FLAG_NO_STATIC_CONSTRUCTORS |
                       IMAGE_LOAD_FLAG_IGNORE_INTERPRETER;

    Status = ImpElfGetImageSize(&FakeList, Image, Buffer, &InterpreterPath);
    if (!KSUCCESS(Status)) {
        goto ElfRelocateSelfEnd;
    }

    Image->File.Size = Image->Size;
    LoadingImage.Buffer.Size = Image->Size;
    LoadingImage.ElfHeader = Buffer->Data;
    Image->BaseDifference = Buffer->Data - Image->PreferredLowestAddress;
    Image->LoadedImageBuffer = Buffer->Data;
    Image->ImageContext = &LoadingImage;
    Status = ImpElfGatherExportInformation(Image, PltResolver, TRUE);
    if (!KSUCCESS(Status)) {
        goto ElfRelocateSelfEnd;
    }

    Status = ImpElfRelocateImage(Image);

ElfRelocateSelfEnd:
    Image->ImageContext = NULL;

    ASSERT(KSUCCESS(Status));

    return;
}

KSTATUS
ImpElfGetSymbolByName (
    PLOADED_IMAGE Image,
    PSTR SymbolName,
    PLOADED_IMAGE Skip,
    PIMAGE_SYMBOL Symbol
    )

/*++

Routine Description:

    This routine attempts to find an exported symbol with the given name in the
    given binary.

Arguments:

    Image - Supplies a pointer to the image to query.

    SymbolName - Supplies a pointer to the string containing the name of the
        symbol to search for.

    Skip - Supplies an optional pointer to an image to skip when searching.

    Symbol - Supplies a pointer to a structure that receives the symbol's
        information on success.

Return Value:

    Status code.

--*/

{

    PELF_SYMBOL ElfSymbol;
    PLOADED_IMAGE FoundImage;
    ELF_SYMBOL_TYPE SymbolType;
    ELF_ADDR Value;

    ASSERT(Image->Format == ImageElfNative);

    ElfSymbol = ImpElfGetSymbolInScope(Image, Skip, SymbolName, &FoundImage);
    if (ElfSymbol == NULL) {
        return STATUS_NOT_FOUND;
    }

    ASSERT((ElfSymbol->SectionIndex != ELF_SECTION_UNDEFINED) &&
           ((ElfSymbol->SectionIndex < ELF_SECTION_RESERVED_LOW) ||
            (ElfSymbol->SectionIndex == ELF_SECTION_ABSOLUTE)));

    //
    // TLS symbols are relative to their section base and are not adjusted.
    //

    Value = ElfSymbol->Value;
    Symbol->TlsAddress = FALSE;
    SymbolType = ELF_GET_SYMBOL_TYPE(ElfSymbol->Information);
    if (SymbolType == ElfSymbolTls) {
        Symbol->TlsAddress = TRUE;

    } else if (ElfSymbol->SectionIndex != ELF_SECTION_ABSOLUTE) {
        Value += FoundImage->BaseDifference;
    }

    Symbol->Address = (PVOID)(UINTN)Value;
    Symbol->Name = FoundImage->ExportStringTable + ElfSymbol->NameOffset;
    Symbol->Image = FoundImage;
    return STATUS_SUCCESS;
}

KSTATUS
ImpElfGetSymbolByAddress (
    PLOADED_IMAGE Image,
    PVOID Address,
    PIMAGE_SYMBOL Symbol
    )

/*++

Routine Description:

    This routine attempts to find the given address in the given image and
    resolve it to a symbol.

Arguments:

    Image - Supplies a pointer to the image to query.

    Address - Supplies the address to search for.

    Symbol - Supplies a pointer to a structure that receives the address's
        symbol information on success.

Return Value:

    Status code.

--*/

{

    ELF_ADDR BaseDifference;
    ELF_WORD BucketCount;
    ELF_WORD BucketIndex;
    PELF_SYMBOL ElfSymbol;
    ELF_WORD FilterWords;
    PELF_WORD HashBuckets;
    PELF_WORD HashChains;
    PELF_WORD HashTable;
    PVOID LoadedLowestAddress;
    ELF_ADDR SymbolAddress;
    ELF_WORD SymbolBase;
    ULONG SymbolHash;
    ELF_WORD SymbolIndex;
    PSTR SymbolName;
    ELF_ADDR Value;

    ASSERT(Image->Format == ImageElfNative);

    //
    // If the address is not within the bounds of the image, then it should not
    // be in the symbols.
    //

    BaseDifference = Image->BaseDifference;
    LoadedLowestAddress = Image->PreferredLowestAddress + BaseDifference;
    if ((Address < LoadedLowestAddress) ||
        (Address >= (LoadedLowestAddress + Image->Size))) {

        return STATUS_NOT_FOUND;
    }

    SymbolName = NULL;
    SymbolAddress = 0;

    //
    // If there is no export symbol table for this image, then only report the
    // image binary name and address without symbol information.
    //

    if (Image->ExportSymbolTable == NULL) {
        goto GetSymbolByAddressEnd;
    }

    //
    // Search for the symbol by its value, which is an address relative to the
    // preferred lowest address.
    //

    Value = (ELF_ADDR)(UINTN)Address - BaseDifference;

    //
    // Handle GNU-style hashing to iterate over the symbols.
    //

    if ((Image->Flags & IMAGE_FLAG_GNU_HASH) != 0) {
        HashTable = Image->ExportHashTable;
        BucketCount = *HashTable;
        HashTable += 1;
        SymbolBase = *HashTable;
        HashTable += 1;
        FilterWords = *HashTable;
        HashTable += 2;
        HashTable = (PELF_WORD)(HashTable + FilterWords);
        for (BucketIndex = 0; BucketIndex < BucketCount; BucketIndex += 1) {
            SymbolIndex = HashTable[BucketIndex];
            if (SymbolIndex == 0) {
                break;
            }

            if (SymbolIndex < SymbolBase) {

                ASSERT(FALSE);

                break;
            }

            HashChains = HashTable + BucketCount;
            do {
                SymbolHash = HashChains[SymbolIndex - SymbolBase];
                ElfSymbol = (PELF_SYMBOL)Image->ExportSymbolTable + SymbolIndex;
                if (((ElfSymbol->Size == 0) && (ElfSymbol->Value == Value)) ||
                    ((Value >= ElfSymbol->Value) &&
                     (Value < (ElfSymbol->Value + ElfSymbol->Size)))) {

                    SymbolName = Image->ExportStringTable +
                                 ElfSymbol->NameOffset;

                    SymbolAddress = ElfSymbol->Value + BaseDifference;
                    goto GetSymbolByAddressEnd;
                }

                SymbolIndex += 1;

            } while ((SymbolHash & 0x1) == 0);
        }

    //
    // Handle SVR hashing's mode of storing symbols.
    //

    } else {
        BucketCount = *((PELF_WORD)Image->ExportHashTable);
        HashBuckets = (PELF_WORD)Image->ExportHashTable + 2;
        HashChains = (PELF_WORD)Image->ExportHashTable + 2 + BucketCount;
        for (BucketIndex = 0; BucketIndex < BucketCount; BucketIndex += 1) {
            SymbolIndex = *(HashBuckets + BucketIndex);
            while (SymbolIndex != 0) {
                ElfSymbol = (PELF_SYMBOL)Image->ExportSymbolTable + SymbolIndex;
                if (((ElfSymbol->Size == 0) && (ElfSymbol->Value == Value)) ||
                    ((Value >= ElfSymbol->Value) &&
                     (Value < (ElfSymbol->Value + ElfSymbol->Size)))) {

                    SymbolName = Image->ExportStringTable +
                                 ElfSymbol->NameOffset;

                    SymbolAddress = ElfSymbol->Value + BaseDifference;
                    goto GetSymbolByAddressEnd;
                }

                //
                // Get the next entry in the chain.
                //

                SymbolIndex = *(HashChains + SymbolIndex);
            }
        }
    }

GetSymbolByAddressEnd:
    Symbol->Image = Image;
    Symbol->Name = SymbolName;
    Symbol->Address = (PVOID)(UINTN)SymbolAddress;
    Symbol->TlsAddress = FALSE;
    return STATUS_SUCCESS;
}

PVOID
ImpElfResolvePltEntry (
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

    ASSERT(Image->Format == ImageElfNative);

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
    Result = ImpElfApplyRelocation(Image,
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

    PELF_DYNAMIC_ENTRY DynamicEntry;
    PLOADED_IMAGE Import;
    ULONG ImportCount;
    ULONG ImportIndex;
    PSTR ImportName;
    ULONG LoadFlags;
    PELF_LOADING_IMAGE LoadingImage;
    KSTATUS Status;
    ELF_OFF StringTableOffset;

    ImportIndex = 0;
    LoadingImage = Image->ImageContext;

    ASSERT(LoadingImage != NULL);

    DynamicEntry = Image->DynamicSection;
    Status = STATUS_SUCCESS;
    if (DynamicEntry == NULL) {
        goto LoadImportsForImageEnd;
    }

    //
    // Loop over all dynamic entries once to count the number of import
    // libraries needed.
    //

    ImportCount = 0;
    while (DynamicEntry->Tag != ELF_DYNAMIC_NULL) {

        //
        // A "needed" entry indicates a required import library.
        //

        if (DynamicEntry->Tag == ELF_DYNAMIC_NEEDED) {
            ImportCount += 1;
        }

        DynamicEntry += 1;
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
    DynamicEntry = Image->DynamicSection;
    while (DynamicEntry->Tag != ELF_DYNAMIC_NULL) {

        //
        // A "needed" entry indicates a required import library.
        //

        if (DynamicEntry->Tag == ELF_DYNAMIC_NEEDED) {
            StringTableOffset = DynamicEntry->Value;

            ASSERT((Image->ExportStringTable != NULL) &&
                   (StringTableOffset < Image->ExportStringTableSize));

            ImportName = (PSTR)Image->ExportStringTable + StringTableOffset;

            //
            // Load the import. Dynamic libraries shouldn't have interpreters
            // specified.
            //

            LoadFlags = Image->LoadFlags | IMAGE_LOAD_FLAG_IGNORE_INTERPRETER;
            LoadFlags &= ~IMAGE_LOAD_FLAG_PRIMARY_EXECUTABLE;
            Status = ImpLoad(ListHead,
                             ImportName,
                             NULL,
                             NULL,
                             Image->SystemContext,
                             LoadFlags,
                             Image,
                             &Import,
                             NULL);

            if (!KSUCCESS(Status)) {
                RtlDebugPrint("%s: Failed to find import '%s': %d\n",
                              Image->FileName,
                              ImportName,
                              Status);

                goto LoadImportsForImageEnd;
            }

            Image->Imports[ImportIndex] = Import;
            ImportIndex += 1;
        }

        DynamicEntry += 1;
    }

LoadImportsForImageEnd:
    if (!KSUCCESS(Status)) {
        if (Image->Imports != NULL) {
            if (ImportIndex != 0) {
                ImportCount = ImportIndex - 1;
                for (ImportIndex = 0;
                     ImportIndex < ImportCount;
                     ImportIndex += 1) {

                    ImImageReleaseReference(Image->Imports[ImportIndex]);
                }
            }

            ImFreeMemory(Image->Imports);
            Image->Imports = NULL;
            Image->ImportCount = 0;
        }
    }

    return Status;
}

KSTATUS
ImpElfGatherExportInformation (
    PLOADED_IMAGE Image,
    PIM_RESOLVE_PLT_ENTRY PltResolver,
    BOOL UseLoadedAddress
    )

/*++

Routine Description:

    This routine gathers necessary pointers from the non-loaded portion of the
    image file needed to retrieve exports from the image.

Arguments:

    Image - Supplies a pointer to the loaded image.

    PltResolver - Supplies a pointer to the function used to resolve PLT
        entries.

    UseLoadedAddress - Supplies a boolean indicating whether to use the file
        offset (FALSE) or the final virtual address (TRUE).

Return Value:

    Status code.

--*/

{

    PVOID Address;
    ELF_ADDR BaseDifference;
    PELF_DYNAMIC_ENTRY DynamicEntry;
    PVOID DynamicSymbols;
    PVOID DynamicSymbolStrings;
    ELF_XWORD DynamicSymbolStringsSize;
    PELF_ADDR Got;
    PELF_WORD HashTable;
    ELF_XWORD HashTag;
    ELF_HALF HeaderCount;
    ELF_HALF Index;
    ELF_XWORD LibraryNameOffset;
    PELF_LOADING_IMAGE LoadingImage;
    PVOID PltRelocations;
    BOOL PltRelocationsAddends;
    PELF_PROGRAM_HEADER ProgramHeader;
    PIMAGE_STATIC_FUNCTIONS StaticFunctions;
    KSTATUS Status;
    ELF_SXWORD Tag;
    ELF_XWORD Value;

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

    BaseDifference = Image->BaseDifference;

    //
    // If the loaded image buffer address is the loaded address, then go ahead
    // and indicate things are live.
    //

    if (Image->PreferredLowestAddress + BaseDifference ==
        Image->LoadedImageBuffer) {

        UseLoadedAddress = TRUE;
    }

    if (UseLoadedAddress != FALSE) {
        BaseDifference = Image->BaseDifference;
        DynamicEntry = (PVOID)(UINTN)(ProgramHeader->VirtualAddress +
                                      BaseDifference);

    } else {
        BaseDifference = Image->LoadedImageBuffer -
                         Image->PreferredLowestAddress;

        DynamicEntry = LoadingImage->Buffer.Data + ProgramHeader->Offset;
    }

    //
    // Save the pointer to the dynamic section header and dynamic symbol count.
    // This is used by the load import routine.
    //

    Image->DynamicSection = DynamicEntry;
    while (DynamicEntry->Tag != ELF_DYNAMIC_NULL) {
        Tag = DynamicEntry->Tag;
        Value = DynamicEntry->Value;
        Address = (PVOID)(UINTN)(Value + BaseDifference);
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

                if ((Image->TlsImageSize != 0) &&
                    ((Image->LoadFlags & IMAGE_LOAD_FLAG_PRIMARY_LOAD) == 0)) {

                    Status = STATUS_TOO_LATE;
                    goto GatherExportInformationEnd;
                }
            }

            break;

        //
        // Upon finding the GOT, save the image and the resolution address in
        // the second and third entries of the GOT. Note that reaching through
        // a global would be bad if trying to relocate oneself, but that
        // should only ever be done by the dynamic linker, which won't have
        // imports.
        //

        case ELF_DYNAMIC_PLT_GOT:
            Got = Address;
            Got[1] = (UINTN)Image;
            Got[2] = (UINTN)PltResolver;
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

        //
        // Stick a pointer to the debug structure into the debug dynamic entry.
        // Don't do this if loading from some alternate address space.
        //

        case ELF_DYNAMIC_DEBUG:
            if (UseLoadedAddress != FALSE) {
                DynamicEntry->Value = (UINTN)&(Image->Debug);
            }

            break;

        case ELF_DYNAMIC_BIND_NOW:
            Image->LoadFlags |= IMAGE_LOAD_FLAG_BIND_NOW;
            break;

        default:
            break;
        }

        DynamicEntry += 1;
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

    //
    // Set the library name if there is one.
    //

    if (LibraryNameOffset != 0) {
        Image->LibraryName = DynamicSymbolStrings + LibraryNameOffset;
    }

    Status = STATUS_SUCCESS;

GatherExportInformationEnd:
    return Status;
}

PELF_DYNAMIC_ENTRY
ImpElfGetDynamicEntry (
    PLOADED_IMAGE Image,
    ELF_SXWORD Tag
    )

/*++

Routine Description:

    This routine attempts to find a dynamic entry with the given tag.

Arguments:

    Image - Supplies a pointer to the image.

    Tag - Supplies the desired tag.

Return Value:

    Returns a pointer to the requested entry on success.

    NULL if the entry could not be found or there is no dynamic section.

--*/

{

    PELF_DYNAMIC_ENTRY Entry;

    Entry = Image->DynamicSection;
    if (Entry != NULL) {
        while (Entry->Tag != ELF_DYNAMIC_NULL) {
            if (Entry->Tag == Tag) {
                return Entry;
            }

            Entry += 1;
        }
    }

    return NULL;
}

KSTATUS
ImpElfRelocateImage (
    PLOADED_IMAGE Image
    )

/*++

Routine Description:

    This routine relocates a loaded image.

Arguments:

    Image - Supplies a pointer to the loaded image.

Return Value:

    Status code.

--*/

{

    PVOID Address;
    ELF_ADDR BaseDifference;
    PELF_DYNAMIC_ENTRY DynamicEntry;
    PELF_LOADING_IMAGE LoadingImage;
    BOOL PltRelocationAddends;
    PVOID PltRelocations;
    ELF_XWORD PltRelocationsSize;
    PVOID Relocations;
    PVOID RelocationsAddends;
    ELF_XWORD RelocationsAddendsSize;
    ELF_XWORD RelocationsSize;
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

    DynamicEntry = Image->DynamicSection;
    if (DynamicEntry == NULL) {
        Status = STATUS_SUCCESS;
        goto RelocateImageEnd;
    }

    while (DynamicEntry->Tag != ELF_DYNAMIC_NULL) {
        Address = (PVOID)(UINTN)(DynamicEntry->Value + BaseDifference);
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
        Status = ImpElfProcessRelocateSection(Image,
                                              Relocations,
                                              RelocationsSize,
                                              FALSE);

        if (!KSUCCESS(Status)) {
            goto RelocateImageEnd;
        }
    }

    if ((RelocationsAddends != NULL) && (RelocationsAddendsSize != 0)) {
        Status = ImpElfProcessRelocateSection(Image,
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
            Status = ImpElfProcessRelocateSection(Image,
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
    PLOADED_IMAGE Image,
    PVOID Relocations,
    ELF_XWORD RelocationsSize,
    BOOL Addends
    )

/*++

Routine Description:

    This routine processes the contents of a single relocation section.

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

    PELF_RELOCATION_ENTRY Relocation;
    PELF_RELOCATION_ADDEND_ENTRY RelocationAddend;
    ELF_XWORD RelocationCount;
    UINTN RelocationIndex;
    BOOL Result;

    RelocationAddend = Relocations;
    Relocation = Relocations;
    if (Addends != FALSE) {
        RelocationCount = RelocationsSize / sizeof(ELF_RELOCATION_ADDEND_ENTRY);

    } else {
        RelocationCount = RelocationsSize / sizeof(ELF_RELOCATION_ENTRY);
    }

    //
    // Process each relocation in the table.
    //

    for (RelocationIndex = 0;
         RelocationIndex < RelocationCount;
         RelocationIndex += 1) {

        if (Addends != FALSE) {
            Result = ImpElfApplyRelocation(Image,
                                           RelocationAddend,
                                           TRUE,
                                           NULL);

            ASSERT(Result != FALSE);

            if (Result == FALSE) {
                return STATUS_INVALID_PARAMETER;
            }

            RelocationAddend += 1;

        } else {
            Result = ImpElfApplyRelocation(Image,
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
    ELF_XWORD RelocationsSize,
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

    ELF_ADDR BaseDifference;
    ELF_XWORD Information;
    ELF_ADDR Offset;
    PELF_RELOCATION_ENTRY Relocation;
    PELF_RELOCATION_ADDEND_ENTRY RelocationAddend;
    ELF_XWORD RelocationCount;
    UINTN RelocationIndex;
    PELF_ADDR RelocationPlace;
    ELF_WORD RelocationType;

    BaseDifference = Image->BaseDifference;
    if (BaseDifference == 0) {
        return;
    }

    RelocationAddend = Relocations;
    Relocation = Relocations;
    if (Addends != FALSE) {
        RelocationCount = RelocationsSize /
                          sizeof(ELF_RELOCATION_ADDEND_ENTRY);

    } else {
        RelocationCount = RelocationsSize / sizeof(ELF_RELOCATION_ENTRY);
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

        RelocationType = ELF_GET_RELOCATION_TYPE(Information);

        //
        // If this is a jump slot relocation, bump it up by the base difference.
        //

        if (((Image->Machine == ImageMachineTypeArm32) &&
             (RelocationType == ElfArmRelocationJumpSlot)) ||
            ((Image->Machine == ImageMachineTypeX86) &&
             (RelocationType == Elf386RelocationJumpSlot)) ||
            ((Image->Machine == ImageMachineTypeX64) &&
             (RelocationType == ElfX64RelocationJumpSlot))) {

            RelocationPlace = (PELF_ADDR)((PUCHAR)Image->LoadedImageBuffer +
                               (Offset - (UINTN)Image->PreferredLowestAddress));

            *RelocationPlace += BaseDifference;
        }
    }

    return;
}

ELF_ADDR
ImpElfGetSymbolValue (
    PLOADED_IMAGE Image,
    PELF_SYMBOL Symbol,
    PLOADED_IMAGE *FoundImage,
    PLOADED_IMAGE SkipImage
    )

/*++

Routine Description:

    This routine determines the address of the given symbol.

Arguments:

    Image - Supplies a pointer to the loaded image.

    Symbol - Supplies a pointer to the symbol whose value should be computed.

    FoundImage - Supplies a pointer where the image where the symbol is defined
        will be returned on success.

    SkipImage - Supplies an optional pointer to an image to ignore when
        searching for a symbol definition. This is used in copy relocations
        when looking for the shared object version of a symbol defined in
        both the executable and a shared object.

Return Value:

    Returns the symbol's value on success.

    ELF_INVALID_ADDRESS on failure.

--*/

{

    ELF_SYMBOL_BIND_TYPE BindType;
    ULONG Hash;
    PELF_SYMBOL Potential;
    PSTR SymbolName;
    ELF_ADDR Value;

    *FoundImage = NULL;
    BindType = ELF_GET_SYMBOL_BIND(Symbol->Information);
    if (Symbol->NameOffset != 0) {
        SymbolName = Image->ExportStringTable + Symbol->NameOffset;
        if (BindType == ElfBindLocal) {

            ASSERT(SkipImage == NULL);

            if ((Image->Flags & IMAGE_FLAG_GNU_HASH) != 0) {
                Hash = ImpElfGnuHash(SymbolName);

            } else {
                Hash = ImpElfOriginalHash(SymbolName);
            }

            Potential = ImpElfGetSymbol(Image, Hash, SymbolName);
            if (Potential != NULL) {
                if ((Potential->SectionIndex == 0) ||
                    ((Potential->SectionIndex >= ELF_SECTION_RESERVED_LOW) &&
                     (Potential->SectionIndex != ELF_SECTION_ABSOLUTE))) {

                    Potential = NULL;

                } else {
                    *FoundImage = Image;
                }
            }

        } else {
            Potential = ImpElfGetSymbolInScope(Image,
                                               SkipImage,
                                               SymbolName,
                                               FoundImage);
        }

        if (Potential != NULL) {

            ASSERT((Potential->SectionIndex != 0) &&
                   ((Potential->SectionIndex < ELF_SECTION_RESERVED_LOW) ||
                    (Potential->SectionIndex == ELF_SECTION_ABSOLUTE)));

            //
            // TLS symbols are relative to their section base, and are
            // not adjusted.
            //

            Value = Potential->Value;
            if ((ELF_GET_SYMBOL_TYPE(Symbol->Information) != ElfSymbolTls) &&
                (Potential->SectionIndex != ELF_SECTION_ABSOLUTE)) {

                Value += (*FoundImage)->BaseDifference;
            }

            goto ElfGetSymbolValueEnd;
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

            RtlDebugPrint("Warning: Unresolved reference to symbol %s from "
                          "%s.\n",
                          SymbolName,
                          Image->FileName);

            Value = ELF_INVALID_ADDRESS;

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

        Value = ELF_INVALID_ADDRESS;
        if (Symbol->SectionIndex == ELF_SECTION_ABSOLUTE) {
            Value = Symbol->Value;
        }

        goto ElfGetSymbolValueEnd;
    }

    Value = Symbol->Value + Image->BaseDifference;

ElfGetSymbolValueEnd:
    return Value;
}

PELF_SYMBOL
ImpElfGetSymbolInScope (
    PLOADED_IMAGE ScopeImage,
    PLOADED_IMAGE Skip,
    PCSTR SymbolName,
    PLOADED_IMAGE *FoundImage
    )

/*++

Routine Description:

    This routine attempts to find an exported symbol with the given name in the
    scope for the given binary.

Arguments:

    ScopeImage - Supplies a pointer to the image whose scope should be searched.

    Skip - Supplies an optional pointer to an image to skip. This is used to
        skip the primary executable when finding symbol values for copy
        relocations, and for skipping the image when the "next" symbol is
        desired.

    Hash - Supplies the hashed symbol name.

    SymbolName - Supplies a pointer to the string containing the name of the
        symbol to search for.

    FoundImage - Supplies a pointer where a pointer to the image the symbol
        was found in will be returned on success.

Return Value:

    Returns a pointer to the symbol on success.

    NULL if no such symbol was found.

--*/

{

    ULONG CurrentFlags;
    ULONG Hash;
    PLOADED_IMAGE Image;
    UINTN Index;
    PELF_SYMBOL Result;
    PLOADED_IMAGE *Scope;
    UINTN ScopeSize;

    //
    // Check the global scope first. The conditional limits recursion to once.
    //

    if ((ScopeImage != ImPrimaryExecutable) &&
        (ImPrimaryExecutable != NULL)) {

        Result = ImpElfGetSymbolInScope(ImPrimaryExecutable,
                                        Skip,
                                        SymbolName,
                                        FoundImage);

        if (Result != NULL) {
            return Result;
        }
    }

    //
    // Take a guess at what the hashing style is going to be based on the
    // hashing style of the scope owner.
    //

    CurrentFlags = ScopeImage->Flags;
    if ((CurrentFlags & IMAGE_FLAG_GNU_HASH) != 0) {
        Hash = ImpElfGnuHash(SymbolName);

    } else {
        Hash = ImpElfOriginalHash(SymbolName);
    }

    Scope = ScopeImage->Scope;
    ScopeSize = ScopeImage->ScopeSize;
    for (Index = 0; Index < ScopeSize; Index += 1) {
        Image = Scope[Index];
        if (Image == Skip) {
            continue;
        }

        //
        // Potentially recompute the hash value if the style doesn't match
        // the guess.
        //

        if (((CurrentFlags ^ Image->Flags) & IMAGE_FLAG_GNU_HASH) != 0) {
            CurrentFlags = Image->Flags;
            if ((CurrentFlags & IMAGE_FLAG_GNU_HASH) != 0) {
                Hash = ImpElfGnuHash(SymbolName);

            } else {
                Hash = ImpElfOriginalHash(SymbolName);
            }
        }

        Result = ImpElfGetSymbol(Image, Hash, SymbolName);

        //
        // Symbols from section 0 are undefined, so don't return those.
        // Also ignore symbols above the reserved value, except for absolute
        // symbols.
        //

        if ((Result != NULL) && (Result->SectionIndex != 0) &&
            ((Result->SectionIndex < ELF_SECTION_RESERVED_LOW) ||
             (Result->SectionIndex == ELF_SECTION_ABSOLUTE))) {

            *FoundImage = Image;
            return Result;
        }
    }

    return NULL;
}

PELF_SYMBOL
ImpElfGetSymbol (
    PLOADED_IMAGE Image,
    ULONG Hash,
    PCSTR SymbolName
    )

/*++

Routine Description:

    This routine attempts to find an exported symbol with the given name in the
    given binary.

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

    ELF_WORD BucketCount;
    ELF_WORD BucketIndex;
    BOOL Equal;
    PELF_WORD Filter;
    ELF_WORD FilterMask;
    ELF_WORD FilterWord;
    ELF_WORD FilterWords;
    PELF_WORD HashBuckets;
    PELF_WORD HashChains;
    PELF_WORD HashTable;
    PELF_SYMBOL Potential;
    ULONG PotentialHash;
    PSTR PotentialName;
    ULONG RemainingSize;
    ELF_WORD Shift;
    ELF_WORD SymbolBase;
    ELF_WORD SymbolIndex;
    ELF_WORD WordIndex;

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
        //

        Filter = (PELF_WORD)HashTable;
        HashTable = (PELF_WORD)(Filter + FilterWords);
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
                Potential = (PELF_SYMBOL)Image->ExportSymbolTable +
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
        BucketCount = *((PELF_WORD)Image->ExportHashTable);
        HashBuckets = (PELF_WORD)Image->ExportHashTable + 2;
        HashChains = (PELF_WORD)Image->ExportHashTable + 2 + BucketCount;
        BucketIndex = Hash % BucketCount;
        SymbolIndex = *(HashBuckets + BucketIndex);
        while (SymbolIndex != 0) {
            Potential = (PELF_SYMBOL)Image->ExportSymbolTable + SymbolIndex;
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
    PLOADED_IMAGE Image,
    PELF_RELOCATION_ADDEND_ENTRY RelocationEntry,
    BOOL AddendEntry,
    PVOID *FinalSymbolValue
    )

/*++

Routine Description:

    This routine applies a relocation entry to a loaded image.

Arguments:

    Image - Supplies a pointer to the loaded image structure.

    RelocationEntry - Supplies a pointer to the relocation entry. This should
        either be of type PELF_RELOCATION_ENTRY or
        PELF_RELOCATION_ADDEND_ENTRY depending on the Addends parameter.

    AddendEntry - Supplies a flag indicating that the entry if of type
        ELF_RELOCATION_ADDEND_ENTRY, not ELF_RELOCATION_ENTRY.

    FinalSymbolValue - Supplies an optional pointer where the symbol value will
        be returned on success. This is used by PLT relocations that are being
        fixed up on the fly and also need to jump directly to the symbol
        address.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    ELF_SXWORD Addend;
    BOOL AddendNeeded;
    ELF_ADDR Address;
    ELF_ADDR BaseDifference;
    BOOL Copy;
    ELF_XWORD Information;
    PELF_LOADING_IMAGE LoadingImage;
    ELF_ADDR Offset;
    ELF_ADDR Place;
    PVOID RelocationEnd;
    BOOL RelocationNeeded;
    PELF_ADDR RelocationPlace;
    ELF_XWORD RelocationType;
    PLOADED_IMAGE SymbolImage;
    ELF_XWORD SymbolIndex;
    PELF_SYMBOL Symbols;
    ELF_ADDR SymbolValue;

    Address = 0;
    LoadingImage = Image->ImageContext;
    BaseDifference = Image->BaseDifference;
    Offset = RelocationEntry->Offset;
    Information = RelocationEntry->Information;
    AddendNeeded = TRUE;
    Addend = 0;
    if (AddendEntry != FALSE) {
        Addend = RelocationEntry->Addend;
        AddendNeeded = FALSE;
    }

    //
    // The place is the actual VA of the relocation.
    //

    Place = BaseDifference + Offset;

    //
    // The Information field contains both the symbol index to the
    // relocation as well as the type of relocation to apply.
    //

    Symbols = Image->ExportSymbolTable;
    SymbolIndex = ELF_GET_RELOCATION_SYMBOL(Information);
    RelocationType = ELF_GET_RELOCATION_TYPE(Information);

    //
    // Compute the symbol value.
    //

    SymbolValue = ImpElfGetSymbolValue(Image,
                                       &(Symbols[SymbolIndex]),
                                       &SymbolImage,
                                       NULL);

    if (SymbolValue == ELF_INVALID_ADDRESS) {
        SymbolValue = 0;
    }

    if (FinalSymbolValue != NULL) {
        *FinalSymbolValue = (PVOID)(UINTN)SymbolValue;
    }

    //
    // Based on the type of relocation, compute the relocated value.
    //

    Copy = FALSE;
    RelocationNeeded = TRUE;
    if (Image->Machine == ImageMachineTypeArm32) {
        switch (RelocationType) {

        //
        // None is a no-op.
        //

        case ElfArmRelocationNone:
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

            ASSERT(Image->PreferredLowestAddress + BaseDifference ==
                   Image->LoadedImageBuffer);

            //
            // Find the shared object version, not the executable version.
            //

            SymbolValue = ImpElfGetSymbolValue(Image,
                                               &(Symbols[SymbolIndex]),
                                               &SymbolImage,
                                               Image);

            if (SymbolValue == ELF_INVALID_ADDRESS) {
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

        case Elf386RelocationNone:
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

            ASSERT(Image->PreferredLowestAddress + BaseDifference ==
                   Image->LoadedImageBuffer);

            //
            // Find the shared object version, not the executable version.
            //

            SymbolValue = ImpElfGetSymbolValue(Image,
                                               &(Symbols[SymbolIndex]),
                                               &SymbolImage,
                                               Image);

            if (SymbolValue == ELF_INVALID_ADDRESS) {
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

    //
    // Handle x64 images.
    //

    } else if (Image->Machine == ImageMachineTypeX64) {
        switch (RelocationType) {

        //
        // None is a no-op.
        //

        case ElfX64RelocationNone:
            RelocationNeeded = FALSE;
            break;

        //
        // Absolute uses only the symbol's value.
        //

        case ElfX64Relocation64:
            Address = SymbolValue + Addend;
            break;

        //
        // PC32 is Symbol + Addend - Place
        //

        case ElfX64RelocationPc32:

            //
            // TODO: Handle non-native sizes.
            //

            ASSERT(FALSE);

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

        case ElfX64RelocationCopy:

            ASSERT(Image->PreferredLowestAddress + BaseDifference ==
                   Image->LoadedImageBuffer);

            //
            // Find the shared object version, not the executable version.
            //

            SymbolValue = ImpElfGetSymbolValue(Image,
                                               &(Symbols[SymbolIndex]),
                                               &SymbolImage,
                                               Image);

            if (SymbolValue == ELF_INVALID_ADDRESS) {
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

        case ElfX64RelocationGlobalData:
            Address = SymbolValue;
            AddendNeeded = FALSE;
            break;

        case ElfX64RelocationJumpSlot:
            Address = SymbolValue;
            AddendNeeded = FALSE;
            break;

        //
        // Relative relocations just adjust for the new base.
        //

        case ElfX64RelocationRelative:
            Address = BaseDifference + Addend;
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
        RelocationPlace = (PELF_ADDR)((PUCHAR)Image->LoadedImageBuffer +
                           (Offset - (UINTN)Image->PreferredLowestAddress));

        if (AddendNeeded != FALSE) {
            Address += *RelocationPlace;
        }

        if (Copy != FALSE) {
            RtlCopyMemory(RelocationPlace,
                          (PVOID)(UINTN)Address,
                          Symbols[SymbolIndex].Size);

            RelocationEnd = (PVOID)RelocationPlace +
                            Symbols[SymbolIndex].Size;

        } else {

            //
            // Avoid the write unless it's necessary, as unnecessary write
            // faults are expensive.
            //

            if (*RelocationPlace != Address) {
                *RelocationPlace = Address;
                RelocationEnd = RelocationPlace + 1;

            } else {
                RelocationEnd = NULL;
            }
        }

        if ((LoadingImage != NULL) &&
            ((Image->Flags & IMAGE_FLAG_TEXT_RELOCATIONS) != 0) &&
            (RelocationEnd != NULL)) {

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

