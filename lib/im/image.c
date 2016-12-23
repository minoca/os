/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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
ImpOpenLibrary (
    PLIST_ENTRY ListHead,
    PLOADED_IMAGE Parent,
    PVOID SystemContext,
    PCSTR BinaryName,
    PIMAGE_FILE_INFORMATION File,
    PSTR *Path
    );

KSTATUS
ImpGetImageSize (
    PLIST_ENTRY ListHead,
    PLOADED_IMAGE Image,
    PIMAGE_BUFFER Buffer,
    PSTR *InterpreterPath
    );

KSTATUS
ImpLoadImage (
    PLIST_ENTRY ListHead,
    PLOADED_IMAGE Image,
    PIMAGE_BUFFER Buffer
    );

KSTATUS
ImpAddImage (
    PIMAGE_BUFFER ImageBuffer,
    PLOADED_IMAGE Image
    );

KSTATUS
ImpOpenImport (
    PLIST_ENTRY ListHead,
    PLOADED_IMAGE Parent,
    PVOID SystemContext,
    PCSTR BinaryName,
    PIMAGE_FILE_INFORMATION File,
    PSTR *Path
    );

VOID
ImpUnloadImage (
    PLOADED_IMAGE Image
    );

KSTATUS
ImpGetSymbolByName (
    PLOADED_IMAGE Image,
    PSTR SymbolName,
    ULONG RecursionLevel,
    ULONG VisitMarker,
    PIMAGE_SYMBOL Symbol
    );

KSTATUS
ImpGetSymbolByAddress (
    PLOADED_IMAGE Image,
    PVOID Address,
    ULONG RecursionLevel,
    ULONG VisitMarker,
    PIMAGE_SYMBOL Symbol
    );

VOID
ImpRelocateSelf (
    PIMAGE_BUFFER Buffer,
    PIM_RESOLVE_PLT_ENTRY PltResolver,
    PLOADED_IMAGE Image
    );

PLOADED_IMAGE
ImpFindImageByLibraryName (
    PLIST_ENTRY ListHead,
    PCSTR Name
    );

PLOADED_IMAGE
ImpFindImageByFile (
    PLIST_ENTRY ListHead,
    PIMAGE_FILE_INFORMATION File
    );

PLOADED_IMAGE
ImpAllocateImage (
    VOID
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
    PIMAGE_FILE_INFORMATION ImageFile,
    PIMAGE_BUFFER ImageBuffer,
    PIMAGE_FORMAT Format
    )

/*++

Routine Description:

    This routine determines the executable format of a given image path.

Arguments:

    BinaryName - Supplies the name of the binary executable image to examine.

    SystemContext - Supplies the context pointer passed to the load executable
        function.

    ImageFile - Supplies an optional pointer where the file handle and other
        information will be returned on success.

    ImageBuffer - Supplies an optional pointer where the image buffer
        information will be returned.

    Format - Supplies a pointer where the format will be returned on success.

Return Value:

    Status code.

--*/

{

    ULONG BinaryNameLength;
    IMAGE_BUFFER Buffer;
    IMAGE_FILE_INFORMATION File;
    KSTATUS Status;

    File.Handle = INVALID_HANDLE;
    RtlZeroMemory(&Buffer, sizeof(IMAGE_BUFFER));
    BinaryNameLength = RtlStringLength(BinaryName);
    if (BinaryNameLength == 0) {
        Status = STATUS_INVALID_PARAMETER;
        goto GetExecutableFormatEnd;
    }

    //
    // Load the file contents into memory.
    //

    Status = ImpOpenLibrary(NULL, NULL, SystemContext, BinaryName, &File, NULL);
    if (!KSUCCESS(Status)) {
        goto GetExecutableFormatEnd;
    }

    Status = ImReadFile(&File,
                        0,
                        IMAGE_INITIAL_READ_SIZE,
                        &Buffer);

    if (!KSUCCESS(Status)) {
        goto GetExecutableFormatEnd;
    }

    //
    // Determine the file format.
    //

    *Format = ImGetImageFormat(&Buffer);
    if ((*Format == ImageInvalidFormat) || (*Format == ImageUnknownFormat)) {
        Status = STATUS_UNKNOWN_IMAGE_FORMAT;
        goto GetExecutableFormatEnd;
    }

    Status = STATUS_SUCCESS;

GetExecutableFormatEnd:
    if (((!KSUCCESS(Status)) || (ImageBuffer == NULL)) &&
        (Buffer.Data != NULL)) {

        ImUnloadBuffer(&File, &Buffer);
        Buffer.Data = NULL;
    }

    if (((!KSUCCESS(Status)) || (ImageFile == NULL)) &&
        (File.Handle != INVALID_HANDLE)) {

        ImCloseFile(&File);
        File.Handle = INVALID_HANDLE;
    }

    if (ImageFile != NULL) {
        RtlCopyMemory(ImageFile, &File, sizeof(IMAGE_FILE_INFORMATION));
    }

    if (ImageBuffer != NULL) {
        RtlCopyMemory(ImageBuffer, &Buffer, sizeof(IMAGE_BUFFER));
    }

    return Status;
}

