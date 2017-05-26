/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    imp.h

Abstract:

    This header contains definitions internal to the Image Library.

Author:

    Evan Green 13-Oct-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

#define RTL_API __DLLEXPORT

#include <minoca/kernel/driver.h>
#include "immux.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the initial amount to read for loading image segments.
//

#define IMAGE_INITIAL_READ_SIZE 1024

//
// Define the macros to the various functions.
//

#define ImAllocateMemory ImImportTable->AllocateMemory
#define ImFreeMemory ImImportTable->FreeMemory
#define ImOpenFile ImImportTable->OpenFile
#define ImCloseFile ImImportTable->CloseFile
#define ImLoadFile ImImportTable->LoadFile
#define ImReadFile ImImportTable->ReadFile
#define ImUnloadBuffer ImImportTable->UnloadBuffer
#define ImAllocateAddressSpace ImImportTable->AllocateAddressSpace
#define ImFreeAddressSpace ImImportTable->FreeAddressSpace
#define ImMapImageSegment ImImportTable->MapImageSegment
#define ImUnmapImageSegment ImImportTable->UnmapImageSegment
#define ImNotifyImageLoad ImImportTable->NotifyImageLoad
#define ImNotifyImageUnload ImImportTable->NotifyImageUnload
#define ImInvalidateInstructionCacheRegion \
    ImImportTable->InvalidateInstructionCacheRegion

#define ImGetEnvironmentVariable ImImportTable->GetEnvironmentVariable
#define ImFinalizeSegments ImImportTable->FinalizeSegments

//
// Define the initial scope array size.
//

#define IM_INITIAL_SCOPE_SIZE 8

//
// Define the maximum size a collection of shared object dependencies can
// reasonably grow to.
//

#define IM_MAX_SCOPE_SIZE 0x10000

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// Define the table of functions called by the image library.
//

extern PIM_IMPORT_TABLE ImImportTable;

//
// -------------------------------------------------------- Function Prototypes
//

PVOID
ImpReadBuffer (
    PIMAGE_FILE_INFORMATION File,
    PIMAGE_BUFFER Buffer,
    UINTN Offset,
    UINTN Size
    );

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
    );

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

KSTATUS
ImpAddImageToScope (
    PLOADED_IMAGE Parent,
    PLOADED_IMAGE Child
    );

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

