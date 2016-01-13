/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    image.c

Abstract:

    This module implements image loading functionality, forking off most of the
    work to the individual file format functions.

Author:

    Evan Green 13-Oct-2012

Environment:

    Kernel/Boot/Build

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "imp.h"
#include "pe.h"
#include "elf.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro subtracts a value from a pointer.
//

#define POINTER_SUBTRACT(_Pointer, _Value) (PVOID)((UINTN)(_Pointer) - (_Value))

//
// This macro adds a value to a pointer.
//

#define POINTER_ADD(_Pointer, _Value) (PVOID)((UINTN)(_Pointer) + (_Value))

//
// ---------------------------------------------------------------- Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
ImpGetImageSize (
    PLIST_ENTRY ListHead,
    PLOADED_IMAGE Image,
    PVOID FileBuffer,
    UINTN FileBufferSize,
    PSTR *InterpreterPath
    );

KSTATUS
ImpLoadImage (
    PLIST_ENTRY ListHead,
    PLOADED_IMAGE Image,
    PVOID FileBuffer,
    UINTN FileBufferSize,
    ULONG ImportDepth
    );

KSTATUS
ImpAddImage (
    PLOADED_IMAGE Image
    );

VOID
ImpUnloadImage (
    PLOADED_IMAGE Image
    );

KSTATUS
ImpGetSymbolAddress (
    PLIST_ENTRY ListHead,
    PLOADED_IMAGE Image,
    PSTR SymbolName,
    ULONG RecursionLevel,
    UCHAR VisitMarker,
    PVOID *Address
    );

PLOADED_IMAGE
ImpFindImageInList (
    PLIST_ENTRY ListHead,
    PSTR ImageName
    );

PLOADED_IMAGE
ImpAllocateImage (
    PSTR ImageName,
    UINTN ImageNameSize
    );

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// Define the table of functions called by the image library.
//

PIM_IMPORT_TABLE ImImportTable = NULL;

//
// Store the last visit marker.
//

UCHAR ImLastVisitMarker;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
ImInitialize (
    PIM_IMPORT_TABLE ImportTable
    )

/*++

Routine Description:

    This routine initializes the image library. It must be called before any
    other image library routines are called.

Arguments:

    ImportTable - Supplies a pointer to a table of functions that will be used
        by the image library to provide basic memory allocation and loading
        support. This memory must stick around, the given pointer is cached.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_TOO_LATE if the image library has already been initialized.

    STATUS_INVALID_PARAMETER if one of the required functions is not
        implemented.

--*/

{

    if (ImImportTable != NULL) {
        return STATUS_TOO_LATE;
    }

    ImImportTable = ImportTable;
    return STATUS_SUCCESS;
}

KSTATUS
ImGetExecutableFormat (
    PSTR BinaryName,
    PVOID SystemContext,
    PIMAGE_FILE_INFORMATION Information,
    PIMAGE_FORMAT Format
    )

/*++

Routine Description:

    This routine determines the executable format of a given image path.

Arguments:

    BinaryName - Supplies the name of the binary executable image to examine.

    SystemContext - Supplies the context pointer passed to the load executable
        function.

    Information - Supplies an optional pointer where the file handle and other
        information will be returned on success.

    Format - Supplies a pointer where the format will be returned on success.

Return Value:

    Status code.

--*/

{

    ULONG BinaryNameLength;
    IMAGE_FILE_INFORMATION File;
    PVOID FileBuffer;
    KSTATUS Status;

    File.Handle = INVALID_HANDLE;
    FileBuffer = NULL;
    BinaryNameLength = RtlStringLength(BinaryName);
    if (BinaryNameLength == 0) {
        Status = STATUS_INVALID_PARAMETER;
        goto GetExecutableFormatEnd;
    }

    //
    // Load the file contents into memory.
    //

    Status = ImOpenFile(SystemContext, BinaryName, &File);
    if (!KSUCCESS(Status)) {
        goto GetExecutableFormatEnd;
    }

    Status = ImLoadFile(&File, &FileBuffer);
    if (!KSUCCESS(Status)) {
        goto GetExecutableFormatEnd;
    }

    //
    // Determine the file format.
    //

    *Format = ImGetImageFormat(FileBuffer, File.Size);
    if ((*Format == ImageInvalidFormat) || (*Format == ImageUnknownFormat)) {
        Status = STATUS_UNKNOWN_IMAGE_FORMAT;
        goto GetExecutableFormatEnd;
    }

    Status = STATUS_SUCCESS;

GetExecutableFormatEnd:
    if (FileBuffer != NULL) {
        ImUnloadFile(&File, FileBuffer);
    }

    if (((!KSUCCESS(Status)) || (Information == NULL)) &&
        (File.Handle != INVALID_HANDLE)) {

        ImCloseFile(&File);
        File.Handle = INVALID_HANDLE;
    }

    if (Information != NULL) {
        RtlCopyMemory(Information, &File, sizeof(IMAGE_FILE_INFORMATION));
    }

    return Status;
}