KSTATUS
ImLoad (
    PLIST_ENTRY ListHead,
    PCSTR BinaryName,
    PIMAGE_FILE_INFORMATION BinaryFile,
    PIMAGE_BUFFER ImageBuffer,
    PVOID SystemContext,
    ULONG Flags,
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

    ImageBuffer - Supplies an optional pointer to the image buffer. This can
        be a complete image file buffer, or just a partial load of the file.

    SystemContext - Supplies an opaque token that will be passed to the
        support functions called by the image support library.

    Flags - Supplies a bitfield of flags governing the load. See
        IMAGE_LOAD_FLAG_* flags.

    LoadedImage - Supplies an optional pointer where a pointer to the loaded
        image structure will be returned on success.

    Interpreter - Supplies an optional pointer where a pointer to the loaded
        interpreter structure will be returned on success.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    Status = ImpLoad(ListHead,
                     BinaryName,
                     BinaryFile,
                     ImageBuffer,
                     SystemContext,
                     Flags,
                     NULL,
                     LoadedImage,
                     Interpreter);

    return Status;
}

KSTATUS
ImAddImage (
    PIMAGE_BUFFER Buffer,
    PLOADED_IMAGE *LoadedImage
    )

/*++

Routine Description:

    This routine adds the accounting structures for an image that has already
    been loaded into memory.

Arguments:

    Buffer - Supplies the image buffer containing the loaded image.

    LoadedImage - Supplies an optional pointer where a pointer to the loaded
        image structure will be returned on success.

Return Value:

    Status code.

--*/

{

    PLOADED_IMAGE Image;
    KSTATUS Status;

    Image = ImpAllocateImage();
    if (Image == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto AddImageEnd;
    }

    Image->Format = ImGetImageFormat(Buffer);
    Image->LoadedImageBuffer = Buffer->Data;
    Image->File.Size = Buffer->Size;
    Status = ImpAddImage(Buffer, Image);

    //
    // Set the file name equal to the library name so there's at least
    // something to go off of.
    //

    Image->FileName = Image->LibraryName;

AddImageEnd:
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
        Status = ImpElf32LoadAllImports(ListHead);
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
        Status = ImpElf32RelocateImages(ListHead);
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
        ImFreeAddressSpace(Image);
    }

    if (Image->File.Handle != INVALID_HANDLE) {
        ImCloseFile(&(Image->File));
    }

    if (Image->FileName != NULL) {
        ImFreeMemory(Image->FileName);
    }

    ImFreeMemory(Image);
    return;
}

KSTATUS
ImGetImageInformation (
    PIMAGE_BUFFER Buffer,
    PIMAGE_INFORMATION Information
    )

/*++

Routine Description:

    This routine gets various pieces of information about an image. This is the
    generic form that can get information from any supported image type.

Arguments:

    Buffer - Supplies a pointer to the image buffer.

    Information - Supplies a pointer to the information structure that will be
        filled out by this function. It is assumed the memory pointed to here
        is valid.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_UNKNOWN_IMAGE_FORMAT if the image is unknown or corrupt.

--*/

{

    LOADED_IMAGE Image;
    BOOL IsPeImage;
    PIMAGE_NT_HEADERS PeHeaders;
    KSTATUS Status;

    Status = STATUS_UNKNOWN_IMAGE_FORMAT;
    RtlZeroMemory(Information, sizeof(IMAGE_INFORMATION));

    //
    // Attempt to get image information for a PE image.
    //

    IsPeImage = ImpPeGetHeaders(Buffer, &PeHeaders);
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

    RtlZeroMemory(&Image, sizeof(LOADED_IMAGE));
    Status = ImpElf32GetImageSize(NULL, &Image, Buffer, NULL);
    if (KSUCCESS(Status)) {
        Information->Format = Image.Format;
        Information->Machine = Image.Machine;
        Information->EntryPoint = (UINTN)(Image.EntryPoint);
        Information->ImageBase = (UINTN)(Image.PreferredLowestAddress);
        goto GetImageInformationEnd;
    }

GetImageInformationEnd:
    return Status;
}

