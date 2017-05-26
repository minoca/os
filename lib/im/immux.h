/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    immux.h

Abstract:

    This header contains muxing definitions that either route the image
    library functions to one specific image format, or route them to functions
    that can switch to accomodate multiple image formats. Typically native
    image loaders only need support for one format, where debuggers want
    support for all formats.

Author:

    Evan Green 26-May-2017

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

#ifdef WANT_IM_NATIVE

#if defined(__i386) || defined(__arm__)

//
// Mux the functions directly to the 32-bit ELF functions.
//

#define ImpLoadImports ImpElf32LoadAllImports
#define ImpRelocateImages ImpElf32RelocateImages
#define ImpGetImageSize ImpElf32GetImageSize
#define ImpLoadImage ImpElf32LoadImage
#define ImpAddImage ImpElf32AddImage
#define ImpOpenImport ImpElf32OpenLibrary
#define ImpUnloadImage ImpElf32UnloadImage
#define ImpGetSymbolByName ImpElf32GetSymbolByName
#define ImpGetSymbolByAddress ImpElf32GetSymbolByAddress
#define ImpRelocateSelf ImpElf32RelocateSelf
#define ImpResolvePltEntry ImpElf32ResolvePltEntry
#define ImpGetSection ImpElf32GetSection
#define ImpGetHeader ImpElf32GetHeader

#define ImageNative ImageElf32

#elif defined(__amd64)

#define ImpLoadImports ImpElf64LoadAllImports
#define ImpRelocateImages ImpElf64RelocateImages
#define ImpGetImageSize ImpElf64GetImageSize
#define ImpLoadImage ImpElf64LoadImage
#define ImpAddImage ImpElf64AddImage
#define ImpOpenImport ImpElf64OpenLibrary
#define ImpUnloadImage ImpElf64UnloadImage
#define ImpGetSymbolByName ImpElf64GetSymbolByName
#define ImpGetSymbolByAddress ImpElf64GetSymbolByAddress
#define ImpRelocateSelf ImpElf64RelocateSelf
#define ImpResolvePltEntry ImpElf64ResolvePltEntry
#define ImpGetSection ImpElf64GetSection
#define ImpGetHeader ImpElf64GetHeader

#define ImageNative ImageElf64

//
// Mux the functions directly to the 64-bit ELF functions.
//

#else

#error Unknown architecture

#endif
#endif

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

//
// Define real function prototypes for the universal functions.
//

#ifndef WANT_IM_NATIVE

KSTATUS
ImpLoadImports (
    PLIST_ENTRY ListHead
    );

/*++

Routine Description:

    This routine loads all import libraries for a given image list.

Arguments:

    ListHead - Supplies a pointer to the head of the list of loaded images to
        load import libraries for.

Return Value:

    Status code.

--*/

KSTATUS
ImpRelocateImages (
    PLIST_ENTRY ListHead
    );

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

KSTATUS
ImpGetImageSize (
    PLIST_ENTRY ListHead,
    PLOADED_IMAGE Image,
    PIMAGE_BUFFER Buffer,
    PSTR *InterpreterPath
    );

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

KSTATUS
ImpLoadImage (
    PLIST_ENTRY ListHead,
    PLOADED_IMAGE Image,
    PIMAGE_BUFFER Buffer
    );

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

KSTATUS
ImpAddImage (
    PIMAGE_BUFFER ImageBuffer,
    PLOADED_IMAGE Image
    );

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

KSTATUS
ImpOpenImport (
    PLIST_ENTRY ListHead,
    PLOADED_IMAGE Parent,
    PCSTR BinaryName,
    PIMAGE_FILE_INFORMATION File,
    PSTR *Path
    );

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

VOID
ImpUnloadImage (
    PLOADED_IMAGE Image
    );

/*++

Routine Description:

    This routine unloads an executable image from virtual memory.

Arguments:

    Image - Supplies a pointer to the loaded image.

Return Value:

    None.

--*/

KSTATUS
ImpGetSymbolByName (
    PLOADED_IMAGE Image,
    PSTR SymbolName,
    PLOADED_IMAGE Skip,
    PIMAGE_SYMBOL Symbol
    );

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

KSTATUS
ImpGetSymbolByAddress (
    PLOADED_IMAGE Image,
    PVOID Address,
    PIMAGE_SYMBOL Symbol
    );

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

VOID
ImpRelocateSelf (
    PIMAGE_BUFFER Buffer,
    PIM_RESOLVE_PLT_ENTRY PltResolver,
    PLOADED_IMAGE Image
    );

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

PVOID
ImpResolvePltEntry (
    PLOADED_IMAGE Image,
    UINTN RelocationOffset
    );

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

#endif