KSTATUS
ImLoadExecutable (
    PLIST_ENTRY ListHead,
    PSTR BinaryName,
    PIMAGE_FILE_INFORMATION BinaryFile,
    PVOID SystemContext,
    ULONG Flags,
    ULONG ImportDepth,
    PLOADED_IMAGE *LoadedImage,
    PLOADED_IMAGE *Interpreter
    )

/*++

Routine Description:

    This routine loads an executable image into memory.

Arguments:

    ListHead - Supplies a pointer to the head of the list of loaded images.

    BinaryName - Supplies the name of the binary executable image to load. If
        this is NULL, then a pointer to the first (primary) image loaded, with
        a reference added.

    BinaryFile - Supplies an optional handle to the file information. The
        handle should be positioned to the beginning of the file. Supply NULL
        if the caller does not already have an open handle to the binary. On
        success, the image library takes ownership of the handle.

    SystemContext - Supplies an opaque token that will be passed to the
        support functions called by the image support library.

    Flags - Supplies a bitfield of flags governing the load. See
        IMAGE_LOAD_FLAG_* flags.

    ImportDepth - Supplies the import depth of the image. Supply 0 here.

    LoadedImage - Supplies an optional pointer where a pointer to the loaded
        image structure will be returned on success.

    Interpreter - Supplies an optional pointer where a pointer to the loaded
        interpreter structure will be returned on success.

Return Value:

    Status code.

--*/

