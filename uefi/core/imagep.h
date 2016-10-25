/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    imagep.h

Abstract:

    This header contains internal UEFI image loading definitions.

Author:

    Evan Green 12-Mar-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "peimage.h"
#include <minoca/uefi/protocol/loadimg.h>

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_IMAGE_FILE_HANDLE_MAGIC 0x46676D49 // 'FgmI'
#define EFI_IMAGE_DATA_MAGIC 0x67616D49 // 'gamI'

#define EFI_LOAD_PE_IMAGE_ATTRIBUTE_NONE                                 0x00
#define EFI_LOAD_PE_IMAGE_ATTRIBUTE_RUNTIME_REGISTRATION                 0x01
#define EFI_LOAD_PE_IMAGE_ATTRIBUTE_DEBUG_IMAGE_INFO_TABLE_REGISTRATION  0x02

//
// Return status codes from the PE/COFF Loader services
//

#define IMAGE_ERROR_SUCCESS                      0
#define IMAGE_ERROR_IMAGE_READ                   1
#define IMAGE_ERROR_INVALID_PE_HEADER_SIGNATURE  2
#define IMAGE_ERROR_INVALID_MACHINE_TYPE         3
#define IMAGE_ERROR_INVALID_SUBSYSTEM            4
#define IMAGE_ERROR_INVALID_IMAGE_ADDRESS        5
#define IMAGE_ERROR_INVALID_IMAGE_SIZE           6
#define IMAGE_ERROR_INVALID_SECTION_ALIGNMENT    7
#define IMAGE_ERROR_SECTION_NOT_LOADED           8
#define IMAGE_ERROR_FAILED_RELOCATION            9
#define IMAGE_ERROR_FAILED_ICACHE_FLUSH          10
#define IMAGE_ERROR_UNSUPPORTED                  11

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores the internal information about loaded image.

Members:

    Magic - Stores the magic constant EFI_IMAGE_FILE_HANDLE_MAGIC.

    FreeBuffer - Stores a boolean indicating if the buffer needs to be freed
        or not.

    Source - Stores a pointer to the file buffer.

    SourceSize - Stores the size of the buffer in bytes.

--*/

typedef struct _EFI_IMAGE_FILE_HANDLE {
    UINT32 Magic;
    BOOLEAN FreeBuffer;
    VOID *Source;
    UINTN SourceSize;
} EFI_IMAGE_FILE_HANDLE, *PEFI_IMAGE_FILE_HANDLE;

typedef
RETURN_STATUS
(EFIAPI *PE_COFF_LOADER_READ_FILE) (
    VOID *FileHandle,
    UINTN FileOffset,
    UINTN *ReadSize,
    VOID *Buffer
    );

/*++

Routine Description:

    This routine reads contents of the PE/COFF image specified by the given
    file handle. The read operation copies the given number of bytes from the
    PE/COFF image starting at the given file offset into the given buffer. The
    size of the buffer actually read is returned in the read size parameter. If
    the file offset specifies an offset past the end of the PE/COFF image, a
    read size of 0 is returned. This function abstracts access to a PE/COFF
    image so it can be implemented in an environment specific manner.
    For example, SEC and PEI environments may access memory directly to read
    the contents of a PE/COFF image, and DXE or UEFI environments may require
    protocol services to read the contents of PE/COFF image stored on FLASH,
    disk, or network devices.

Arguments:

    FileHandle - Supplies a pointer to the file handle to read from.

    FileOffset - Supplies an offset in bytes from the beginning of the file to
        read.

    ReadSize - Supplies a pointer that on input contains the number of bytes to
        read. On output, returns the number of bytes read.

    Buffer - Supplies the buffer where the read data will be returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if the image could not be read.

--*/