BOOL
ImGetImageSection (
    PIMAGE_BUFFER Buffer,
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

    IMAGE_FORMAT Format;

    Format = ImGetImageFormat(Buffer);
    switch (Format) {
    case ImagePe32:
        return ImpPeGetSection(Buffer,
                               SectionName,
                               Section,
                               VirtualAddress,
                               SectionSizeInFile,
                               SectionSizeInMemory);

    case ImageElf32:
        return ImpElf32GetSection(Buffer,
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
    PIMAGE_BUFFER Buffer
    )

/*++

Routine Description:

    This routine determines the file format for an image mapped in memory.

Arguments:

    Buffer - Supplies a pointer to the image buffer to determine the type of.

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

    IsElfImage = ImpElf32GetHeader(Buffer, &ElfHeader);
    if (IsElfImage != FALSE) {
        return ImageElf32;
    }

    //
    // Attempt to get the PE image headers.
    //

    IsPeImage = ImpPeGetHeaders(Buffer, &PeHeaders);
    if (IsPeImage != FALSE) {
        return ImagePe32;
    }

    //
    // Unknown image format.
    //

    return ImageUnknownFormat;
}

KSTATUS
ImGetSymbolByName (
    PLOADED_IMAGE Image,
    PSTR SymbolName,
    BOOL Recursive,
    PIMAGE_SYMBOL Symbol
    )

/*++

Routine Description:

    This routine attempts to find an exported symbol with the given name in the
    given binary. This routine also looks through the image imports if the
    recursive flag is specified.

Arguments:

    Image - Supplies a pointer to the image to query.

    SymbolName - Supplies a pointer to the string containing the name of the
        symbol to search for.

    Recursive - Supplies a boolean indicating if the routine should recurse
        into imports or just query this binary.

    Symbol - Supplies a pointer to a structure that receives the symbol's
        information on success.

Return Value:

    Status code.

--*/

{

    ULONG RecursionLevel;
    KSTATUS Status;
    UCHAR VisitMarker;

    //
    // Get a new visitor generation number. This means only one thread can be
    // in here at a time.
    //

    ImLastVisitMarker += 1;
    VisitMarker = ImLastVisitMarker;
    RecursionLevel = 0;
    if (Recursive == FALSE) {
        RecursionLevel = MAX_IMPORT_RECURSION_DEPTH;
    }

    Status = ImpGetSymbolByName(Image,
                                SymbolName,
                                RecursionLevel,
                                VisitMarker,
                                Symbol);

    return Status;
}

KSTATUS
ImGetSymbolByAddress (
    PLOADED_IMAGE Image,
    PVOID Address,
    BOOL Recursive,
    PIMAGE_SYMBOL Symbol
    )

/*++

Routine Description:

    This routine attempts to resolve the given address into a symbol. This
    routine also looks through the image imports if the recursive flag is
    specified.

Arguments:

    Image - Supplies a pointer to the image to query.

    Address - Supplies the address to search for.

    Recursive - Supplies a boolean indicating if the routine should recurse
        into imports or just query this binary.

    Symbol - Supplies a pointer to a structure that receives the address's
        symbol information on success.

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

    ImLastVisitMarker += 1;
    VisitMarker = ImLastVisitMarker;
    RecursionLevel = 0;
    if (Recursive == FALSE) {
        RecursionLevel = MAX_IMPORT_RECURSION_DEPTH;
    }

    Status = ImpGetSymbolByAddress(Image,
                                   Address,
                                   RecursionLevel,
                                   VisitMarker,
                                   Symbol);

    return Status;
}

VOID
ImRelocateSelf (
    PVOID Base,
    PIM_RESOLVE_PLT_ENTRY PltResolver
    )

/*++

Routine Description:

    This routine relocates the currently running image.

Arguments:

    Base - Supplies a pointer to the base of the loaded image.

    PltResolver - Supplies a pointer to the function used to resolve PLT
        entries.

Return Value:

    None.

--*/

{

    IMAGE_BUFFER Buffer;
    LOADED_IMAGE FakeImage;

    Buffer.Context = NULL;
    Buffer.Data = Base;
    Buffer.Size = -1;
    RtlZeroMemory(&FakeImage, sizeof(LOADED_IMAGE));
    FakeImage.Format = ImGetImageFormat(&Buffer);
    ImpRelocateSelf(&Buffer, PltResolver, &FakeImage);
    return;
}

PVOID
ImResolvePltEntry (
    PLIST_ENTRY ListHead,
    PLOADED_IMAGE Image,
    UINTN RelocationOffset
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

    switch (Image->Format) {
    case ImageElf32:
        FunctionAddress = ImpElf32ResolvePltEntry(ListHead,
                                                  Image,
                                                  RelocationOffset);

        break;

    default:

        ASSERT(FALSE);

        FunctionAddress = NULL;
        break;
    }

    return FunctionAddress;
}

PVOID
ImpReadBuffer (
    PIMAGE_FILE_INFORMATION File,
    PIMAGE_BUFFER Buffer,
    UINTN Offset,
    UINTN Size
    )

/*++

Routine Description:

    This routine handles access to an image buffer.

Arguments:

    File - Supplies an optional pointer to the file information, if the buffer
        may need to be resized.

    Buffer - Supplies a pointer to the buffer to read from.

    Offset - Supplies the offset from the start of the file to read.

    Size - Supplies the required size.

Return Value:

    Returns a pointer to the image file at the requested offset on success.

    NULL if the range is invalid or the file could not be fully loaded.

--*/

{

    UINTN End;
    KSTATUS Status;

    End = Offset + Size;
    if (Offset > End) {
        return NULL;
    }

    //
    // In most cases, the buffer can satisfy the request.
    //

    if ((Buffer->Data != NULL) &&
        (Offset < Buffer->Size) &&
        (End <= Buffer->Size)) {

        return Buffer->Data + Offset;
    }

    //
    // If there's no file, buffer is already the entire file, or the entire
    // file wouldn't satisfy the request, fail.
    //

    if ((File == NULL) || (Buffer->Size == File->Size) || (End > File->Size)) {
        return NULL;
    }

    //
    // Unload the current buffer.
    //

    ImUnloadBuffer(File, Buffer);
    RtlZeroMemory(Buffer, sizeof(IMAGE_BUFFER));

    //
    // Load up the whole file.
    //

    Status = ImLoadFile(File, Buffer);
    if (!KSUCCESS(Status)) {
        RtlDebugPrint("Failed to load file: %d\n", Status);
        return NULL;
    }

    ASSERT(End <= Buffer->Size);

    return Buffer->Data + Offset;
}

KSTATUS
ImpLoad (
    PLIST_ENTRY ListHead,
    PCSTR BinaryName,
    PIMAGE_FILE_INFORMATION BinaryFile,
    PIMAGE_BUFFER ImageBuffer,
    PVOID SystemContext,
    ULONG Flags,
    PLOADED_IMAGE Parent,
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

    ImageBuffer - Supplies an optional pointer to the image buffer. This can
        be a complete image file buffer, or just a partial load of the file.

    SystemContext - Supplies an opaque token that will be passed to the
        support functions called by the image support library.

    Flags - Supplies a bitfield of flags governing the load. See
        IMAGE_LOAD_FLAG_* flags.

    Parent - Supplies an optional pointer to the parent image that imports this
        image.

    LoadedImage - Supplies an optional pointer where a pointer to the loaded
        image structure will be returned on success.

    Interpreter - Supplies an optional pointer where a pointer to the loaded
        interpreter structure will be returned on success.

Return Value:

    Status code.

--*/

{

    ULONG BinaryNameLength;
    PLOADED_IMAGE ExistingImage;
    PLOADED_IMAGE Image;
    PLOADED_IMAGE InterpreterImage;
    PSTR InterpreterPath;
    IMAGE_BUFFER LocalImageBuffer;
    KSTATUS Status;

    Image = NULL;
    InterpreterImage = NULL;
    RtlZeroMemory(&LocalImageBuffer, sizeof(IMAGE_BUFFER));

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

        Image = ImpGetPrimaryExecutable(ListHead);
        if (Image == NULL) {
            Status = STATUS_NOT_READY;
            goto LoadEnd;
        }

        ImImageAddReference(Image);
        Status = STATUS_SUCCESS;
        goto LoadEnd;
    }

    BinaryNameLength = RtlStringLength(BinaryName);
    if (BinaryNameLength == 0) {
        Status = STATUS_INVALID_PARAMETER;
        goto LoadEnd;
    }

    //
    // See if the image is already loaded, and return if so.
    //

    Image = ImpFindImageByLibraryName(ListHead, BinaryName);
    if (Image != NULL) {
        ImImageAddReference(Image);
        Status = STATUS_SUCCESS;
        goto LoadEnd;
    }

    //
    // Allocate space for the loaded image structure.
    //

    Image = ImpAllocateImage();
    if (Image == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto LoadEnd;
    }

    Image->SystemContext = SystemContext;
    if (Parent != NULL) {

        ASSERT((Flags & IMAGE_LOAD_FLAG_IGNORE_INTERPRETER) != 0);

        Image->Parent = Parent;
        Image->ImportDepth = Parent->ImportDepth + 1;
    }

    Image->LoadFlags = Flags;

    //
    // Open up the file.
    //

    if (BinaryFile != NULL) {
        RtlCopyMemory(&(Image->File),
                      BinaryFile,
                      sizeof(IMAGE_FILE_INFORMATION));

        Image->FileName = ImAllocateMemory(BinaryNameLength + 1,
                                           IM_ALLOCATION_TAG);

        if (Image->FileName == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto LoadEnd;
        }

        RtlCopyMemory(Image->FileName, BinaryName, BinaryNameLength + 1);

    } else {
        Status = ImpOpenLibrary(ListHead,
                                Parent,
                                SystemContext,
                                BinaryName,
                                &(Image->File),
                                &(Image->FileName));

        if (!KSUCCESS(Status)) {
            goto LoadEnd;
        }

        //
        // The library is open, and the real path has been found. Search
        // for an already loaded library with the same absolute path.
        //

        ExistingImage = ImpFindImageByFile(ListHead, &(Image->File));
        if (ExistingImage != NULL) {
            ImCloseFile(&(Image->File));
            ImFreeMemory(Image->FileName);
            ImFreeMemory(Image);
            Image = ExistingImage;
            ImImageAddReference(Image);
            Status = STATUS_SUCCESS;
            goto LoadEnd;
        }
    }

    if (ImageBuffer == NULL) {

        //
        // In a load-only scenario, just try to do a small read of the file
        // contents. Otherwise, just map the whole file.
        //

        if ((Flags & IMAGE_LOAD_FLAG_LOAD_ONLY) != 0) {
            Status = ImReadFile(&(Image->File),
                                0,
                                IMAGE_INITIAL_READ_SIZE,
                                &LocalImageBuffer);

        } else {
            Status = ImLoadFile(&(Image->File), &LocalImageBuffer);
        }

        if (!KSUCCESS(Status)) {
            goto LoadEnd;
        }

        ImageBuffer = &LocalImageBuffer;
    }

    //
    // Determine the file format.
    //

    Image->Format = ImGetImageFormat(ImageBuffer);
    if ((Image->Format == ImageInvalidFormat) ||
        (Image->Format == ImageUnknownFormat)) {

        Status = STATUS_UNKNOWN_IMAGE_FORMAT;
        goto LoadEnd;
    }

    //
    // Determine the image size and preferred VA.
    //

    Status = ImpGetImageSize(ListHead, Image, ImageBuffer, &InterpreterPath);
    if (!KSUCCESS(Status)) {
        goto LoadEnd;
    }

    //
    // Load the interpreter if there is one.
    //

    if (InterpreterPath != NULL) {

        ASSERT(((Flags & IMAGE_LOAD_FLAG_IGNORE_INTERPRETER) == 0) &&
               (Parent == NULL));

        Status = ImpLoad(ListHead,
                         InterpreterPath,
                         NULL,
                         NULL,
                         SystemContext,
                         Flags | IMAGE_LOAD_FLAG_IGNORE_INTERPRETER,
                         NULL,
                         &InterpreterImage,
                         NULL);

        if (!KSUCCESS(Status)) {
            goto LoadEnd;
        }
    }

    if (ImAllocateAddressSpace != NULL) {

        //
        // Call out to the allocator to get space for the image.
        //

        Image->BaseDifference = 0;
        Status = ImAllocateAddressSpace(Image);
        if (!KSUCCESS(Status)) {
            goto LoadEnd;
        }

        //
        // If the image is not relocatable and the preferred address could not
        // be allocated, then this image cannot be loaded.
        //

        if ((Image->BaseDifference != 0) &&
            ((Image->Flags & IMAGE_FLAG_RELOCATABLE) == 0)) {

            Status = STATUS_MEMORY_CONFLICT;
            goto LoadEnd;
        }

    //
    // Just pretend for now it got put at the right spot. This will be adjusted
    // later.
    //

    } else {
        Image->BaseDifference = 0;
    }

    //
    // Call the image-specific routine to actually load/map the image into its
    // allocated space.
    //

    Status = ImpLoadImage(ListHead, Image, ImageBuffer);
    if (!KSUCCESS(Status)) {
        goto LoadEnd;
    }

LoadEnd:

    //
    // Tear down the portion of the image loaded so far on failure.
    //

    if (!KSUCCESS(Status)) {
        if (InterpreterImage != NULL) {
            ImImageReleaseReference(InterpreterImage);
        }

        if (Image != NULL) {
            if (Image->AllocatorHandle != INVALID_HANDLE) {
                ImFreeAddressSpace(Image);
            }

            if (Image->File.Handle != INVALID_HANDLE) {
                if (LocalImageBuffer.Data != NULL) {
                    ImUnloadBuffer(&(Image->File), &LocalImageBuffer);
                }

                if (BinaryFile == NULL) {
                    ImCloseFile(&(Image->File));
                }
            }

            if (Image->FileName != NULL) {
                ImFreeMemory(Image->FileName);
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

PLOADED_IMAGE
ImpGetPrimaryExecutable (
    PLIST_ENTRY ListHead
    )

/*++

Routine Description:

    This routine returns the primary executable in the list, if there is one.

Arguments:

    ListHead - Supplies a pointer to the head of the list of loaded images.

Return Value:

    Returns a pointer to the primary executable if it exists. This routine does
    not add a reference on the image.

    NULL if no primary executable is currently loaded in the list.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PLOADED_IMAGE Image;

    if (ListHead == NULL) {
        return NULL;
    }

    CurrentEntry = ListHead->Next;
    while (CurrentEntry != ListHead) {
        Image = LIST_VALUE(CurrentEntry, LOADED_IMAGE, ListEntry);
        if ((Image->LoadFlags & IMAGE_LOAD_FLAG_PRIMARY_EXECUTABLE) != 0) {
            return Image;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    return NULL;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
ImpOpenLibrary (
    PLIST_ENTRY ListHead,
    PLOADED_IMAGE Parent,
    PVOID SystemContext,
    PCSTR BinaryName,
    PIMAGE_FILE_INFORMATION File,
    PSTR *Path
    )

/*++

Routine Description:

    This routine attempts to open a file.

Arguments:

    ListHead - Supplies an optional pointer to the head of the list of loaded
        images.

    Parent - Supplies an optional pointer to the parent image requiring this
        image for load.

    SystemContext - Supplies the context pointer passed to the load executable
        function.

    BinaryName - Supplies the name of the executable image to open.

    File - Supplies a pointer where the information for the file including its
        open handle will be returned.

    Image - Supplies a pointer where an existing image may be returned.

    Path - Supplies a pointer where the real path to the opened file will be
        returned. The caller is responsible for freeing this memory.

Return Value:

    Status code.

--*/

{

    ULONG NameLength;
    KSTATUS Status;

    if (Parent == NULL) {
        Parent = ImpGetPrimaryExecutable(ListHead);
    }

    //
    // If this is an executable being loaded for the first time, just try to
    // open the file directly. No extra paths are searched.
    //

    if (Parent == NULL) {
        Status = ImOpenFile(SystemContext, BinaryName, File);
        if (KSUCCESS(Status)) {
            if (Path == NULL) {
                Status = STATUS_SUCCESS;

            } else {
                NameLength = RtlStringLength(BinaryName);
                *Path = ImAllocateMemory(NameLength + 1, IM_ALLOCATION_TAG);
                if (*Path != NULL) {
                    RtlCopyMemory(*Path, BinaryName, NameLength + 1);

                } else {
                    Status = STATUS_INSUFFICIENT_RESOURCES;
                }
            }

            if (!KSUCCESS(Status)) {
                ImCloseFile(File);
                File->Handle = INVALID_HANDLE;
            }
        }

    } else {
        Status = ImpOpenImport(ListHead,
                               Parent,
                               SystemContext,
                               BinaryName,
                               File,
                               Path);
    }

    return Status;
}

KSTATUS
ImpGetImageSize (
    PLIST_ENTRY ListHead,
    PLOADED_IMAGE Image,
    PIMAGE_BUFFER Buffer,
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

    Buffer - Supplies a pointer to the loaded image buffer.

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
        Status = ImpElf32GetImageSize(ListHead, Image, Buffer, InterpreterPath);
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
    PIMAGE_BUFFER Buffer
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

    Buffer - Supplies a pointer to the image file buffer.

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
        Status = ImpElf32LoadImage(ListHead, Image, Buffer);
        break;

    default:
        Status = STATUS_UNKNOWN_IMAGE_FORMAT;
        break;
    }

    return Status;
}

KSTATUS
ImpAddImage (
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

    KSTATUS Status;

    switch (Image->Format) {
    case ImageElf32:
        Status = ImpElf32AddImage(ImageBuffer, Image);
        break;

    default:
        Status = STATUS_UNKNOWN_IMAGE_FORMAT;
        break;
    }

    return Status;
}

KSTATUS
ImpOpenImport (
    PLIST_ENTRY ListHead,
    PLOADED_IMAGE Parent,
    PVOID SystemContext,
    PCSTR BinaryName,
    PIMAGE_FILE_INFORMATION File,
    PSTR *Path
    )

/*++

Routine Description:

    This routine attempts to open a file.

Arguments:

    ListHead - Supplies an optional pointer to the head of the list of loaded
        images.

    Parent - Supplies an optional pointer to the parent image requiring this
        image for load.

    SystemContext - Supplies the context pointer passed to the load executable
        function.

    BinaryName - Supplies the name of the executable image to open.

    File - Supplies a pointer where the information for the file including its
        open handle will be returned.

    Image - Supplies a pointer where an existing image may be returned.

    Path - Supplies a pointer where the real path to the opened file will be
        returned. The caller is responsible for freeing this memory.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    ASSERT(Parent->SystemContext == SystemContext);

    switch (Parent->Format) {
    case ImageElf32:
        Status = ImpElf32OpenLibrary(ListHead, Parent, BinaryName, File, Path);
        break;

    default:

        ASSERT(FALSE);

        Status = STATUS_INVALID_CONFIGURATION;
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
        ImpElf32UnloadImage(Image);
        break;

    default:

        ASSERT(FALSE);

        break;
    }

    return;
}

KSTATUS
ImpGetSymbolByName (
    PLOADED_IMAGE Image,
    PSTR SymbolName,
    ULONG RecursionLevel,
    ULONG VisitMarker,
    PIMAGE_SYMBOL Symbol
    )

/*++

Routine Description:

    This routine attempts to find an exported symbol with the given name in the
    given binary. This routine also looks through the image imports if the
    recursive level is not greater than or equal to the maximum import
    recursion depth.

Arguments:

    Image - Supplies a pointer to the image to query.

    SymbolName - Supplies a pointer to the string containing the name of the
        symbol to search for.

    RecursionLevel - Supplies the current level of recursion.

    VisitMarker - Supplies the value that images are marked with to indicate
        they've been visited in this trip already.

    Symbol - Supplies a pointer to a structure that receives the symbol's
        information on success.

Return Value:

    Status code.

--*/

{

    PLOADED_IMAGE Import;
    ULONG ImportIndex;
    KSTATUS Status;

    if (Image == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    switch (Image->Format) {
    case ImageElf32:
        Status = ImpElf32GetSymbolByName(Image, SymbolName, Symbol);
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
            Status = ImpGetSymbolByName(Import,
                                        SymbolName,
                                        RecursionLevel + 1,
                                        VisitMarker,
                                        Symbol);

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

KSTATUS
ImpGetSymbolByAddress (
    PLOADED_IMAGE Image,
    PVOID Address,
    ULONG RecursionLevel,
    ULONG VisitMarker,
    PIMAGE_SYMBOL Symbol
    )

/*++

Routine Description:

    This routine attempts to resolve the given address into a symbol. This
    routine also looks through the image imports if the recursive level is not
    greater than or equal to the maximum import recursion depth.

Arguments:

    Image - Supplies a pointer to the image to query.

    Address - Supplies the address to search for.

    RecursionLevel - Supplies the current level of recursion.

    VisitMarker - Supplies the value that images are marked with to indicate
        they've been visited in this trip already.

    Symbol - Supplies a pointer to a structure that receives the address's
        symbol information on success.

Return Value:

    Status code.

--*/

{

    PLOADED_IMAGE Import;
    ULONG ImportIndex;
    KSTATUS Status;

    if (Image == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    switch (Image->Format) {
    case ImageElf32:
        Status = ImpElf32GetSymbolByAddress(Image, Address, Symbol);
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
            Status = ImpGetSymbolByAddress(Import,
                                           Address,
                                           RecursionLevel + 1,
                                           VisitMarker,
                                           Symbol);

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

VOID
ImpRelocateSelf (
    PIMAGE_BUFFER Buffer,
    PIM_RESOLVE_PLT_ENTRY PltResolver,
    PLOADED_IMAGE Image
    )

/*++

Routine Description:

    This routine relocates the currently running image.

Arguments:

    Buffer - Supplies a pointer to an initialized buffer pointing at the base
        of the loaded image.

    PltResolver - Supplies a pointer to the function used to resolve PLT
        entries.

    Image - Supplies a pointer to a zeroed out image structure. The image
        format should be initialized. This can be stack allocated.

Return Value:

    None.

--*/

{

    switch (Image->Format) {
    case ImageElf32:
        ImpElf32RelocateSelf(Buffer, PltResolver, Image);
        break;

    default:

        ASSERT(FALSE);

        break;
    }

    return;
}

PLOADED_IMAGE
ImpFindImageByLibraryName (
    PLIST_ENTRY ListHead,
    PCSTR Name
    )

/*++

Routine Description:

    This routine attempts to find an image with the given library name in the
    given list.

Arguments:

    ListHead - Supplies a pointer to the head of the list of images to
        search through.

    Name - Supplies a pointer to a string containing the name of the image.

Return Value:

    Returns a pointer to the image within the list on success.

    NULL on failure.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PLOADED_IMAGE Image;
    ULONG NameLength;
    PSTR Potential;

    NameLength = RtlStringLength(Name) + 1;
    CurrentEntry = ListHead->Next;
    while (CurrentEntry != ListHead) {
        Image = LIST_VALUE(CurrentEntry, LOADED_IMAGE, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        Potential = Image->LibraryName;
        if ((Potential != NULL) &&
            (RtlAreStringsEqual(Potential, Name, NameLength) != FALSE)) {

            //
            // This routine is used to load real images, so it would be bad to
            // return a placeholder image here.
            //

            ASSERT((Image->LoadFlags & IMAGE_LOAD_FLAG_PLACEHOLDER) == 0);

            //
            // Finding the image indicates that an image further along in the
            // list depends on said images. Move it to be back of the list.
            //

            LIST_REMOVE(&(Image->ListEntry));
            INSERT_BEFORE(&(Image->ListEntry), ListHead);
            return Image;
        }
    }

    return NULL;
}

PLOADED_IMAGE
ImpFindImageByFile (
    PLIST_ENTRY ListHead,
    PIMAGE_FILE_INFORMATION File
    )

/*++

Routine Description:

    This routine attempts to find an image matching the given file and device
    ID.

Arguments:

    ListHead - Supplies a pointer to the head of the list of images to
        search through.

    File - Supplies a pointer to the file information.

Return Value:

    Returns a pointer to the image within the list on success.

    NULL on failure.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PLOADED_IMAGE Image;

    //
    // If this image doesn't have the file/device ID supported, then don't
    // match anything.
    //

    if ((File->DeviceId == 0) && (File->FileId == 0)) {
        return NULL;
    }

    CurrentEntry = ListHead->Next;
    while (CurrentEntry != ListHead) {
        Image = LIST_VALUE(CurrentEntry, LOADED_IMAGE, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if ((Image->File.DeviceId == File->DeviceId) &&
            (Image->File.FileId == File->FileId)) {

            //
            // This routine is used to load real images, so it would be bad to
            // return a placeholder image here.
            //

            ASSERT((Image->LoadFlags & IMAGE_LOAD_FLAG_PLACEHOLDER) == 0);

            //
            // Finding the image indicates that an image further along in the
            // list depends on said images. Move it to be back of the list.
            //

            LIST_REMOVE(&(Image->ListEntry));
            INSERT_BEFORE(&(Image->ListEntry), ListHead);
            return Image;
        }
    }

    return NULL;
}

PLOADED_IMAGE
ImpAllocateImage (
    VOID
    )

/*++

Routine Description:

    This routine allocates a new loaded image structure, and initializes some
    basic fields and the name.

Arguments:

    None.

Return Value:

    Returns a pointer to the newly allocated image structure on success.

    NULL on failure.

--*/

{

    PLOADED_IMAGE Image;

    //
    // Allocate space for the loaded image structure.
    //

    Image = ImAllocateMemory(sizeof(LOADED_IMAGE), IM_ALLOCATION_TAG);
    if (Image == NULL) {
        return NULL;
    }

    RtlZeroMemory(Image, sizeof(LOADED_IMAGE));
    Image->ReferenceCount = 1;
    Image->AllocatorHandle = INVALID_HANDLE;
    Image->File.Handle = INVALID_HANDLE;
    Image->TlsOffset = -1;
    Image->Debug.Version = IMAGE_DEBUG_VERSION;
    Image->Debug.Image = Image;

    //
    // Consider consolidating ImNotifyImageLoad and ImNotifyImageUnload
    // so this mechanism works fully.
    //

    Image->Debug.ImageChangeFunction = ImNotifyImageLoad;
    return Image;
}

