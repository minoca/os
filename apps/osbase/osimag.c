/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    osimag.c

Abstract:

    This module implements the underlying support routines for the image
    library to be run in user mode.

Author:

    Evan Green 17-Oct-2013

Environment:

    User

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "osbasep.h"

//
// ---------------------------------------------------------------- Definitions
//

#define OS_IMAGE_ALLOCATION_TAG 0x6D49734F // 'mIsO'

#define OS_IMAGE_LIST_SIZE_GUESS 512
#define OS_IMAGE_LIST_TRY_COUNT 10

#define OS_DYNAMIC_LOADER_USAGE                                                \
    "usage: libminocaos.so [options] [program [arguments]]\n"                  \
    "This can be run either indirectly as an interpreter, or it can load and " \
    "execute a command line directly.\n"

//
// Define the name of the environment variable to look at to determine whether
// to resolve all PLT symbols at load time or not.
//

#define LD_BIND_NOW "LD_BIND_NOW"

//
// ------------------------------------------------------ Data Type Definitions
//

typedef
VOID
(*PIMAGE_ENTRY_POINT) (
    PPROCESS_ENVIRONMENT Environment
    );

/*++

Routine Description:

    This routine implements the entry point for a loaded image.

Arguments:

    Environment - Supplies the process environment.

Return Value:

    None, the image does not return.

--*/

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
OspImArchResolvePltEntry (
    VOID
    );

PVOID
OspImAllocateMemory (
    ULONG Size,
    ULONG Tag
    );

VOID
OspImFreeMemory (
    PVOID Allocation
    );

KSTATUS
OspImOpenFile (
    PVOID SystemContext,
    PCSTR BinaryName,
    PIMAGE_FILE_INFORMATION File
    );

VOID
OspImCloseFile (
    PIMAGE_FILE_INFORMATION File
    );

KSTATUS
OspImLoadFile (
    PIMAGE_FILE_INFORMATION File,
    PIMAGE_BUFFER Buffer
    );

VOID
OspImUnloadBuffer (
    PIMAGE_FILE_INFORMATION File,
    PIMAGE_BUFFER Buffer
    );

KSTATUS
OspImAllocateAddressSpace (
    PLOADED_IMAGE Image
    );

VOID
OspImFreeAddressSpace (
    PLOADED_IMAGE Image
    );

KSTATUS
OspImMapImageSegment (
    HANDLE AddressSpaceHandle,
    PVOID AddressSpaceAllocation,
    PIMAGE_FILE_INFORMATION File,
    ULONGLONG FileOffset,
    PIMAGE_SEGMENT Segment,
    PIMAGE_SEGMENT PreviousSegment
    );

VOID
OspImUnmapImageSegment (
    HANDLE AddressSpaceHandle,
    PIMAGE_SEGMENT Segment
    );

KSTATUS
OspImNotifyImageLoad (
    PLOADED_IMAGE Image
    );

VOID
OspImNotifyImageUnload (
    PLOADED_IMAGE Image
    );

VOID
OspImInvalidateInstructionCacheRegion (
    PVOID Address,
    ULONG Size
    );

PSTR
OspImGetEnvironmentVariable (
    PSTR Variable
    );

KSTATUS
OspImFinalizeSegments (
    HANDLE AddressSpaceHandle,
    PIMAGE_SEGMENT Segments,
    UINTN SegmentCount
    );

VOID
OspImInitializeImages (
    PLIST_ENTRY ListHead
    );

VOID
OspImInitializeImage (
    PLOADED_IMAGE Image
    );

PVOID
OspImResolvePltEntry (
    PLOADED_IMAGE Image,
    UINTN RelocationOffset
    );

KSTATUS
OspLoadInitialImageList (
    BOOL Relocate
    );

KSTATUS
OspImAssignModuleNumber (
    PLOADED_IMAGE Image
    );