/*++

Structure Description:

    This structure stores the internal information about loaded image.

Members:

    ImageAddress - Stores the base address of the image from the PE/COFF header.

    ImageSize - Stores the image size from the PE/COFF header.

    DestinatinAddress - Stores the optional address to relocate to. If zero,
        the image address will be used as the base.

    EntryPoint - Stores the address of the image entry point.

    ImageRead - Stores a pointer to a function used to access the image.

    Handle - Stores the handle passed in to the image read function.

    FixupData - Stores a pointer to the relocation information.

    SectionAlignment - Stores the section alignment from the PE/COFF header.

    PeCoffHeaderOffset - Stores the offset to the PE/COFF header.

    DebugDirectoryEntryRva - Stores the Relative Virtual Address of the debug
        directory, if it exists.

    CodeView - Stores a pointer to the CodeView area of the PE/COFF debug
        directory.

    PdbPointer - Stores a pointer to the PDB entry contained in the CodeView
        area, which describes a filename of the PDB used for source-level
        debug of an image.

    SizeOfHeaders - Stores the size of the headers from the PE/COFF header.

    ImageCodeMemoryType - Stores the type of memory to use for code.

    ImageDataMemoryType - Stores the type of memory to use for data.

    ImageError - Stores the error code encountered while loading the image.

    FixupDataSize - Stores the size of the fixup data that the caller must
        allocate before calling the relocate image function.

    Machine - Stores the machine type from the PE/COFF header.

    RelocationsStripped - Stores a boolean indicating if the image does not
        have any relocation information.

    IsTeImage - Stores a boolean indicating if this is a TE image.

    HiiResourceData - Stores the address of the HII resource data.

    Context - Stores implementation specific context.

--*/

typedef struct _EFI_PE_LOADER_CONTEXT {
    PHYSICAL_ADDRESS ImageAddress;
    UINT64 ImageSize;
    PHYSICAL_ADDRESS DestinationAddress;
    PHYSICAL_ADDRESS EntryPoint;
    PE_COFF_LOADER_READ_FILE ImageRead;
    VOID *Handle;
    VOID *FixupData;
    UINT32 SectionAlignment;
    UINT32 PeCoffHeaderOffset;
    UINT32 DebugDirectoryEntryRva;
    VOID *CodeView;
    CHAR8 *PdbPointer;
    UINTN SizeOfHeaders;
    UINT32 ImageCodeMemoryType;
    UINT32 ImageDataMemoryType;
    UINT32 ImageError;
    UINTN FixupDataSize;
    UINT16 Machine;
    UINT16 ImageType;
    BOOLEAN RelocationsStripped;
    BOOLEAN IsTeImage;
    PHYSICAL_ADDRESS HiiResourceData;
    UINT64 Context;
} EFI_PE_LOADER_CONTEXT, *PEFI_PE_LOADER_CONTEXT;

/*++

Structure Description:

    This structure stores the internal information about loaded image.

Members:

    Magic - Stores the magic constant EFI_IMAGE_DATA_MAGIC.

    Handle - Stores the image handle.

    Type - Stores the image type.

    Started - Stores a boolean indicating if the entry point has been invoked.

    EntryPoint - Stores the address of the image entry point.

    Information - Stores the loaded image protocol data.

    ImageBasePage - Stores the address where the image was loaded.

    ImagePageCount - Stores the size of the in-memory image in pages.

    FixupData - Stores a pointer to the relocation information.

    Tpl - Stores the TPL of the started image.

    Status - Stores the status returned by the started image.

    ExitDataSize - Stores the size of the exit data returned from the started
        image.

    ExitData - Stores the exit data returned from the started image.

    JumpBuffer - Stores a pointer to pool allocation for context save and
        restore.

    JumpContext - Stores a pointer to a buffer used for context save and
        restore.

    Machine - Stores the machine type from the PE image.

    RuntimeData - Stores a pointer to the runtime image entry.

    LoadedImageDevicePath - Stores a pointer to the loaded image device path
        protocol.

    ImageContext - Stores the PE/COFF loader image data.

    DebuggerData - Stores a pointer to the debugger image context.

    LoadImageStatus - Stores the status returned by the LoadImage service.

--*/