{

    ULONG BinaryNameLength;
    PLIST_ENTRY CurrentEntry;
    PVOID FileBuffer;
    ULONG FileNameSize;
    PLOADED_IMAGE Image;
    PLOADED_IMAGE InterpreterImage;
    PSTR InterpreterPath;
    ULONG NameIndex;
    KSTATUS Status;

    FileBuffer = NULL;
    Image = NULL;
    InterpreterImage = NULL;

    //
    // If the primary executable flag is set, also set the primary load flag.
    // The difference is that the primary executable flag is set only on the
    // executable itself, whereas the primary load flag is set on the primary
    // executable and any dynamic libraries loaded to satisfy dependencies
    // during this process.
    //

    if ((Flags & IMAGE_LOAD_FLAG_PRIMARY_EXECUTABLE) != 0) {
        Flags |= IMAGE_LOAD_FLAG_PRIMARY_LOAD;
    }

    //
    // If the name is NULL, return the primary executable.
    //

    if (BinaryName == NULL) {

        ASSERT((Flags & IMAGE_LOAD_FLAG_PRIMARY_EXECUTABLE) == 0);

        CurrentEntry = ListHead->Next;
        while (CurrentEntry != ListHead) {
            Image = LIST_VALUE(CurrentEntry, LOADED_IMAGE, ListEntry);
            if ((Image->LoadFlags & IMAGE_LOAD_FLAG_PRIMARY_EXECUTABLE) != 0) {
                ImImageAddReference(Image);
                Status = STATUS_SUCCESS;
                goto LoadExecutableEnd;
            }

            CurrentEntry = CurrentEntry->Next;
        }

        Status = STATUS_NOT_READY;
        goto LoadExecutableEnd;
    }

    BinaryNameLength = RtlStringLength(BinaryName);
    if (BinaryNameLength == 0) {
        Status = STATUS_INVALID_PARAMETER;
        goto LoadExecutableEnd;
    }

    //
    // Chop off everything but the name.
    //

    NameIndex = BinaryNameLength - 1;
    while ((NameIndex != 0) && (BinaryName[NameIndex] != '/')) {
        NameIndex -= 1;
    }

    if (BinaryName[NameIndex] == '/') {
        NameIndex += 1;
    }

    if (NameIndex >= BinaryNameLength) {
        Status = STATUS_INVALID_PARAMETER;
        goto LoadExecutableEnd;
    }

    FileNameSize = BinaryNameLength - NameIndex;

    //
    // See if the image is already loaded, and return if so.
    //

    Image = ImpFindImageInList(ListHead, BinaryName + NameIndex);
    if (Image != NULL) {
        ImImageAddReference(Image);
        Status = STATUS_SUCCESS;
        goto LoadExecutableEnd;
    }

    //
    // Allocate space for the loaded image structure.
    //

    Image = ImpAllocateImage(BinaryName + NameIndex, FileNameSize + 1);
    if (Image == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto LoadExecutableEnd;
    }

    Image->SystemContext = SystemContext;
    Image->ImportDepth = ImportDepth;
    Image->LoadFlags = Flags;

    //
    // Load the file contents into memory.
    //

    if (BinaryFile != NULL) {
        RtlCopyMemory(&(Image->File),
                      BinaryFile,
                      sizeof(IMAGE_FILE_INFORMATION));

    } else {
        Status = ImOpenFile(SystemContext, BinaryName, &(Image->File));
        if (!KSUCCESS(Status)) {
            goto LoadExecutableEnd;
        }
    }

    Status = ImLoadFile(&(Image->File), &FileBuffer);
    if (!KSUCCESS(Status)) {
        goto LoadExecutableEnd;
    }

    //
    // Determine the file format.
    //

    Image->Format = ImGetImageFormat(FileBuffer, Image->File.Size);
    if ((Image->Format == ImageInvalidFormat) ||
        (Image->Format == ImageUnknownFormat)) {

        Status = STATUS_UNKNOWN_IMAGE_FORMAT;
        goto LoadExecutableEnd;
    }

    //
    // Determine the image size and preferred VA.
    //

    Status = ImpGetImageSize(ListHead,
                             Image,
                             FileBuffer,
                             Image->File.Size,
                             &InterpreterPath);

    if (!KSUCCESS(Status)) {
        goto LoadExecutableEnd;
    }

    //
    // Load the interpreter if there is one.
    //

    if (InterpreterPath != NULL) {

        ASSERT((Flags & IMAGE_LOAD_FLAG_IGNORE_INTERPRETER) == 0);

        Status = ImLoadExecutable(ListHead,
                                  InterpreterPath,
                                  NULL,
                                  SystemContext,
                                  Flags | IMAGE_LOAD_FLAG_IGNORE_INTERPRETER,
                                  ImportDepth,
                                  &InterpreterImage,
                                  NULL);

        if (!KSUCCESS(Status)) {
            goto LoadExecutableEnd;
        }
    }

    if (ImAllocateAddressSpace != NULL) {

        //
        // Call out to the allocator to get space for the image.
        //

        Image->LoadedLowestAddress = Image->PreferredLowestAddress;
        Status = ImAllocateAddressSpace(Image->SystemContext,
                                        &(Image->File),
                                        Image->Size,
                                        &(Image->AllocatorHandle),
                                        &(Image->LoadedLowestAddress),
                                        &(Image->LoadedImageBuffer));

        if (!KSUCCESS(Status)) {
            goto LoadExecutableEnd;
        }

        //
        // If the image is not relocatable and the preferred address could not
        // be allocated, then this image cannot be loaded.
        //

        if ((Image->LoadedLowestAddress != Image->PreferredLowestAddress) &&
            ((Image->Flags & IMAGE_FLAG_RELOCATABLE) == 0)) {

            Status = STATUS_MEMORY_CONFLICT;
            goto LoadExecutableEnd;
        }

    //
    // Just pretend for now it got put at the right spot. This will be adjusted
    // later.
    //

    } else {
        Image->LoadedLowestAddress = Image->PreferredLowestAddress;
    }

    //
    // Call the image-specific routine to actually load/map the image into its
    // allocated space.
    //

    Status = ImpLoadImage(ListHead,
                          Image,
                          FileBuffer,
                          Image->File.Size,
                          ImportDepth);

    if (!KSUCCESS(Status)) {
        goto LoadExecutableEnd;
    }

LoadExecutableEnd:

    //
    // Tear down the portion of the image loaded so far on failure.
    //

    if (!KSUCCESS(Status)) {
        if (InterpreterImage != NULL) {
            ImImageReleaseReference(InterpreterImage);
        }

        if (Image != NULL) {
            if (Image->AllocatorHandle != INVALID_HANDLE) {
                ImFreeAddressSpace(Image->AllocatorHandle,
                                   Image->LoadedLowestAddress,
                                   Image->Size);
            }

            if (Image->File.Handle != INVALID_HANDLE) {
                if (FileBuffer != NULL) {
                    ImUnloadFile(&(Image->File), FileBuffer);
                }

                if (BinaryFile == NULL) {
                    ImCloseFile(&(Image->File));
                }
            }

            ImFreeMemory(Image);
            Image = NULL;
        }
    }

    if (LoadedImage != NULL) {
        *LoadedImage = Image;
    }

    if (Interpreter != NULL) {
        *Interpreter = InterpreterImage;
    }

    return Status;
}