VOID
OspImReleaseModuleNumber (
    PLOADED_IMAGE Image
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the routine used to get environment variable contents.
//

OS_API PIM_GET_ENVIRONMENT_VARIABLE OsImGetEnvironmentVariable;

//
// Store a pointer to the list head of all loaded images.
//

LIST_ENTRY OsLoadedImagesHead;
OS_RWLOCK OsLoadedImagesLock;

//
// Define the image library function table.
//

IM_IMPORT_TABLE OsImageFunctionTable = {
    OspImAllocateMemory,
    OspImFreeMemory,
    OspImOpenFile,
    OspImCloseFile,
    OspImLoadFile,
    NULL,
    OspImUnloadBuffer,
    OspImAllocateAddressSpace,
    OspImFreeAddressSpace,
    OspImMapImageSegment,
    OspImUnmapImageSegment,
    OspImNotifyImageLoad,
    OspImNotifyImageUnload,
    OspImInvalidateInstructionCacheRegion,
    OspImGetEnvironmentVariable,
    OspImFinalizeSegments,
    OspImArchResolvePltEntry
};

//
// Store the overridden library path specified by the command arguments to the
// dynamic linker.
//

PSTR OsImLibraryPathOverride;

//
// Store the bitmap for the image module numbers. Index zero is never valid.
//

UINTN OsImStaticModuleNumberBitmap = 0x1;
PUINTN OsImModuleNumberBitmap = &OsImStaticModuleNumberBitmap;
UINTN OsImModuleNumberBitmapSize = 1;

//
// Store the module generation number, which increments whenever a module is
// loaded or unloaded. It is protected under the image list lock.
//

UINTN OsImModuleGeneration;

//
// Store a boolean indicating whether or not the initial image is loaded.
//

BOOL OsImExecutableLoaded = TRUE;

//
// ------------------------------------------------------------------ Functions
//

OS_API
VOID
OsDynamicLoaderMain (
    PPROCESS_ENVIRONMENT Environment
    )

/*++

Routine Description:

    This routine implements the main routine for the Minoca OS loader when
    invoked directly (either as a standalone application or an interpreter).

Arguments:

    Environment - Supplies the process environment.

Return Value:

    None. This routine exits directly and never returns.

--*/

{

    PSTR Argument;
    UINTN ArgumentIndex;
    PLIST_ENTRY CurrentEntry;
    PLOADED_IMAGE CurrentImage;
    PLOADED_IMAGE Image;
    ULONG LoadFlags;
    PIMAGE_ENTRY_POINT Start;
    KSTATUS Status;
    PTHREAD_CONTROL_BLOCK Thread;

    //
    // Start by relocating this image. Until this is done, no global variables
    // can be touched.
    //

    ImRelocateSelf(Environment->StartData->OsLibraryBase,
                   OspImArchResolvePltEntry);

    OsInitializeLibrary(Environment);
    OsImExecutableLoaded = FALSE;
    Status = OspLoadInitialImageList(TRUE);
    if (!KSUCCESS(Status)) {
        RtlDebugPrint("Failed to populate initial image list: %d.\n", Status);
        goto DynamicLoaderMainEnd;
    }

    //
    // If the executable is this library, then the dynamic loader is being
    // invoked directly.
    //

    if (Environment->StartData->ExecutableBase ==
        Environment->StartData->OsLibraryBase) {

        LoadFlags = IMAGE_LOAD_FLAG_IGNORE_INTERPRETER |
                    IMAGE_LOAD_FLAG_PRIMARY_LOAD |
                    IMAGE_LOAD_FLAG_NO_RELOCATIONS |
                    IMAGE_LOAD_FLAG_GLOBAL;

        if (OspImGetEnvironmentVariable(LD_BIND_NOW) != NULL) {
            LoadFlags |= IMAGE_LOAD_FLAG_BIND_NOW;
        }

        ArgumentIndex = 1;
        while (ArgumentIndex < Environment->ArgumentCount) {
            Argument = Environment->Arguments[ArgumentIndex];
            if (RtlAreStringsEqual(Argument, "--library-path", -1) != FALSE) {
                ArgumentIndex += 1;
                if (ArgumentIndex == Environment->ArgumentCount) {
                    RtlDebugPrint("--library-path Argument missing.\n");
                    Status = STATUS_INVALID_PARAMETER;
                    goto DynamicLoaderMainEnd;
                }

                OsImLibraryPathOverride = Environment->Arguments[ArgumentIndex];
                ArgumentIndex += 1;

            } else {
                break;
            }
        }

        if (ArgumentIndex >= Environment->ArgumentCount) {
            RtlDebugPrint(OS_DYNAMIC_LOADER_USAGE);
            Status = STATUS_UNSUCCESSFUL;
            goto DynamicLoaderMainEnd;
        }

        //
        // Munge the environment to make it look like the program was
        // invoked directly.
        //

        Environment->Arguments = &(Environment->Arguments[ArgumentIndex]);
        Environment->ArgumentCount -= ArgumentIndex;
        Environment->ImageName = Environment->Arguments[0];
        Environment->ImageNameLength =
                                RtlStringLength(Environment->Arguments[0]) + 1;

        Status = ImLoad(&OsLoadedImagesHead,
                        Environment->ImageName,
                        NULL,
                        NULL,
                        NULL,
                        LoadFlags,
                        &Image,
                        NULL);
    }

    if (!KSUCCESS(Status)) {
        RtlDebugPrint("Failed to load %s: %d\n",
                      Environment->ImageName,
                      Status);

        goto DynamicLoaderMainEnd;
    }

    //
    // Assign module numbers to any modules that do not have them yet. This is
    // done after the executable is loaded so the executable gets the first
    // slot.
    //

    CurrentEntry = OsLoadedImagesHead.Next;
    while (CurrentEntry != &OsLoadedImagesHead) {
        CurrentImage = LIST_VALUE(CurrentEntry, LOADED_IMAGE, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if (CurrentImage->ModuleNumber == 0) {
            OspImAssignModuleNumber(CurrentImage);
        }
    }

    if (Image == NULL) {
        Image = ImPrimaryExecutable;
    }

    OsImExecutableLoaded = TRUE;

    //
    // Initialize TLS support.
    //

    OspTlsAllocate(&OsLoadedImagesHead, (PVOID *)&Thread, FALSE);
    OsSetThreadPointer(Thread);

    //
    // Now that TLS offsets are settled, relocate the images.
    //

    Status = ImRelocateImages(&OsLoadedImagesHead);
    if (!KSUCCESS(Status)) {
        RtlDebugPrint("Failed to relocate: %d\n", Status);
        goto DynamicLoaderMainEnd;
    }

    //
    // Call static constructors, without acquiring and releasing the lock
    // constantly.
    //

    CurrentEntry = OsLoadedImagesHead.Previous;
    while (CurrentEntry != &OsLoadedImagesHead) {
        CurrentImage = LIST_VALUE(CurrentEntry, LOADED_IMAGE, ListEntry);

        ASSERT((CurrentImage->Flags & IMAGE_FLAG_INITIALIZED) == 0);

        //
        // Copy in the TLS image if there is one.
        //

        if (CurrentImage->TlsImageSize != 0) {
            RtlCopyMemory(Thread->TlsVector[CurrentImage->ModuleNumber],
                          CurrentImage->TlsImage,
                          CurrentImage->TlsImageSize);
        }

        OspImInitializeImage(CurrentImage);
        CurrentImage->Flags |= IMAGE_FLAG_INITIALIZED;
        CurrentEntry = CurrentEntry->Previous;
    }

    //
    // Jump off to the image entry point.
    //

    Start = Image->EntryPoint;
    Start(Environment);
    RtlDebugPrint("Warning: Image returned to interpreter!\n");
    Status = STATUS_UNSUCCESSFUL;

DynamicLoaderMainEnd:
    OsExitProcess(Status);
    return;
}

OS_API
KSTATUS
OsLoadLibrary (
    PSTR LibraryName,
    ULONG Flags,
    PHANDLE Handle
    )

/*++

Routine Description:

    This routine loads a dynamic library.

Arguments:

    LibraryName - Supplies a pointer to the library name to load.

    Flags - Supplies a bitfield of flags associated with the request. See
        IMAGE_LOAD_FLAG_* definitions.

    Handle - Supplies a pointer where a handle to the dynamic library will be
        returned on success. INVALID_HANDLE will be returned on failure.

Return Value:

    Status code.

--*/

{

    PLOADED_IMAGE LoadedImage;
    KSTATUS Status;

    //
    // Always look through the primary executable's library paths.
    //

    Flags |= IMAGE_LOAD_FLAG_DYNAMIC_LIBRARY;

    //
    // Prime the get environment variable function to ensure it does not
    // have to resolve a PLT entry (and reacquire the lock) during load.
    //

    OspImGetEnvironmentVariable(LD_BIND_NOW);
    OspAcquireImageLock(TRUE);
    if (OsLoadedImagesHead.Next == NULL) {
        Status = OspLoadInitialImageList(FALSE);
        if (!KSUCCESS(Status)) {
            OspReleaseImageLock();
            goto LoadLibraryEnd;
        }
    }

    *Handle = INVALID_HANDLE;
    LoadedImage = NULL;
    Status = ImLoad(&OsLoadedImagesHead,
                    LibraryName,
                    NULL,
                    NULL,
                    NULL,
                    Flags,
                    &LoadedImage,
                    NULL);

    OspReleaseImageLock();
    if (!KSUCCESS(Status)) {
        goto LoadLibraryEnd;
    }

    OspImInitializeImages(&OsLoadedImagesHead);
    *Handle = LoadedImage;

LoadLibraryEnd:
    return Status;
}

OS_API
VOID
OsFreeLibrary (
    HANDLE Library
    )

/*++

Routine Description:

    This routine indicates a release of the resources associated with a
    previously loaded library. This may or may not actually unload the library
    depending on whether or not there are other references to it.

Arguments:

    Library - Supplies the library to release.

Return Value:

    None.

--*/

{

    if (Library == INVALID_HANDLE) {
        return;
    }

    OspAcquireImageLock(TRUE);
    ImImageReleaseReference(Library);
    OspReleaseImageLock();
    return;
}

OS_API
KSTATUS
OsGetSymbolAddress (
    HANDLE Library,
    PSTR SymbolName,
    HANDLE Skip,
    PVOID *Address
    )

/*++

Routine Description:

    This routine returns the address of the given symbol in the given image.
    Both the image and all of its imports will be searched.

Arguments:

    Library - Supplies the image to look up. Supply NULL to search the global
        scope.

    SymbolName - Supplies a pointer to a null terminated string containing the
        name of the symbol to look up.

    Skip - Supplies an optional pointer to a library to skip. Supply NULL or
        INVALID_HANDLE here to not skip any libraries.

    Address - Supplies a pointer that on success receives the address of the
        symbol, or NULL on failure.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_HANDLE if the library handle is not valid.

    STATUS_NOT_FOUND if the symbol could not be found.

--*/

{

    PLOADED_IMAGE Image;
    KSTATUS Status;
    IMAGE_SYMBOL Symbol;
    TLS_INDEX TlsIndex;

    *Address = NULL;
    Symbol.Image = INVALID_HANDLE;
    OspAcquireImageLock(FALSE);
    if (OsLoadedImagesHead.Next == NULL) {
        Status = OspLoadInitialImageList(FALSE);
        if (!KSUCCESS(Status)) {
            goto GetLibrarySymbolAddressEnd;
        }
    }

    if (Library == NULL) {
        Library = ImPrimaryExecutable;
    }

    if (Skip == INVALID_HANDLE) {
        Skip = NULL;
    }

    Status = ImGetSymbolByName(Library, SymbolName, Skip, &Symbol);
    if (KSUCCESS(Status)) {
        if (Symbol.TlsAddress != FALSE) {
            Image = Symbol.Image;
            if (Image == INVALID_HANDLE) {
                Status = STATUS_INVALID_HANDLE;
                goto GetLibrarySymbolAddressEnd;
            }

            TlsIndex.Module = Image->ModuleNumber;
            TlsIndex.Offset = (UINTN)Symbol.Address;
            Symbol.Address = OsGetTlsAddress(&TlsIndex);
        }

        *Address = Symbol.Address;
    }

GetLibrarySymbolAddressEnd:
    OspReleaseImageLock();
    return Status;
}

OS_API
KSTATUS
OsGetImageSymbolForAddress (
    PVOID Address,
    POS_IMAGE_SYMBOL Symbol
    )

/*++

Routine Description:

    This routine resolves the given address into an image and closest symbol
    whose address is less than or equal to the given address.

Arguments:

    Address - Supplies the address to look up.

    Symbol - Supplies a pointer to a structure that receives the resolved
        symbol information.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_HANDLE if the library handle is not valid.

    STATUS_NOT_FOUND if the address could not be found.

--*/

{

    PLOADED_IMAGE Image;
    IMAGE_SYMBOL ImageSymbol;
    KSTATUS Status;

    RtlZeroMemory(Symbol, sizeof(OS_IMAGE_SYMBOL));
    RtlZeroMemory(&ImageSymbol, sizeof(IMAGE_SYMBOL));
    ImageSymbol.Image = INVALID_HANDLE;
    OspAcquireImageLock(FALSE);
    if (OsLoadedImagesHead.Next == NULL) {
        Status = OspLoadInitialImageList(FALSE);
        if (!KSUCCESS(Status)) {
            goto GetLibrarySymbolForAddressEnd;
        }
    }

    Image = ImGetImageByAddress(&OsLoadedImagesHead, Address);
    if (Image == NULL) {
        Status = STATUS_NOT_FOUND;
        goto GetLibrarySymbolForAddressEnd;
    }

    Status = ImGetSymbolByAddress(Image, Address, &ImageSymbol);
    if (KSUCCESS(Status)) {

        //
        // If the image has no name and it's the primary executable, then fill
        // in the name from the OS environment.
        //

        Image = ImageSymbol.Image;
        Symbol->ImagePath = Image->FileName;
        if ((Symbol->ImagePath == NULL) &&
            ((Image->LoadFlags & IMAGE_LOAD_FLAG_PRIMARY_EXECUTABLE) != 0)) {

            Symbol->ImagePath = OsEnvironment->ImageName;
        }

        Symbol->ImageBase = Image->LoadedImageBuffer;
        Symbol->SymbolName = ImageSymbol.Name;
        Symbol->SymbolAddress = ImageSymbol.Address;
    }

GetLibrarySymbolForAddressEnd:
    OspReleaseImageLock();
    return Status;
}

OS_API
HANDLE
OsGetImageForAddress (
    PVOID Address
    )

/*++

Routine Description:

    This routine returns a handle to the image that contains the given address.

Arguments:

    Address - Supplies the address to look up.

Return Value:

    INVALID_HANDLE if no image contains the given address.

    On success, returns the dynamic image handle that contains the given
    address.

--*/

{

    PLOADED_IMAGE Image;
    KSTATUS Status;

    Image = NULL;
    OspAcquireImageLock(FALSE);
    if (OsLoadedImagesHead.Next == NULL) {
        Status = OspLoadInitialImageList(FALSE);
        if (!KSUCCESS(Status)) {
            goto GetImageForAddressEnd;
        }
    }

    Image = ImGetImageByAddress(&OsLoadedImagesHead, Address);

GetImageForAddressEnd:
    OspReleaseImageLock();
    if (Image == NULL) {
        return INVALID_HANDLE;
    }

    return (HANDLE)Image;
}

OS_API
KSTATUS
OsFlushCache (
    PVOID Address,
    UINTN Size
    )

/*++

Routine Description:

    This routine flushes the caches for a region of memory after executable
    code has been modified.

Arguments:

    Address - Supplies the address of the region to flush.

    Size - Supplies the number of bytes in the region.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_ACCESS_VIOLATION if the given address was not valid.

--*/

{

    SYSTEM_CALL_FLUSH_CACHE Parameters;

    Parameters.Address = Address;
    Parameters.Size = Size;
    return OsSystemCall(SystemCallFlushCache, &Parameters);
}

OS_API
KSTATUS
OsCreateThreadData (
    PVOID *ThreadData
    )

/*++

Routine Description:

    This routine creates the OS library data necessary to manage a new thread.
    This function is usually called by the C library.

Arguments:

    ThreadData - Supplies a pointer where a pointer to the thread data will be
        returned on success. It is the callers responsibility to destroy this
        thread data. The contents of this data are opaque and should not be
        interpreted. The caller should set this returned pointer as the
        thread pointer.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    //
    // Allocate the initial TLS image and control block for the thread.
    //

    OspAcquireImageLock(FALSE);
    Status = OspTlsAllocate(&OsLoadedImagesHead, ThreadData, TRUE);
    OspReleaseImageLock();
    return Status;
}

OS_API
VOID
OsDestroyThreadData (
    PVOID ThreadData
    )

/*++

Routine Description:

    This routine destroys the previously created OS library thread data.

Arguments:

    ThreadData - Supplies the previously returned thread data.

Return Value:

    Status code.

--*/

{

    OspAcquireImageLock(FALSE);
    OspTlsDestroy(ThreadData);
    OspReleaseImageLock();
    return;
}

OS_API
VOID
OsIterateImages (
    PIMAGE_ITERATOR_ROUTINE IteratorRoutine,
    PVOID Context
    )

/*++

Routine Description:

    This routine iterates over all images currently loaded in the process.

Arguments:

    IteratorRoutine - Supplies a pointer to the routine to call for each image.

    Context - Supplies an opaque context pointer that is passed directly into
        the iterator routine.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PLOADED_IMAGE Image;

    OspAcquireImageLock(FALSE);
    CurrentEntry = OsLoadedImagesHead.Next;
    if (CurrentEntry != NULL) {
        while (CurrentEntry != &OsLoadedImagesHead) {
            Image = LIST_VALUE(CurrentEntry, LOADED_IMAGE, ListEntry);
            CurrentEntry = CurrentEntry->Next;
            IteratorRoutine(Image, Context);
        }
    }

    OspReleaseImageLock();
    return;
}

VOID
OspInitializeImageSupport (
    VOID
    )

/*++

Routine Description:

    This routine initializes the image library for use in the image creation
    tool.

Arguments:

    None.

Return Value:

    None.

--*/

{

    OsRwLockInitialize(&OsLoadedImagesLock, 0);
    ImInitialize(&OsImageFunctionTable);
    return;
}

VOID
OspAcquireImageLock (
    BOOL Exclusive
    )

/*++

Routine Description:

    This routine acquires the global image lock.

Arguments:

    Exclusive - Supplies a boolean indicating whether the lock should be
        held shared (FALSE) or exclusive (TRUE).

Return Value:

    None.

--*/

{

    if (Exclusive != FALSE) {
        OsRwLockWrite(&OsLoadedImagesLock);

    } else {
        OsRwLockRead(&OsLoadedImagesLock);
    }

    return;
}

VOID
OspReleaseImageLock (
    VOID
    )

/*++

Routine Description:

    This routine releases the global image lock.

Arguments:

    None.

Return Value:

    None.

--*/

{

    OsRwLockUnlock(&OsLoadedImagesLock);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

PVOID
OspImAllocateMemory (
    ULONG Size,
    ULONG Tag
    )

/*++

Routine Description:

    This routine allocates memory for the image library.

Arguments:

    Size - Supplies the number of bytes required for the memory allocation.

    Tag - Supplies a 32-bit ASCII identifier used to tag the memroy allocation.

Return Value:

    Returns a pointer to the memory allocation on success.

    NULL on failure.

--*/

{

    return OsHeapAllocate(Size, Tag);
}

VOID
OspImFreeMemory (
    PVOID Allocation
    )

/*++

Routine Description:

    This routine frees memory allocated by the image library.

Arguments:

    Allocation - Supplies a pointer the allocation to free.

Return Value:

    None.

--*/

{

    OsHeapFree(Allocation);
    return;
}

KSTATUS
OspImOpenFile (
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

    ULONG BinaryNameSize;
    FILE_CONTROL_PARAMETERS_UNION FileControlParameters;
    FILE_PROPERTIES FileProperties;
    KSTATUS Status;

    File->Handle = INVALID_HANDLE;
    BinaryNameSize = RtlStringLength(BinaryName) + 1;
    Status = OsOpen(INVALID_HANDLE,
                    BinaryName,
                    BinaryNameSize,
                    SYS_OPEN_FLAG_READ,
                    FILE_PERMISSION_NONE,
                    &(File->Handle));

    if (!KSUCCESS(Status)) {
        goto OpenFileEnd;
    }

    FileControlParameters.SetFileInformation.FieldsToSet = 0;
    FileControlParameters.SetFileInformation.FileProperties = &FileProperties;
    Status = OsFileControl(File->Handle,
                           FileControlCommandGetFileInformation,
                           &FileControlParameters);

    if (!KSUCCESS(Status)) {
        goto OpenFileEnd;
    }

    if (FileProperties.Type != IoObjectRegularFile) {
        Status = STATUS_UNEXPECTED_TYPE;
        goto OpenFileEnd;
    }

    File->Size = FileProperties.Size;
    File->ModificationDate = FileProperties.ModifiedTime.Seconds;
    File->DeviceId = FileProperties.DeviceId;
    File->FileId = FileProperties.FileId;
    Status = STATUS_SUCCESS;

OpenFileEnd:
    if (!KSUCCESS(Status)) {
        if (File->Handle != INVALID_HANDLE) {
            OsClose(File->Handle);
            File->Handle = INVALID_HANDLE;
        }
    }

    return Status;
}

VOID
OspImCloseFile (
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

    KSTATUS Status;

    if (File->Handle != INVALID_HANDLE) {
        Status = OsClose(File->Handle);

        ASSERT(KSUCCESS(Status));

        File->Handle = INVALID_HANDLE;
    }

    return;
}

KSTATUS
OspImLoadFile (
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

    ULONGLONG AlignedSize;
    KSTATUS Status;

    AlignedSize = ALIGN_RANGE_UP(File->Size, OsPageSize);
    if (AlignedSize > MAX_UINTN) {
        return STATUS_NOT_SUPPORTED;
    }

    Status = OsMemoryMap(File->Handle,
                         0,
                         (UINTN)AlignedSize,
                         SYS_MAP_FLAG_READ,
                         &(Buffer->Data));

    if (!KSUCCESS(Status)) {
        return Status;
    }

    Buffer->Size = File->Size;
    return Status;
}

VOID
OspImUnloadBuffer (
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

    UINTN AlignedSize;
    KSTATUS Status;

    ASSERT(Buffer->Data != NULL);

    AlignedSize = ALIGN_RANGE_UP(File->Size, OsPageSize);
    Status = OsMemoryUnmap(Buffer->Data, AlignedSize);

    ASSERT(KSUCCESS(Status));

    Buffer->Data = NULL;
    return;
}

KSTATUS
OspImAllocateAddressSpace (
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

    PVOID Address;
    UINTN AlignedSize;
    ULONG MapFlags;
    KSTATUS Status;

    //
    // Memory map a region to use.
    //

    Address = Image->PreferredLowestAddress;
    AlignedSize = ALIGN_RANGE_UP(Image->Size, OsPageSize);
    MapFlags = SYS_MAP_FLAG_READ | SYS_MAP_FLAG_WRITE | SYS_MAP_FLAG_EXECUTE;
    Status = OsMemoryMap(Image->File.Handle,
                         0,
                         AlignedSize,
                         MapFlags,
                         &Address);

    Image->BaseDifference = Address - Image->PreferredLowestAddress;
    Image->LoadedImageBuffer = Address;
    Image->AllocatorHandle = Address;
    return Status;
}

VOID
OspImFreeAddressSpace (
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

    KSTATUS Status;

    Status = OsMemoryUnmap(Image->LoadedImageBuffer, Image->Size);

    ASSERT(KSUCCESS(Status));

    return;
}

KSTATUS
OspImMapImageSegment (
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

    PVOID Address;
    UINTN BytesCompleted;
    HANDLE FileHandle;
    PVOID FileRegion;
    UINTN FileRegionSize;
    UINTN FileSize;
    UINTN IoSize;
    ULONG MapFlags;
    UINTN MemoryRegionSize;
    UINTN MemorySize;
    UINTN NextPage;
    UINTN PageMask;
    UINTN PageOffset;
    UINTN PageSize;
    UINTN PreviousEnd;
    UINTN RegionEnd;
    UINTN RegionSize;
    UINTN SegmentAddress;
    KSTATUS Status;

    ASSERT((PreviousSegment == NULL) ||
           (Segment->VirtualAddress > PreviousSegment->VirtualAddress));

    FileRegion = NULL;
    FileRegionSize = 0;
    FileHandle = INVALID_HANDLE;
    if (File != NULL) {
        FileHandle = File->Handle;
    }

    FileSize = Segment->FileSize;
    MemorySize = Segment->MemorySize;

    ASSERT((FileSize == Segment->FileSize) &&
           (MemorySize == Segment->MemorySize));

    //
    // Map everything readable and writable for now, it will get fixed up
    // during finalization.
    //

    MapFlags = SYS_MAP_FLAG_READ | SYS_MAP_FLAG_WRITE;
    if ((Segment->Flags & IMAGE_MAP_FLAG_EXECUTE) != 0) {
        MapFlags |= SYS_MAP_FLAG_EXECUTE;
    }

    if ((Segment->Flags & IMAGE_MAP_FLAG_FIXED) != 0) {
        MapFlags |= SYS_MAP_FLAG_FIXED;
    }

    //
    // Handle the first part, which may overlap with the previous segment.
    //

    PageSize = OsPageSize;
    PageMask = PageSize - 1;
    SegmentAddress = (UINTN)(Segment->VirtualAddress);
    if (PreviousSegment != NULL) {
        PreviousEnd = (UINTN)(PreviousSegment->VirtualAddress) +
                      PreviousSegment->MemorySize;

        RegionEnd = ALIGN_RANGE_UP(PreviousEnd, PageSize);
        if (RegionEnd > SegmentAddress) {

            //
            // Compute the portion of this section that needs to be read or
            // zeroed into it.
            //

            if (SegmentAddress + MemorySize < RegionEnd) {
                RegionEnd = SegmentAddress + MemorySize;
            }

            RegionSize = RegionEnd - SegmentAddress;
            IoSize = FileSize;
            if (IoSize > RegionSize) {
                IoSize = RegionSize;
            }

            Status = OsPerformIo(FileHandle,
                                 FileOffset,
                                 IoSize,
                                 0,
                                 SYS_WAIT_TIME_INDEFINITE,
                                 (PVOID)SegmentAddress,
                                 &BytesCompleted);

            if (!KSUCCESS(Status)) {
                goto MapImageSegmentEnd;
            }

            if (BytesCompleted != IoSize) {
                Status = STATUS_END_OF_FILE;
                goto MapImageSegmentEnd;
            }

            if (IoSize < RegionSize) {
                RtlZeroMemory((PVOID)SegmentAddress + IoSize,
                              RegionSize - IoSize);
            }

            if (((Segment->Flags | PreviousSegment->Flags) &
                 IMAGE_MAP_FLAG_EXECUTE) != 0) {

                Status = OsFlushCache((PVOID)SegmentAddress, RegionSize);

                ASSERT(KSUCCESS(Status));
            }

            FileOffset += IoSize;
            FileSize -= IoSize;
            MemorySize -= RegionSize;
            SegmentAddress = RegionEnd;

        //
        // If there is a hole in between the previous segment and this one,
        // change the protection to none for the hole.
        //

        } else {
            RegionSize = SegmentAddress - RegionEnd;
            RegionSize = ALIGN_RANGE_DOWN(RegionSize, PageSize);
            if (RegionSize != 0) {
                Status = OsSetMemoryProtection((PVOID)RegionEnd, RegionSize, 0);
                if (!KSUCCESS(Status)) {

                    ASSERT(FALSE);

                    goto MapImageSegmentEnd;
                }
            }
        }
    }

    //
    // This is the main portion. If the file offset and address have the same
    // page alignment, then it can be mapped directly. Otherwise, it must be
    // read in.
    //

    if (FileSize != 0) {
        PageOffset = FileOffset & PageMask;
        FileRegion = (PVOID)(SegmentAddress - PageOffset);
        FileRegionSize = ALIGN_RANGE_UP(FileSize + PageOffset, PageSize);

        //
        // Try to memory map the file directly.
        //

        if (PageOffset == (SegmentAddress & PageMask)) {

            //
            // Memory map the file to the desired address. The address space
            // allocation was created by memory mapping the beginning of the
            // file, so skip the mapping if it's trying to do exactly that.
            // This saves a redundant system call.
            //

            if ((FileOffset != PageOffset) ||
                (FileRegion != AddressSpaceAllocation)) {

                Status = OsMemoryMap(FileHandle,
                                     FileOffset - PageOffset,
                                     FileRegionSize,
                                     MapFlags,
                                     &FileRegion);

                if (!KSUCCESS(Status)) {
                    RtlDebugPrint("Failed to map 0x%x bytes at 0x%x: %d\n",
                                  FileRegionSize,
                                  FileRegion,
                                  Status);

                    FileRegionSize = 0;
                    goto MapImageSegmentEnd;
                }
            }

            IoSize = 0;

        //
        // The file offsets don't agree. Allocate a region for reading.
        //

        } else {
            Status = OsMemoryMap(INVALID_HANDLE,
                                 0,
                                 FileRegionSize,
                                 MapFlags | SYS_MAP_FLAG_ANONYMOUS,
                                 &FileRegion);

            if (!KSUCCESS(Status)) {
                RtlDebugPrint("Failed to map 0x%x bytes at 0x%x: %d\n",
                              FileRegionSize,
                              FileRegion,
                              Status);

                FileRegionSize = 0;
                goto MapImageSegmentEnd;
            }

            IoSize = FileSize;
        }

        //
        // If the mapping wasn't at the expected location, adjust.
        //

        if ((UINTN)FileRegion != SegmentAddress - PageOffset) {

            ASSERT((PreviousSegment == NULL) &&
                   ((Segment->Flags & IMAGE_MAP_FLAG_FIXED) == 0));

            SegmentAddress = (UINTN)FileRegion + PageOffset;
            Segment->VirtualAddress = (PVOID)SegmentAddress;
        }

        Segment->MappingStart = FileRegion;

        //
        // Read from the file if the file wasn't mapped directly.
        //

        if (IoSize != 0) {
            Status = OsPerformIo(FileHandle,
                                 FileOffset,
                                 IoSize,
                                 0,
                                 SYS_WAIT_TIME_INDEFINITE,
                                 (PVOID)SegmentAddress,
                                 &BytesCompleted);

            if (!KSUCCESS(Status)) {
                goto MapImageSegmentEnd;
            }

            if (BytesCompleted != IoSize) {
                Status = STATUS_END_OF_FILE;
                goto MapImageSegmentEnd;
            }

            if ((Segment->Flags & IMAGE_MAP_FLAG_EXECUTE) != 0) {
                Status = OsFlushCache((PVOID)SegmentAddress, IoSize);

                ASSERT(KSUCCESS(Status));
            }
        }

        SegmentAddress += FileSize;
        MemorySize -= FileSize;

        //
        // Zero out any region between the end of the file portion and the next
        // page.
        //

        NextPage = ALIGN_RANGE_UP(SegmentAddress, PageSize);
        if (NextPage - SegmentAddress != 0) {
            RtlZeroMemory((PVOID)SegmentAddress, NextPage - SegmentAddress);
            if ((Segment->Flags & IMAGE_MAP_FLAG_EXECUTE) != 0) {
                Status = OsFlushCache((PVOID)SegmentAddress,
                                      NextPage - SegmentAddress);

                ASSERT(KSUCCESS(Status));
            }
        }

        if (NextPage >= SegmentAddress + MemorySize) {
            Status = STATUS_SUCCESS;
            goto MapImageSegmentEnd;
        }

        MemorySize -= NextPage - SegmentAddress;
        SegmentAddress = NextPage;

        //
        // If the file region was decided, any remaining memory region is now
        // fixed.
        //

        MapFlags |= SYS_MAP_FLAG_FIXED;
    }

    //
    // Memory map the remaining region.
    //

    PageOffset = SegmentAddress & PageMask;
    Address = (PVOID)(SegmentAddress - PageOffset);
    MemoryRegionSize = MemorySize + PageOffset;
    MemoryRegionSize = ALIGN_RANGE_UP(MemoryRegionSize, PageSize);
    Status = OsMemoryMap(INVALID_HANDLE,
                         0,
                         MemoryRegionSize,
                         MapFlags | SYS_MAP_FLAG_ANONYMOUS,
                         &Address);

    if (!KSUCCESS(Status)) {
        RtlDebugPrint("Failed to map 0x%x bytes at 0x%x: %d\n",
                      MemorySize + PageOffset,
                      Address,
                      Status);

        goto MapImageSegmentEnd;
    }

    if (Segment->MappingStart == NULL) {
        Segment->MappingStart = Address;
    }

MapImageSegmentEnd:
    if (!KSUCCESS(Status)) {
        if (FileRegionSize != 0) {
            OsMemoryUnmap(FileRegion, FileRegionSize);
        }
    }

    return Status;
}

VOID
OspImUnmapImageSegment (
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
    // There's no need to unmap each segment individually, the free address
    // space function does it all at the end.
    //

    return;
}

KSTATUS
OspImNotifyImageLoad (
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

    PROCESS_DEBUG_MODULE_CHANGE Notification;
    KSTATUS Status;

    ASSERT(OsLoadedImagesHead.Next != NULL);

    Image->Debug.DynamicLinkerBase = OsEnvironment->StartData->InterpreterBase;
    Notification.Version = PROCESS_DEBUG_MODULE_CHANGE_VERSION;
    Notification.Load = TRUE;
    Notification.Image = Image;
    Notification.BinaryNameSize = RtlStringLength(Image->FileName) + 1;
    Status = OsDebug(DebugCommandReportModuleChange,
                     0,
                     NULL,
                     &Notification,
                     sizeof(PROCESS_DEBUG_MODULE_CHANGE),
                     0);

    if (!KSUCCESS(Status)) {
        RtlDebugPrint("Warning: Failed to notify kernel of module %s: %d\n",
                      Image->FileName,
                      Status);
    }

    Status = OspImAssignModuleNumber(Image);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    return STATUS_SUCCESS;
}

VOID
OspImNotifyImageUnload (
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

    PIMAGE_STATIC_FUNCTION *Begin;
    PIMAGE_STATIC_FUNCTION *DestructorPointer;
    PROCESS_DEBUG_MODULE_CHANGE Notification;
    PIMAGE_STATIC_FUNCTIONS StaticFunctions;
    KSTATUS Status;

    //
    // Release the image lock while calling out to destructors.
    //

    if (OsImExecutableLoaded != FALSE) {
        OspReleaseImageLock();
    }

    //
    // Call the static destructor functions. These are only filled in for
    // dynamic objects. For executables, this is all handled internally in the
    // static portion of the C library.
    //

    StaticFunctions = Image->StaticFunctions;
    if ((StaticFunctions != NULL) &&
        ((Image->Flags & IMAGE_FLAG_INITIALIZED) != 0) &&
        ((Image->LoadFlags & IMAGE_LOAD_FLAG_PRIMARY_EXECUTABLE) == 0)) {

        //
        // Call the .fini_array functions in reverse order.
        //

        if (StaticFunctions->FiniArraySize >= sizeof(PIMAGE_STATIC_FUNCTION)) {
            Begin = StaticFunctions->FiniArray;
            DestructorPointer = (PVOID)(Begin) + StaticFunctions->FiniArraySize;
            DestructorPointer -= 1;
            while (DestructorPointer >= Begin) {

                //
                // Call the destructor.
                //

                (*DestructorPointer)();
                DestructorPointer -= 1;
            }
        }

        //
        // Also call the old school _fini destructor if present.
        //

        if (StaticFunctions->FiniFunction != NULL) {
            StaticFunctions->FiniFunction();
        }
    }

    ASSERT(OsLoadedImagesHead.Next != NULL);

    if (OsImExecutableLoaded != FALSE) {
        OspAcquireImageLock(TRUE);
    }

    //
    // Tear down all the TLS segments for this module.
    //

    OspTlsTearDownModule(Image);

    //
    // Notify the kernel the module is being unloaded.
    //

    Notification.Version = PROCESS_DEBUG_MODULE_CHANGE_VERSION;
    Notification.Load = FALSE;
    Notification.Image = Image;
    Notification.BinaryNameSize = RtlStringLength(Image->FileName) + 1;
    Status = OsDebug(DebugCommandReportModuleChange,
                     0,
                     NULL,
                     &Notification,
                     sizeof(PROCESS_DEBUG_MODULE_CHANGE),
                     0);

    if (!KSUCCESS(Status)) {
        RtlDebugPrint("Warning: Failed to unload module %s: %d\n",
                      Image->FileName,
                      Status);
    }

    OspImReleaseModuleNumber(Image);
    return;
}

VOID
OspImInvalidateInstructionCacheRegion (
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

    //
    // This might fail if an image has multiple segments with unmapped space
    // between, and both segments have relocations. Ignore failures, as the
    // kernel flushes everything it can, which is all that's needed.
    //

    OsFlushCache(Address, Size);
    return;
}

PSTR
OspImGetEnvironmentVariable (
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

    PPROCESS_ENVIRONMENT Environment;
    UINTN Index;
    BOOL Match;
    UINTN VariableLength;
    PSTR VariableString;

    VariableLength = RtlStringLength(Variable);
    Match = RtlAreStringsEqual(Variable,
                               IMAGE_LOAD_LIBRARY_PATH_VARIABLE,
                               VariableLength + 1);

    if (Match != FALSE) {
        if (OsImLibraryPathOverride != NULL) {
            return OsImLibraryPathOverride;
        }
    }

    if (OsImGetEnvironmentVariable != NULL) {
        return OsImGetEnvironmentVariable(Variable);
    }

    //
    // Search through the initial environment.
    //

    Environment = OsGetCurrentEnvironment();
    for (Index = 0; Index < Environment->EnvironmentCount; Index += 1) {
        VariableString = Environment->Environment[Index];
        Match = RtlAreStringsEqual(Variable,
                                   VariableString,
                                   VariableLength);

        if ((Match != FALSE) && (VariableString[VariableLength] == '=')) {
            return VariableString + VariableLength + 1;
        }
    }

    return NULL;
}

KSTATUS
OspImFinalizeSegments (
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
    KSTATUS Status;

    PageSize = OsPageSize;
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
        // If the image was so small it fit entirely in some other segment's
        // remainder, skip it.
        //

        if (Segment->MappingStart == NULL) {
            continue;
        }

        //
        // Compute the region whose protection should actually be changed.
        //

        End = (UINTN)(Segment->VirtualAddress) + Segment->MemorySize;
        End = ALIGN_RANGE_UP(End, PageSize);

        //
        // If the region has a real size, change it's protection to read-only.
        //

        if ((PVOID)End > Segment->MappingStart) {
            Size = End - (UINTN)(Segment->MappingStart);
            MapFlags = SYS_MAP_FLAG_READ;
            if ((Segment->Flags & IMAGE_MAP_FLAG_EXECUTE) != 0) {
                MapFlags |= SYS_MAP_FLAG_EXECUTE;
            }

            Status = OsSetMemoryProtection(Segment->MappingStart,
                                           Size,
                                           MapFlags);

            if (!KSUCCESS(Status)) {
                goto FinalizeSegmentsEnd;
            }
        }
    }

    Status = STATUS_SUCCESS;

FinalizeSegmentsEnd:
    return Status;
}

VOID
OspImInitializeImages (
    PLIST_ENTRY ListHead
    )

/*++

Routine Description:

    This routine initializes any new images and calls their static constructors.
    This routine assumes the list lock is already held.

Arguments:

    ListHead - Supplies a pointer to the head of the list of images to
        initialize.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PLOADED_IMAGE Image;

    //
    // Iterate over list backwards to initialize dependencies before the
    // libraries that depend on them.
    //

    OspAcquireImageLock(FALSE);
    CurrentEntry = ListHead->Previous;
    while (CurrentEntry != ListHead) {
        Image = LIST_VALUE(CurrentEntry, LOADED_IMAGE, ListEntry);
        if ((Image->Flags & IMAGE_FLAG_INITIALIZED) == 0) {

            //
            // Release the lock around initializing the image.
            //

            OspReleaseImageLock();
            OspImInitializeImage(Image);
            OspAcquireImageLock(FALSE);
            Image->Flags |= IMAGE_FLAG_INITIALIZED;
        }

        CurrentEntry = CurrentEntry->Previous;
    }

    OspReleaseImageLock();
    return;
}

VOID
OspImInitializeImage (
    PLOADED_IMAGE Image
    )

/*++

Routine Description:

    This routine is called when the image is fully loaded. It flushes the cache
    region and calls the entry point.

Arguments:

    Image - Supplies the image that has just been completely loaded.

Return Value:

    None.

--*/

{

    PIMAGE_STATIC_FUNCTION *ConstructorPointer;
    PIMAGE_STATIC_FUNCTION *End;
    PIMAGE_STATIC_FUNCTIONS StaticFunctions;

    StaticFunctions = Image->StaticFunctions;
    if (StaticFunctions == NULL) {
        return;
    }

    //
    // The executable is responsible for its own initialization.
    //

    if ((Image->LoadFlags & IMAGE_LOAD_FLAG_PRIMARY_EXECUTABLE) != 0) {
        return;
    }

    //
    // Call the .preinit_array functions.
    //

    ConstructorPointer = StaticFunctions->PreinitArray;
    End = ((PVOID)ConstructorPointer) + StaticFunctions->PreinitArraySize;
    while (ConstructorPointer < End) {

        //
        // Call the constructor function. Remember it's an array pointer, hence
        // the extra dereference.
        //

        (*ConstructorPointer)();
        ConstructorPointer += 1;
    }

    //
    // Call the old school init function if it exists.
    //

    if (StaticFunctions->InitFunction != NULL) {
        StaticFunctions->InitFunction();
    }

    //
    // Call the .init_array functions.
    //

    ConstructorPointer = StaticFunctions->InitArray;
    End = ((PVOID)ConstructorPointer) + StaticFunctions->InitArraySize;
    while (ConstructorPointer < End) {

        //
        // Call the constructor function. Remember it's an array pointer, hence
        // the extra dereference.
        //

        (*ConstructorPointer)();
        ConstructorPointer += 1;
    }

    return;
}

PVOID
OspImResolvePltEntry (
    PLOADED_IMAGE Image,
    UINTN RelocationOffset
    )

/*++

Routine Description:

    This routine implements the slow path for a Procedure Linkable Table entry
    that has not yet been resolved to its target function address. This routine
    is only called once for each PLT entry, as subsequent calls jump directly
    to the destination function address. This routine is called directly by
    assembly, which takes care of the volatile register save/restore and
    non C style return jump at the end.

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

    OspAcquireImageLock(FALSE);
    FunctionAddress = ImResolvePltEntry(Image, RelocationOffset);
    OspReleaseImageLock();
    return FunctionAddress;
}

KSTATUS
OspLoadInitialImageList (
    BOOL Relocate
    )

/*++

Routine Description:

    This routine attempts to populate the initial image list with data
    from the kernel.

Arguments:

    Relocate - Supplies a boolean indicating whether or not the loaded images
        should be relocated.

Return Value:

    Status code.

--*/

{

    PLOADED_IMAGE Executable;
    ULONG Flags;
    IMAGE_BUFFER ImageBuffer;
    PLOADED_IMAGE Interpreter;
    PLOADED_IMAGE OsLibrary;
    PPROCESS_START_DATA StartData;
    KSTATUS Status;

    Interpreter = NULL;

    ASSERT(OsLoadedImagesHead.Next == NULL);

    INITIALIZE_LIST_HEAD(&OsLoadedImagesHead);
    RtlZeroMemory(&ImageBuffer, sizeof(IMAGE_BUFFER));
    StartData = OsEnvironment->StartData;
    ImageBuffer.Size = MAX_UINTN;
    ImageBuffer.Data = StartData->OsLibraryBase;
    Status = ImAddImage(&ImageBuffer, &OsLibrary);
    if (!KSUCCESS(Status)) {
        goto LoadInitialImageListEnd;
    }

    OsLibrary->Flags |= IMAGE_FLAG_RELOCATED | IMAGE_FLAG_IMPORTS_LOADED;
    INSERT_BEFORE(&(OsLibrary->ListEntry), &OsLoadedImagesHead);
    OsLibrary->LoadFlags |= IMAGE_LOAD_FLAG_PRIMARY_LOAD;
    OsLibrary->Debug.DynamicLinkerBase = StartData->InterpreterBase;
    if ((StartData->InterpreterBase != NULL) &&
        (StartData->InterpreterBase != StartData->OsLibraryBase)) {

        ImageBuffer.Data = StartData->InterpreterBase;
        Status = ImAddImage(&ImageBuffer, &Interpreter);
        if (!KSUCCESS(Status)) {
            goto LoadInitialImageListEnd;
        }

        INSERT_BEFORE(&(Interpreter->ListEntry), &OsLoadedImagesHead);
        Interpreter->LoadFlags |= IMAGE_LOAD_FLAG_PRIMARY_LOAD;
    }

    ASSERT(StartData->ExecutableBase != StartData->InterpreterBase);

    if (StartData->ExecutableBase != StartData->OsLibraryBase) {
        ImageBuffer.Data = StartData->ExecutableBase;
        Status = ImAddImage(&ImageBuffer, &Executable);
        if (!KSUCCESS(Status)) {
            goto LoadInitialImageListEnd;
        }

        INSERT_BEFORE(&(Executable->ListEntry), &OsLoadedImagesHead);
        Executable->Debug.DynamicLinkerBase = StartData->InterpreterBase;

    } else {
        Executable = OsLibrary;
    }

    ASSERT(ImPrimaryExecutable == NULL);

    ImPrimaryExecutable = Executable;
    Executable->FileName = OsEnvironment->ImageName;
    Executable->LoadFlags |= IMAGE_LOAD_FLAG_PRIMARY_LOAD |
                             IMAGE_LOAD_FLAG_PRIMARY_EXECUTABLE;

    if (OspImGetEnvironmentVariable(LD_BIND_NOW) != NULL) {
        Executable->LoadFlags |= IMAGE_LOAD_FLAG_BIND_NOW;
    }

    //
    // If no relocations should be performed, another binary is taking care of
    // the binary linking. If this library ever requires relocations to work
    // properly, then relocate just the OS library image here.
    //

    if (Relocate != FALSE) {
        Status = ImLoadImports(&OsLoadedImagesHead);
        if (!KSUCCESS(Status)) {
            RtlDebugPrint("Failed to load initial imports: %d\n", Status);
            goto LoadInitialImageListEnd;
        }

    } else {
        Flags = IMAGE_FLAG_IMPORTS_LOADED | IMAGE_FLAG_RELOCATED |
                IMAGE_FLAG_INITIALIZED;

        OsLibrary->Flags |= Flags;
        if (Interpreter != NULL) {
            Interpreter->Flags |= Flags;
        }

        if (Executable != NULL) {
            Executable->Flags |= Flags;
        }
    }

    Status = STATUS_SUCCESS;

LoadInitialImageListEnd:
    return Status;
}

KSTATUS
OspImAssignModuleNumber (
    PLOADED_IMAGE Image
    )

/*++

Routine Description:

    This routine attempts to assign the newly loaded module an image number.

Arguments:

    Image - Supplies a pointer to the image to assign.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES if an allocation failed.

--*/

{

    UINTN Bitmap;
    UINTN BlockIndex;
    UINTN Index;
    PUINTN NewBuffer;
    UINTN NewCapacity;

    ASSERT(Image->ModuleNumber == 0);

    for (BlockIndex = 0;
         BlockIndex < OsImModuleNumberBitmapSize;
         BlockIndex += 1) {

        Bitmap = OsImModuleNumberBitmap[BlockIndex];
        for (Index = 0; Index < sizeof(UINTN) * BITS_PER_BYTE; Index += 1) {
            if ((Bitmap & (1 << Index)) == 0) {
                OsImModuleNumberBitmap[BlockIndex] |= 1 << Index;
                Image->ModuleNumber =
                        (BlockIndex * (sizeof(UINTN) * BITS_PER_BYTE)) + Index;

                if (Image->ModuleNumber > OsImModuleGeneration) {
                    OsImModuleGeneration += 1;
                }

                return STATUS_SUCCESS;
            }
        }
    }

    //
    // Allocate more space.
    //

    if (OsImModuleNumberBitmap == &OsImStaticModuleNumberBitmap) {
        NewCapacity = 8;
        NewBuffer = OsHeapAllocate(NewCapacity * sizeof(UINTN),
                                   OS_IMAGE_ALLOCATION_TAG);

    } else {
        NewCapacity = OsImModuleNumberBitmapSize * 2;
        NewBuffer = OsHeapReallocate(OsImModuleNumberBitmap,
                                     NewCapacity * sizeof(UINTN),
                                     OS_IMAGE_ALLOCATION_TAG);
    }

    if (NewBuffer == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    if (OsImModuleNumberBitmap == &OsImStaticModuleNumberBitmap) {
        NewBuffer[0] = OsImModuleNumberBitmap[0];
    }

    RtlZeroMemory(NewBuffer + OsImModuleNumberBitmapSize,
                  (NewCapacity - OsImModuleNumberBitmapSize) * sizeof(UINTN));

    Image->ModuleNumber = OsImModuleNumberBitmapSize * sizeof(UINTN) *
                          BITS_PER_BYTE;

    NewBuffer[OsImModuleNumberBitmapSize] = 1;
    if (Image->ModuleNumber > OsImModuleGeneration) {
        OsImModuleGeneration += 1;
    }

    OsImModuleNumberBitmap = NewBuffer;
    OsImModuleNumberBitmapSize = NewCapacity;
    return STATUS_SUCCESS;
}

VOID
OspImReleaseModuleNumber (
    PLOADED_IMAGE Image
    )

/*++

Routine Description:

    This routine releases the module number assigned to the loaded image.

Arguments:

    Image - Supplies a pointer to the image to release.

Return Value:

    None.

--*/

{

    UINTN BlockIndex;
    UINTN BlockOffset;

    ASSERT((Image->ModuleNumber != 0) &&
           (Image->ModuleNumber <
            OsImModuleNumberBitmapSize * sizeof(UINTN) * BITS_PER_BYTE));

    BlockIndex = Image->ModuleNumber / (sizeof(UINTN) * BITS_PER_BYTE);
    BlockOffset = Image->ModuleNumber % (sizeof(UINTN) * BITS_PER_BYTE);
    OsImModuleNumberBitmap[BlockIndex] &= ~(1 << BlockOffset);
    Image->ModuleNumber = 0;
    return;
}