typedef struct _EFI_IMAGE_DATA {
    UINT32 Magic;
    EFI_HANDLE Handle;
    UINTN Type;
    BOOLEAN Started;
    EFI_IMAGE_ENTRY_POINT EntryPoint;
    EFI_LOADED_IMAGE_PROTOCOL Information;
    EFI_PHYSICAL_ADDRESS ImageBasePage;
    UINTN ImagePageCount;
    CHAR8 *FixupData;
    EFI_TPL Tpl;
    EFI_STATUS Status;
    UINTN ExitDataSize;
    VOID *ExitData;
    VOID *JumpBuffer;
    VOID *JumpContext;
    UINT16 Machine;
    EFI_RUNTIME_IMAGE_ENTRY *RuntimeData;
    EFI_DEVICE_PATH_PROTOCOL *LoadedImageDevicePath;
    EFI_PE_LOADER_CONTEXT ImageContext;
    PVOID DebuggerData;
    EFI_STATUS LoadImageStatus;
} EFI_IMAGE_DATA, *PEFI_IMAGE_DATA;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

EFIAPI
RETURN_STATUS
EfiPeLoaderLoadImage (
    PEFI_PE_LOADER_CONTEXT Context
    );

/*++

Routine Description:

    This routine loads a PE/COFF image.

Arguments:

    Context - Supplies a pointer to the image context. Before calling this
        function the caller must have allocated the load buffer and filled in
        the image address and size fields.

Return Value:

    RETURN_SUCCESS on success.

    RETURN_INVALID_PARAMETER if the image address is invalid.

    RETURN_LOAD_ERROR if the image is a PE/COFF runtime image with no
    relocations.

    RETURN_BUFFER_TOO_SMALL if the caller provided buffer was not large enough.

--*/

EFIAPI
RETURN_STATUS
EfiPeLoaderRelocateImage (
    PEFI_PE_LOADER_CONTEXT Context
    );

/*++

Routine Description:

    This routine relocates a loaded PE image.

Arguments:

    Context - Supplies a pointer to the image context.

Return Value:

    RETURN_SUCCESS on success.

    RETURN_LOAD_ERROR if the image is not valid.

    RETURN_UNSUPPORTED if an unsupported relocation type was encountered.

--*/

EFIAPI
RETURN_STATUS
EfiPeLoaderGetImageInfo (
    PEFI_PE_LOADER_CONTEXT Context
    );

/*++

Routine Description:

    This routine extracts information about the given PE/COFF image.

Arguments:

    Context - Supplies a pointer to the image context.

Return Value:

    RETURN_SUCCESS on success.

    RETURN_INVALID_PARAMETER if the image context is NULL.

    RETURN_UNSUPPORTED if the image format is not supported.

--*/

EFIAPI
RETURN_STATUS
EfiPeLoaderUnloadImage (
    PEFI_PE_LOADER_CONTEXT Context
    );

/*++

Routine Description:

    This routine unloads the PE/COFF image.

Arguments:

    Context - Supplies a pointer to the image context.

Return Value:

    RETURN_* status code.

--*/

UINT16
EfiPeLoaderGetPeHeaderMagicValue (
    EFI_IMAGE_OPTIONAL_HEADER_PTR_UNION Header
    );

/*++

Routine Description:

    This routine returns the magic value out of the PE/COFF header.

Arguments:

    Header - Supplies a pointer to the header.

Return Value:

    Returns the magic value from the header.

--*/

RETURN_STATUS
EfiPeLoaderGetPeHeader (
    PEFI_PE_LOADER_CONTEXT Context,
    EFI_IMAGE_OPTIONAL_HEADER_PTR_UNION Header
    );

/*++

Routine Description:

    This routine retrieves the PE or TE header from a PE/COFF or TE image.

Arguments:

    Context - Supplies a pointer to the loader context.

    Header - Supplies a pointer to the header.

Return Value:

    RETURN_* error code.

--*/