KSTATUS
ImAddImage (
    PSTR BinaryName,
    PVOID Buffer,
    PLOADED_IMAGE *LoadedImage
    )

/*++

Routine Description:

    This routine adds the accounting structures for an image that has already
    been loaded into memory.

Arguments:

    ListHead - Supplies a pointer to the head of the list of loaded images.

    BinaryName - Supplies an optional pointer to the name of the image to use.
        If NULL, then the shared object name of the image will be extracted.

    Buffer - Supplies the base address of the loaded image.

    LoadedImage - Supplies an optional pointer where a pointer to the loaded
        image structure will be returned on success.

Return Value:

    Status code.

--*/

{

    PLOADED_IMAGE Image;
    UINTN NameSize;
    KSTATUS Status;

    NameSize = 0;
    if (BinaryName != NULL) {
        NameSize = RtlStringLength(BinaryName) + 1;
    }

    Image = ImpAllocateImage(BinaryName, NameSize);
    if (Image == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto AddExecutableEnd;
    }

    Image->Format = ImGetImageFormat(Buffer, -1);
    Image->LoadedLowestAddress = Buffer;
    Status = ImpAddImage(Image);

AddExecutableEnd:
    if (!KSUCCESS(Status)) {
        if (Image != NULL) {
            ImFreeMemory(Image);
            Image = NULL;
        }
    }

    if (LoadedImage != NULL) {
        *LoadedImage = Image;
    }

    return Status;
}

KSTATUS
ImLoadImports (
    PLIST_ENTRY ListHead
    )

/*++

Routine Description:

    This routine loads all import libraries for a given image list.

Arguments:

    ListHead - Supplies a pointer to the head of the list of loaded images to
        load import libraries for.

Return Value:

    Status code.

--*/

{

    PLOADED_IMAGE FirstImage;
    KSTATUS Status;

    if (LIST_EMPTY(ListHead)) {
        return STATUS_SUCCESS;
    }

    FirstImage = LIST_VALUE(ListHead->Next, LOADED_IMAGE, ListEntry);
    switch (FirstImage->Format) {
    case ImagePe32:
        Status = STATUS_SUCCESS;
        break;

    case ImageElf32:
        Status = ImpElfLoadAllImports(ListHead);
        break;

    default:

        ASSERT(FALSE);

        Status = STATUS_FILE_CORRUPT;
        break;
    }

    return Status;
}

KSTATUS
ImRelocateImages (
    PLIST_ENTRY ListHead
    )

/*++

Routine Description:

    This routine relocates all images that have not yet been relocated on the
    given list.

Arguments:

    ListHead - Supplies a pointer to the head of the list of loaded images to
        apply relocations for.

Return Value:

    Status code.

--*/

