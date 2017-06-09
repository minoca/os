/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    imuniv.c

Abstract:

    This module implements the universal image library mux functions. These
    functions switch and call one of any of the supported image formats.

Author:

    Evan Green 26-May-2017

Environment:

    Any

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
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

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

        } else if (PeHeaders->FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64) {
            Information->Machine = ImageMachineTypeX64;

        } else {
            Information->Machine = ImageMachineTypeUnknown;
        }

        Information->EntryPoint = PeHeaders->OptionalHeader.AddressOfEntryPoint;
        Status = STATUS_SUCCESS;
        goto GetImageInformationEnd;
    }

    RtlZeroMemory(&Image, sizeof(LOADED_IMAGE));
    Status = ImpElf32GetImageSize(NULL, &Image, Buffer, NULL);
    if (!KSUCCESS(Status)) {
        Status = ImpElf64GetImageSize(NULL, &Image, Buffer, NULL);
    }

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

    case ImageElf64:
        return ImpElf64GetSection(Buffer,
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

    PELF32_HEADER Elf32Header;
    PELF64_HEADER Elf64Header;
    PIMAGE_NT_HEADERS PeHeaders;

    //
    // Attempt to get the ELF image header.
    //

    if (ImpElf32GetHeader(Buffer, &Elf32Header) != FALSE) {
        return ImageElf32;
    }

    if (ImpElf64GetHeader(Buffer, &Elf64Header) != FALSE) {
        return ImageElf64;
    }

    //
    // Attempt to get the PE image headers.
    //

    if (ImpPeGetHeaders(Buffer, &PeHeaders) != FALSE) {
        return ImagePe32;
    }

    //
    // Unknown image format.
    //

    return ImageUnknownFormat;
}

KSTATUS
ImpLoadImports (
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

    FirstImage = LIST_VALUE(ListHead->Next, LOADED_IMAGE, ListEntry);
    switch (FirstImage->Format) {
    case ImagePe32:
        Status = STATUS_SUCCESS;
        break;

    case ImageElf32:
        Status = ImpElf32LoadAllImports(ListHead);
        break;

    case ImageElf64:
        Status = ImpElf64LoadAllImports(ListHead);
        break;

    default:

        ASSERT(FALSE);

        Status = STATUS_FILE_CORRUPT;
        break;
    }

    return Status;
}

KSTATUS
ImpRelocateImages (
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

    FirstImage = LIST_VALUE(ListHead->Next, LOADED_IMAGE, ListEntry);
    switch (FirstImage->Format) {
    case ImagePe32:
        Status = STATUS_SUCCESS;
        break;

    case ImageElf32:
        Status = ImpElf32RelocateImages(ListHead);
        break;

    case ImageElf64:
        Status = ImpElf64RelocateImages(ListHead);
        break;

    default:

        ASSERT(FALSE);

        Status = STATUS_FILE_CORRUPT;
        break;
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

    case ImageElf64:
        Status = ImpElf64GetImageSize(ListHead, Image, Buffer, InterpreterPath);
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

    case ImageElf64:
        Status = ImpElf64LoadImage(ListHead, Image, Buffer);
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

    case ImageElf64:
        Status = ImpElf64AddImage(ImageBuffer, Image);
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

    switch (Parent->Format) {
    case ImageElf32:
        Status = ImpElf32OpenLibrary(ListHead, Parent, BinaryName, File, Path);
        break;

    case ImageElf64:
        Status = ImpElf64OpenLibrary(ListHead, Parent, BinaryName, File, Path);
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

    case ImageElf64:
        ImpElf64UnloadImage(Image);
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
    PLOADED_IMAGE Skip,
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

    Skip - Supplies an optional pointer to an image to skip when searching.

    Symbol - Supplies a pointer to a structure that receives the symbol's
        information on success.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    if (Image == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    switch (Image->Format) {
    case ImageElf32:
        Status = ImpElf32GetSymbolByName(Image, SymbolName, Skip, Symbol);
        break;

    case ImageElf64:
        Status = ImpElf64GetSymbolByName(Image, SymbolName, Skip, Symbol);
        break;

    default:
        Status = STATUS_UNKNOWN_IMAGE_FORMAT;
        break;
    }

    return Status;
}

KSTATUS
ImpGetSymbolByAddress (
    PLOADED_IMAGE Image,
    PVOID Address,
    PIMAGE_SYMBOL Symbol
    )

/*++

Routine Description:

    This routine attempts to resolve the given address into a symbol.

Arguments:

    Image - Supplies a pointer to the image to query.

    Address - Supplies the address to search for.

    Symbol - Supplies a pointer to a structure that receives the address's
        symbol information on success.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    if (Image == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    switch (Image->Format) {
    case ImageElf32:
        Status = ImpElf32GetSymbolByAddress(Image, Address, Symbol);
        break;

    case ImageElf64:
        Status = ImpElf64GetSymbolByAddress(Image, Address, Symbol);
        break;

    default:
        Status = STATUS_UNKNOWN_IMAGE_FORMAT;
        break;
    }

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

    case ImageElf64:
        ImpElf64RelocateSelf(Buffer, PltResolver, Image);
        break;

    default:

        ASSERT(FALSE);

        break;
    }

    return;
}

PVOID
ImpResolvePltEntry (
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
        FunctionAddress = ImpElf32ResolvePltEntry(Image, RelocationOffset);
        break;

    case ImageElf64:
        FunctionAddress = ImpElf64ResolvePltEntry(Image, RelocationOffset);
        break;

    default:

        ASSERT(FALSE);

        FunctionAddress = NULL;
        break;
    }

    return FunctionAddress;
}

//
// --------------------------------------------------------- Internal Functions
//

