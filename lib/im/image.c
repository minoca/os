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
#include "elf.h"

//
// --------------------------------------------------------------------- Macros
//

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

KSTATUS
ImpAppendToScope (
    PLOADED_IMAGE Image,
    PLOADED_IMAGE Element
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
// Store a pointer to the primary executable, the root of the global scope.
//

PLOADED_IMAGE ImPrimaryExecutable = NULL;

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

    return ImpLoadImports(ListHead);
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

    return ImpRelocateImages(ListHead);
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

    if (Image->Scope != NULL) {
        ImFreeMemory(Image->Scope);
    }

    ImFreeMemory(Image);
    return;
}

KSTATUS
ImGetSymbolByName (
    PLOADED_IMAGE Image,
    PSTR SymbolName,
    PLOADED_IMAGE Skip,
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

    Skip - Supplies an optional pointer to an image to skip when searching.

    Symbol - Supplies a pointer to a structure that receives the symbol's
        information on success.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    Status = ImpGetSymbolByName(Image, SymbolName, Skip, Symbol);
    return Status;
}

PLOADED_IMAGE
ImGetImageByAddress (
    PLIST_ENTRY ListHead,
    PVOID Address
    )

/*++

Routine Description:

    This routine attempts to find the image that covers the given address.

Arguments:

    ListHead - Supplies the list of loaded images.

    Address - Supplies the address to search for.

Return Value:

    Returns a pointer to an image covering the given address on success.

    NULL if no loaded image covers the given address.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PLOADED_IMAGE Image;
    PVOID Start;

    CurrentEntry = ListHead->Next;
    while (CurrentEntry != ListHead) {
        Image = LIST_VALUE(CurrentEntry, LOADED_IMAGE, ListEntry);
        Start = Image->PreferredLowestAddress + Image->BaseDifference;
        if ((Address >= Start) && (Address < Start + Image->Size)) {
            return Image;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    return NULL;
}

KSTATUS
ImGetSymbolByAddress (
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

    return ImpGetSymbolByAddress(Image, Address, Symbol);
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

    return ImpResolvePltEntry(Image, RelocationOffset);
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
    PLOADED_IMAGE OpenParent;
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

        Image = ImPrimaryExecutable;
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

        //
        // A dynamically loaded library typically doesn't have a parent, but
        // should be located as if the primary executable were its parent.
        //

        OpenParent = Parent;
        if ((Parent == NULL) &&
            ((Flags & IMAGE_LOAD_FLAG_DYNAMIC_LIBRARY) != 0)) {

            OpenParent = ImPrimaryExecutable;
        }

        Status = ImpOpenLibrary(ListHead,
                                OpenParent,
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

KSTATUS
ImpAddImageToScope (
    PLOADED_IMAGE Parent,
    PLOADED_IMAGE Child
    )

/*++

Routine Description:

    This routine appends a breadth first traversal of the child's dependencies
    to the image scope.

Arguments:

    Parent - Supplies a pointer to the innermost scope to add the child to.

    Child - Supplies a pointer to the child to add to the scope. This is often
        the parent itself.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES if there was an allocation failure.

--*/

{

    UINTN ImportCount;
    UINTN ImportIndex;
    UINTN Index;
    KSTATUS Status;

    //
    // Add the child itself.
    //

    Index = Parent->ScopeSize;
    Status = ImpAppendToScope(Parent, Child);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // Now process all the newly added images adding their dependencies until
    // there are none left.
    //

    while (Index < Parent->ScopeSize) {
        Child = Parent->Scope[Index];
        ImportCount = Child->ImportCount;
        for (ImportIndex = 0; ImportIndex < ImportCount; ImportIndex += 1) {
            Status = ImpAppendToScope(Parent, Child->Imports[ImportIndex]);
            if (!KSUCCESS(Status)) {
                return Status;
            }
        }

        Index += 1;
    }

    return STATUS_SUCCESS;
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

    Path - Supplies a pointer where the real path to the opened file will be
        returned. The caller is responsible for freeing this memory.

Return Value:

    Status code.

--*/

{

    ULONG NameLength;
    KSTATUS Status;

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

        ASSERT(Parent->SystemContext == SystemContext);

        Status = ImpOpenImport(ListHead, Parent, BinaryName, File, Path);
    }

    return Status;
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

KSTATUS
ImpAppendToScope (
    PLOADED_IMAGE Image,
    PLOADED_IMAGE Element
    )

/*++

Routine Description:

    This routine appends an image to the scope of the given image.

Arguments:

    Image - Supplies the image whose scope should be expanded.

    Element - Supplies the element to add to the scope.

Return Value:

    STATUS_SUCCESS if the elements were successfully appended.

    STATUS_INSUFFICIENT_RESORUCES on allocation failure.

--*/

{

    UINTN Index;
    UINTN NewCapacity;
    PLOADED_IMAGE *NewScope;
    UINTN Size;

    Size = Image->ScopeSize;

    //
    // First see if it's already there and do nothing if it is.
    //

    for (Index = 0; Index < Size; Index += 1) {
        if (Image->Scope[Index] == Element) {
            return STATUS_SUCCESS;
        }
    }

    if (Size >= IM_MAX_SCOPE_SIZE) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    if (Image->ScopeSize >= Image->ScopeCapacity) {
        NewCapacity = Image->ScopeCapacity * 2;
        if (Image->ScopeCapacity == 0) {
            NewCapacity = IM_INITIAL_SCOPE_SIZE;
        }

        NewScope = ImAllocateMemory(NewCapacity * sizeof(PLOADED_IMAGE),
                                    IM_ALLOCATION_TAG);

        if (NewScope == NULL) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        if (Size != 0) {
            RtlCopyMemory(NewScope, Image->Scope, Size * sizeof(PLOADED_IMAGE));
            ImFreeMemory(Image->Scope);
        }

        Image->Scope = NewScope;
        Image->ScopeCapacity = NewCapacity;
    }

    Image->Scope[Size] = Element;
    Image->ScopeSize += 1;
    return STATUS_SUCCESS;
}