{

    PLOADED_IMAGE FirstImage;
    KSTATUS Status;

    if (LIST_EMPTY(ListHead)) {
        return STATUS_SUCCESS;
    }

    FirstImage = LIST_VALUE(ListHead->Next, LOADED_IMAGE, ListEntry);
    switch (FirstImage->Format) {
    case ImagePe32:
        Status = STATUS_SUCCESS;
        break;

    case ImageElf32:
        Status = ImpElfRelocateImages(ListHead);
        break;

    default:

        ASSERT(FALSE);

        Status = STATUS_FILE_CORRUPT;
        break;
    }

    return Status;
}

VOID
ImImageAddReference (
    PLOADED_IMAGE Image
    )

/*++

Routine Description:

    This routine increments the reference count on an image.

Arguments:

    Image - Supplies a pointer to the loaded image.

Return Value:

    None.

--*/

{

    ASSERT((Image->ReferenceCount != 0) &&
           (Image->ReferenceCount <= 0x10000000));

    RtlAtomicAdd32(&(Image->ReferenceCount), 1);
    return;
}

VOID
ImImageReleaseReference (
    PLOADED_IMAGE Image
    )

/*++

Routine Description:

    This routine releases a reference on a loaded executable image from memory.
    If this is the last reference, the image will be unloaded.

Arguments:

    Image - Supplies a pointer to the loaded image.

Return Value:

    None.

--*/

{

    ASSERT((Image->ReferenceCount != 0) &&
           (Image->ReferenceCount <= 0x10000000));

    if (RtlAtomicAdd32(&(Image->ReferenceCount), -1) != 1) {
        return;
    }

    ImNotifyImageUnload(Image);
    ImpUnloadImage(Image);
    LIST_REMOVE(&(Image->ListEntry));
    if (Image->AllocatorHandle != INVALID_HANDLE) {
        ImFreeAddressSpace(Image->AllocatorHandle,
                           Image->LoadedLowestAddress,
                           Image->Size);
    }

    if (Image->File.Handle != INVALID_HANDLE) {
        ImCloseFile(&(Image->File));
    }

    ImFreeMemory(Image);
    return;
}

KSTATUS
ImGetImageInformation (
    PVOID File,
    UINTN FileSize,
    PIMAGE_INFORMATION Information
    )

/*++

Routine Description:

    This routine gets various pieces of information about an image. This is the
    generic form that can get information from any supported image type.

Arguments:

    File - Supplies a pointer to the memory mapped file.

    FileSize - Supplies the size of the file.

    Information - Supplies a pointer to the information structure that will be
        filled out by this function. It is assumed the memory pointed to here
        is valid.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_UNKNOWN_IMAGE_FORMAT if the image is unknown or corrupt.

--*/

{

    PELF32_HEADER ElfHeader;
    BOOL IsElfImage;
    BOOL IsPeImage;
    PIMAGE_NT_HEADERS PeHeaders;
    KSTATUS Status;

    Status = STATUS_UNKNOWN_IMAGE_FORMAT;
    RtlZeroMemory(Information, sizeof(IMAGE_INFORMATION));

    //
    // Attempt to get image information for a PE image.
    //

    IsPeImage = ImpPeGetHeaders(File, FileSize, &PeHeaders);
    if (IsPeImage != FALSE) {
        Information->Format = ImagePe32;
        Information->ImageBase = PeHeaders->OptionalHeader.ImageBase;
        if (PeHeaders->FileHeader.Machine == IMAGE_FILE_MACHINE_I386) {
            Information->Machine = ImageMachineTypeX86;

        } else if (PeHeaders->FileHeader.Machine == IMAGE_FILE_MACHINE_ARMT) {
            Information->Machine = ImageMachineTypeArm32;

        } else {
            Information->Machine = ImageMachineTypeUnknown;
        }

        Information->EntryPoint = PeHeaders->OptionalHeader.AddressOfEntryPoint;
        Status = STATUS_SUCCESS;
        goto GetImageInformationEnd;
    }

    //
    // Attempt to get the image information for an ELF image.
    //

    IsElfImage = ImpElfGetHeader(File, FileSize, &ElfHeader);
    if (IsElfImage != FALSE) {
        Information->Format = ImageElf32;
        Information->ImageBase = 0;
        switch (ElfHeader->Machine) {
        case ELF_MACHINE_ARM:
            Information->Machine = ImageMachineTypeArm32;
            break;

        case ELF_MACHINE_I386:
            Information->Machine = ImageMachineTypeX86;
            break;

        default:
            Information->Machine = ImageMachineTypeUnknown;
            break;
        }

        Information->EntryPoint = ElfHeader->EntryPoint;
        Status = STATUS_SUCCESS;
        goto GetImageInformationEnd;
    }

GetImageInformationEnd:
    return Status;
}

BOOL
ImGetImageSection (
    PVOID File,
    UINTN FileSize,
    PSTR SectionName,
    PVOID *Section,
    PULONGLONG VirtualAddress,
    PULONG SectionSizeInFile,
    PULONG SectionSizeInMemory
    )

/*++

Routine Description:

    This routine gets a pointer to the given section in a PE image given a
    memory mapped file.

Arguments:

    File - Supplies a pointer to the image file mapped into memory.

    FileSize - Supplies the size of the memory mapped file, in bytes.

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

    IMAGE_FORMAT Format;

    Format = ImGetImageFormat(File, FileSize);
    switch (Format) {
    case ImagePe32:
        return ImpPeGetSection(File,
                               FileSize,
                               SectionName,
                               Section,
                               VirtualAddress,
                               SectionSizeInFile,
                               SectionSizeInMemory);

    case ImageElf32:
        return ImpElfGetSection(File,
                                FileSize,
                                SectionName,
                                Section,
                                VirtualAddress,
                                SectionSizeInFile,
                                SectionSizeInMemory);

    default:
        break;
    }

    //
    // The image format is unknown or invalid.
    //

    return FALSE;
}

IMAGE_FORMAT
ImGetImageFormat (
    PVOID FileBuffer,
    UINTN FileBufferSize
    )

/*++

Routine Description:

    This routine determines the file format for an image mapped in memory.

Arguments:

    FileBuffer - Supplies a pointer to the memory mapped file.

    FileBufferSize - Supplies the size of the file.

Return Value:

    Returns the file format of the image.

--*/

{

    PELF32_HEADER ElfHeader;
    BOOL IsElfImage;
    BOOL IsPeImage;
    PIMAGE_NT_HEADERS PeHeaders;

    //
    // Attempt to get the ELF image header.
    //

    IsElfImage = ImpElfGetHeader(FileBuffer, FileBufferSize, &ElfHeader);
    if (IsElfImage != FALSE) {
        return ImageElf32;
    }

    //
    // Attempt to get the PE image headers.
    //

    IsPeImage = ImpPeGetHeaders(FileBuffer, FileBufferSize, &PeHeaders);
    if (IsPeImage != FALSE) {
        return ImagePe32;
    }

    //
    // Unknown image format.
    //

    return ImageUnknownFormat;
}

KSTATUS
ImGetSymbolAddress (
    PLIST_ENTRY ListHead,
    PLOADED_IMAGE Image,
    PSTR SymbolName,
    BOOL Recursive,
    PVOID *Address
    )

/*++

Routine Description:

    This routine attempts to find an exported symbol with the given name in the
    given binary. This routine also looks through the image imports if the
    recursive flag is specified.

Arguments:

    ListHead - Supplies the head of the list of loaded images.

    Image - Supplies a pointer to the image to query.

    SymbolName - Supplies a pointer to the string containing the name of the
        symbol to search for.

    Recursive - Supplies a boolean indicating if the routine should recurse
        into imports or just query this binary.

    Address - Supplies a pointer where the address of the symbol will be
        returned on success, or NULL will be returned on failure.

Return Value:

    Status code.

--*/

{

    ULONG RecursionLevel;
    KSTATUS Status;
    UCHAR VisitMarker;

    //
    // Toggle between two values that are not the default. This means only one
    // thread can be in here at a time.
    //

    VisitMarker = 1;
    if (ImLastVisitMarker == 1) {
        VisitMarker = 2;
    }

    ImLastVisitMarker = VisitMarker;
    RecursionLevel = 0;
    if (Recursive == FALSE) {
        RecursionLevel = MAX_IMPORT_RECURSION_DEPTH;
    }

    Status = ImpGetSymbolAddress(ListHead,
                                 Image,
                                 SymbolName,
                                 RecursionLevel,
                                 VisitMarker,
                                 Address);

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
ImpGetImageSize (
    PLIST_ENTRY ListHead,
    PLOADED_IMAGE Image,
    PVOID FileBuffer,
    UINTN FileBufferSize,
    PSTR *InterpreterPath
    )

/*++

Routine Description:

    This routine determines the expanded image size and preferred image
    virtual address and stores that in the loaded image structure.

Arguments:

    ListHead - Supplies a pointer to the head of the list of loaded images.

    Image - Supplies a pointer to the loaded image structure. The format
        memeber is the only member that is required to be initialized.

    FileBuffer - Supplies a pointer to the memory mapped file.

    FileBufferSize - Supplies the size of the file.

    InterpreterPath - Supplies a pointer where the interpreter name will be
        returned if the program is requesting an interpreter.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    switch (Image->Format) {
    case ImagePe32:
        Status = STATUS_NOT_SUPPORTED;
        break;

    case ImageElf32:
        Status = ImpElfGetImageSize(ListHead,
                                    Image,
                                    FileBuffer,
                                    FileBufferSize,
                                    InterpreterPath);

        break;

    default:

        ASSERT(FALSE);

        Status = STATUS_INVALID_CONFIGURATION;
        break;
    }

    return Status;
}

KSTATUS
ImpLoadImage (
    PLIST_ENTRY ListHead,
    PLOADED_IMAGE Image,
    PVOID FileBuffer,
    UINTN FileBufferSize,
    ULONG ImportDepth
    )

/*++

Routine Description:

    This routine loads an executable image into virtual memory.

Arguments:

    ListHead - Supplies a pointer to the head of the list of loaded images.

    Image - Supplies a pointer to the loaded image. This must be partially
        filled out. Notable fields that must be filled out by the caller
        include the loaded virtual address and image size. This routine will
        fill out many other fields.

    FileBuffer - Supplies a pointer to the raw file, in memory.

    FileBufferSize - Supplies the size of the memory mapped file buffer, in
        bytes.

    ImportDepth - Supplies the import tree depth of the image being loaded.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_FILE_CORRUPT if the file headers were corrupt or unexpected.

    Other errors on failure.

--*/

{

    KSTATUS Status;

    switch (Image->Format) {
    case ImagePe32:
        Status = STATUS_NOT_SUPPORTED;
        break;

    case ImageElf32:
        Status = ImpElfLoadImage(ListHead,
                                 Image,
                                 FileBuffer,
                                 FileBufferSize,
                                 ImportDepth);

        break;

    default:
        Status = STATUS_UNKNOWN_IMAGE_FORMAT;
        break;
    }

    return Status;
}

KSTATUS
ImpAddImage (
    PLOADED_IMAGE Image
    )

/*++

Routine Description:

    This routine adds the accounting structures for an image that has already
    been loaded into memory.

Arguments:

    Image - Supplies a pointer to the image to initialize.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    switch (Image->Format) {
    case ImageElf32:
        Status = ImpElfAddImage(Image);
        break;

    default:
        Status = STATUS_UNKNOWN_IMAGE_FORMAT;
        break;
    }

    return Status;
}

VOID
ImpUnloadImage (
    PLOADED_IMAGE Image
    )

/*++

Routine Description:

    This routine unloads an executable image from virtual memory.

Arguments:

    Image - Supplies a pointer to the loaded image.

Return Value:

    None.

--*/

{

    switch (Image->Format) {
    case ImageElf32:
        ImpElfUnloadImage(Image);
        break;

    default:

        ASSERT(FALSE);

        break;
    }

    return;
}

KSTATUS
ImpGetSymbolAddress (
    PLIST_ENTRY ListHead,
    PLOADED_IMAGE Image,
    PSTR SymbolName,
    ULONG RecursionLevel,
    UCHAR VisitMarker,
    PVOID *Address
    )

/*++

Routine Description:

    This routine attempts to find an exported symbol with the given name in the
    given binary. This routine also looks through the image imports if the
    recursive flag is specified.

Arguments:

    ListHead - Supplies the head of the list of loaded images.

    Image - Supplies a pointer to the image to query.

    SymbolName - Supplies a pointer to the string containing the name of the
        symbol to search for.

    RecursionLevel - Supplies the current level of recursion.

    VisitMarker - Supplies the value that images are marked with to indicate
        they've been visited in this trip already.

    Address - Supplies a pointer where the address of the symbol will be
        returned on success, or NULL will be returned on failure.

Return Value:

    Status code.

--*/

{

    PLOADED_IMAGE Import;
    ULONG ImportIndex;
    KSTATUS Status;

    *Address = NULL;
    if (Image == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    switch (Image->Format) {
    case ImageElf32:
        Status = ImpElfGetSymbolAddress(ListHead,
                                        Image,
                                        SymbolName,
                                        Address);

        break;

    default:
        Status = STATUS_UNKNOWN_IMAGE_FORMAT;
        break;
    }

    if ((Status != STATUS_NOT_FOUND) ||
        (RecursionLevel >= MAX_IMPORT_RECURSION_DEPTH)) {

        return Status;
    }

    Image->VisitMarker = VisitMarker;
    for (ImportIndex = 0; ImportIndex < Image->ImportCount; ImportIndex += 1) {
        Import = Image->Imports[ImportIndex];
        if ((Import != NULL) && (Import->VisitMarker != VisitMarker)) {
            Status = ImpGetSymbolAddress(ListHead,
                                         Import,
                                         SymbolName,
                                         RecursionLevel + 1,
                                         VisitMarker,
                                         Address);

            if (Status != STATUS_NOT_FOUND) {
                return Status;
            }
        }
    }

    //
    // The image format is unknown or invalid.
    //

    return Status;
}

PLOADED_IMAGE
ImpFindImageInList (
    PLIST_ENTRY ListHead,
    PSTR ImageName
    )

/*++

Routine Description:

    This routine attempts to find an image with the given name in the given
    list.

Arguments:

    ListHead - Supplies a pointer to the head of the list of images to
        search through.

    ImageName - Supplies a pointer to a string containing the name of the image.

Return Value:

    Returns a pointer to the image within the list on success.

    NULL on failure.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PLOADED_IMAGE Image;
    ULONG NameLength;

    NameLength = RtlStringLength(ImageName) + 1;
    CurrentEntry = ListHead->Next;
    while (CurrentEntry != ListHead) {
        Image = LIST_VALUE(CurrentEntry, LOADED_IMAGE, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if (Image->BinaryName == NULL) {
            continue;
        }

        if (RtlAreStringsEqual(Image->BinaryName, ImageName, NameLength) !=
            FALSE) {

            //
            // This routine is used to load real images, so it would be bad to
            // return a placeholder image here.
            //

            ASSERT((Image->LoadFlags & IMAGE_LOAD_FLAG_PLACEHOLDER) == 0);

            return Image;
        }
    }

    return NULL;
}

PLOADED_IMAGE
ImpAllocateImage (
    PSTR ImageName,
    UINTN ImageNameSize
    )

/*++

Routine Description:

    This routine allocates a new loaded image structure, and initializes some
    basic fields and the name.

Arguments:

    ImageName - Supplies a pointer to a string containing the name of the image.
        A copy of this string will be made.

    ImageNameSize - Supplies the number of bytes in the complete image name,
        including the null terminator.

Return Value:

    Returns a pointer to the newly allocated image structure on success.

    NULL on failure.

--*/

{

    PLOADED_IMAGE Image;

    //
    // Allocate space for the loaded image structure.
    //

    Image = ImAllocateMemory(sizeof(LOADED_IMAGE) + ImageNameSize,
                             IM_ALLOCATION_TAG);

    if (Image == NULL) {
        return NULL;
    }

    RtlZeroMemory(Image, sizeof(LOADED_IMAGE));
    Image->ReferenceCount = 1;
    Image->AllocatorHandle = INVALID_HANDLE;
    Image->File.Handle = INVALID_HANDLE;
    Image->TlsOffset = -1;
    if (ImageNameSize != 0) {
        Image->BinaryName = (PSTR)(Image + 1);
        RtlStringCopy(Image->BinaryName, ImageName, ImageNameSize);
    }

    return Image;
}

