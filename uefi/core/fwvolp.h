/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    fwvolp.h

Abstract:

    This header contains internal definitions for the firmware volume support
    library.

Author:

    Evan Green 11-Mar-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "fwvol.h"
#include "fvblock.h"
#include "efiffs.h"
#include "fv2.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro returns a pointer to the firmware volume device given a pointer
// to the protocol instance.
//

#define EFI_FIRMWARE_VOLUME_FROM_THIS(_FirmwareVolumeProtocol) \
    LIST_VALUE(_FirmwareVolumeProtocol, EFI_FIRMWARE_VOLUME, VolumeProtocol)

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_FIRMWARE_VOLUME_MAGIC 0x6F567746 // 'oVwF'

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure describes information about a file in a firmware volume.

Members:

    ListEntry - Stores pointers to the next and previous FFS file list entries.

    FileHeader - Stores a pointer to the FFS file header.

    StreamHandle - Stores the stream handle.

--*/

typedef struct _EFI_FFS_FILE_LIST_ENTRY {
    LIST_ENTRY ListEntry;
    EFI_FFS_FILE_HEADER *FileHeader;
    UINTN StreamHandle;
} EFI_FFS_FILE_LIST_ENTRY, *PEFI_FFS_FILE_LIST_ENTRY;

/*++

Structure Description:

    This structure describes the internal data structure of a firmware volume.

Members:

    Magic - Stores the magic constant EFI_FIRMWARE_VOLUME_MAGIC.

    BlockIo - Stores a pointer to the firmware volume block I/O protocol.

    Handle - Stores the volume handle.

    VolumeProtocol - Stores the firmware volume protocol.

    VolumeHeader - Stores a cached copy of the firmware volume header.

    CachedVolume - Stores cached volume data.

    EndOfCachedVolume - Stores the end of the cached volume data.

    LastKey - Stores a pointer to the last search key used.

    FfsFileList - Stores the head of the list of FFS files.

    ErasePolarity - Stores the erase polarity of the device.

    IsFfs3 - Stores a boolean indicating if this is FFS version 3 (TRUE) or
        version 2 (FALSE).

    AuthenticationStatus - Stores the authentication status.

--*/

typedef struct _EFI_FIRMWARE_VOLUME {
    UINTN Magic;
    EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL *BlockIo;
    EFI_HANDLE Handle;
    EFI_FIRMWARE_VOLUME2_PROTOCOL VolumeProtocol;
    EFI_FIRMWARE_VOLUME_HEADER *VolumeHeader;
    UINT8 *CachedVolume;
    UINT8 *EndOfCachedVolume;
    EFI_FFS_FILE_LIST_ENTRY *LastKey;
    LIST_ENTRY FfsFileList;
    UINT8 ErasePolarity;
    BOOLEAN IsFfs3;
    UINT32 AuthenticationStatus;
} EFI_FIRMWARE_VOLUME, *PEFI_FIRMWARE_VOLUME;

//
// -------------------------------------------------------------------- Globals
//

extern EFI_GUID EfiFirmwareVolumeBlockProtocolGuid;

//
// -------------------------------------------------------- Function Prototypes
//

EFI_STATUS
EfiFvGetVolumeHeader (
    EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL *BlockProtocol,
    EFI_FIRMWARE_VOLUME_HEADER **Header
    );

/*++

Routine Description:

    This routine returns the firmware volume header of the volume represented
    by the given block I/O interface.

Arguments:

    BlockProtocol - Supplies an instance of the block I/O protocol.

    Header - Supplies a pointer where a pointer to the volume header allocated
        from pool will be returned on success.

Return Value:

    EFI status code.

--*/

EFI_STATUS
EfiFvReadData (
    EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL *BlockProtocol,
    EFI_LBA *StartLba,
    UINTN *Offset,
    UINTN DataSize,
    UINT8 *Data
    );

/*++

Routine Description:

    This routine reads data from the firmware volume represented by the given
    block I/O interface. This data may span multiple block ranges.

Arguments:

    BlockProtocol - Supplies an instance of the block I/O protocol.

    StartLba - Supplies a pointer that on input contains the logical block
        address to read from. On output, this will contain the logical block
        address after reading.

    Offset - Supplies a pointer that on input contains the offset within the
        block to start reading. On output, the offset into the block after
        reading will be returned.

    DataSize - Supplies the size of the data to read in bytes.

    Data - Supplies a pointer where the read data will be returned.

Return Value:

    EFI status code.

--*/

BOOLEAN
EfiFvVerifyHeaderChecksum (
    EFI_FIRMWARE_VOLUME_HEADER *VolumeHeader
    );

/*++

Routine Description:

    This routine verifies the checksum of a firmware volume header.

Arguments:

    VolumeHeader - Supplies a pointer to the volume header to verify.

Return Value:

    TRUE if the checksum verification passed.

    FALSE if the checksum verification failed.

--*/

EFIAPI
EFI_STATUS
EfiFvOpenSectionStream (
    UINTN SectionStreamLength,
    VOID *SectionStream,
    UINTN *SectionStreamHandle
    );

/*++

Routine Description:

    This routine creates and returns a new section stream handle to represent
    a new section stream.

Arguments:

    SectionStreamLength - Supplies the size in bytes of the section stream.

    SectionStream - Supplies the section stream.

    SectionStreamHandle - Supplies a pointer where a handle to the stream will
        be returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_OUT_OF_RESOURCES if memory allocation failed.

    EFI_INVALID_PARAMETER if the section stream does not end noincidentally to
    the end of the previous section.

--*/

EFIAPI
EFI_STATUS
EfiFvCloseSectionStream (
    UINTN StreamHandle
    );

/*++

Routine Description:

    This routine closes an open section stream handle.

Arguments:

    StreamHandle - Supplies the stream handle previously returned.

Return Value:

    EFI status code.

--*/

EFIAPI
EFI_STATUS
EfiFvGetSection (
    UINTN SectionStreamHandle,
    EFI_SECTION_TYPE *SectionType,
    EFI_GUID *SectionDefinitionGuid,
    UINTN SectionInstance,
    VOID **Buffer,
    UINTN *BufferSize,
    UINT32 *AuthenticationStatus,
    BOOLEAN IsFfs3Fv
    );

/*++

Routine Description:

    This routine reads a section from a given section stream.

Arguments:

    SectionStreamHandle - Supplies the stream handle of the stream to get the
        section from.

    SectionType - Supplies a pointer that on input contains the type of section
        to search for. On output, this will return the type of the section
        found.

    SectionDefinitionGuid - Supplies a pointer to the GUID of the section to
        search for if the section type indicates EFI_SECTION_GUID_DEFINED.

    SectionInstance - Supplies the instance of the requested section to
        return.

    Buffer - Supplies a pointer to a buffer value. If the value of the buffer
        is NULL, then the buffer is callee-allocated. If it is not NULL, then
        the supplied buffer is used.

    BufferSize - Supplies a pointer that on input contains the size of the
        buffer, if supplied. On output, the size of the section will be
        returned.

    AuthenticationStatus - Supplies a pointer where the authentication status
        will be returned.

    IsFfs3Fv - Supplies a boolean indicating if the firmware file system is
        version 3 (TRUE) or version 2 (FALSE).

Return Value:

    EFI_SUCCESS on success.

    EFI_PROTOCOL_ERROR if a GUIDed section was encountered but no extraction
    protocol was found.

    EFI_NOT_FOUND if an error occurred while parsing the section stream, or the
    requested section does not exist.

    EFI_OUT_OF_RESOURCES on allocation failure.

    EFI_INVALID_PARAMETER if the given section stream handle does not exist.

    EFI_WARN_TOO_SMALL if a buffer value was supplied but it was not big enough
    to hold the requested section.

--*/

EFIAPI
EFI_STATUS
EfiFvGetVolumeAttributes (
    CONST EFI_FIRMWARE_VOLUME2_PROTOCOL *This,
    EFI_FV_ATTRIBUTES *Attributes
    );

/*++

Routine Description:

    This routine returns the attributes and current settings of the firmware
    volume. Because of constraints imposed by the underlying firmware storage,
    an instance of the Firmware Volume Protocol may not be to able to support
    all possible variations of this architecture. These constraints and the
    current state of the firmware volume are exposed to the caller using the
    get volume attributes function. The get volume attributes function is
    callable only from TPL_NOTIFY and below. Behavior of this routine at any
    EFI_TPL above TPL_NOTIFY is undefined.

Arguments:

    This - Supplies the protocol instance.

    Attributes - Supplies a pointer where the volume attributes and current
        settings are returned.

Return Value:

    EFI_SUCCESS on success.

--*/

EFIAPI
EFI_STATUS
EfiFvSetVolumeAttributes (
    CONST EFI_FIRMWARE_VOLUME2_PROTOCOL *This,
    EFI_FV_ATTRIBUTES *Attributes
    );

/*++

Routine Description:

    This routine modifies current settings of the firmware volume according to
    the input parameter. This function is used to set configurable firmware
    volume attributes. Only EFI_FV_READ_STATUS, EFI_FV_WRITE_STATUS, and
    EFI_FV_LOCK_STATUS may be modified, and then only in accordance with the
    declared capabilities. All other bits of FvAttributes are ignored on input.
    On successful return, all bits of *FvAttributes are valid and it contains
    the completed EFI_FV_ATTRIBUTES for the volume. To modify an attribute, the
    corresponding status bit in the EFI_FV_ATTRIBUTES is set to the desired
    value on input. The EFI_FV_LOCK_STATUS bit does not affect the ability to
    read or write the firmware volume. Rather, once the EFI_FV_LOCK_STATUS bit
    is set, it prevents further modification to all the attribute bits. This
    routine is callable only from TPL_NOTIFY and below. Behavior of this
    routine at any EFI_TPL above TPL_NOTIFY is undefined.

Arguments:

    This - Supplies the protocol instance.

    Attributes - Supplies a that on input contains the attributes to set. On
        output, the new current settings of the firmware volume are returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the device does not support the status bit
    settings.

    EFI_ACCESS_DENIED if the device is locked and does not allow modification.

--*/

EFIAPI
EFI_STATUS
EfiFvGetVolumeInfo (
    CONST EFI_FIRMWARE_VOLUME2_PROTOCOL *This,
    CONST EFI_GUID *InformationType,
    UINTN *BufferSize,
    VOID *Buffer
    );

/*++

Routine Description:

    This routine returns information about a firmware volume. this routine
    returns information of the requested type for the requested firmware
    volume. If the volume does not support the requested information type, then
    EFI_UNSUPPORTED is returned. If the buffer is not large enough to hold the
    requested structure, EFI_BUFFER_TOO_SMALL is returned and the buffer size
    is set to the size of buffer that is required to make the request. The
    information types defined by this specification are required information
    types that all file systems must support.

Arguments:

    This - Supplies the protocol instance.

    InformationType - Supplies a pointer to the GUID defining the type of
        information being requested.

    BufferSize - Supplies a pointer that on input contains the size of the
        buffer. On output, contains the size of the data, even if the buffer
        was too small.

    Buffer - Supplies the buffer where the information value will be returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_UNSUPPORTED if the information type is not known.

    EFI_NO_MEDIA if the device has no media inserted.

    EFI_DEVICE_ERROR if a hardware error occurred accessing the device.

    EFI_VOLUME_CORRUPTED if the file system structures are gonezo.

    EFI_BUFFER_TOO_SMALL if the supplied buffer was not large enough to hold
    the requested value. In this case the buffer size will have been updated
    to contain the needed value.

--*/

EFIAPI
EFI_STATUS
EfiFvSetVolumeInfo (
    CONST EFI_FIRMWARE_VOLUME2_PROTOCOL *This,
    CONST EFI_GUID *InformationType,
    UINTN BufferSize,
    CONST VOID *Buffer
    );

/*++

Routine Description:

    This routine sets information about a firmware volume.

Arguments:

    This - Supplies the protocol instance.

    InformationType - Supplies a pointer to the GUID defining the type of
        information being set.

    BufferSize - Supplies the size of the information value in bytes.

    Buffer - Supplies the information value.

Return Value:

    EFI_SUCCESS on success.

    EFI_UNSUPPORTED if the information type is not known.

    EFI_NO_MEDIA if the device has no media inserted.

    EFI_DEVICE_ERROR if a hardware error occurred accessing the device.

    EFI_VOLUME_CORRUPTED if the file system structures are gonezo.

    EFI_WRITE_PROTECTED if the volume is read-only.

    EFI_BAD_BUFFER_SIZE if the buffer size was smaller than the size of the
    type indicated in the information type GUID.

--*/

EFIAPI
EFI_STATUS
EfiFvReadFileSection (
    CONST EFI_FIRMWARE_VOLUME2_PROTOCOL *This,
    CONST EFI_GUID *NameGuid,
    EFI_SECTION_TYPE SectionType,
    UINTN SectionInstance,
    VOID **Buffer,
    UINTN *BufferSize,
    UINT32 *AuthenticationStatus
    );

/*++

Routine Description:

    This routine locates the requested section within a file and returns it in
    a buffer. This routine is used to retrieve a specific section from a file
    within a firmware volume. The section returned is determined using a
    depth-first, left-to-right search algorithm through all sections found in
    the specified file. The output buffer is specified by a double indirection
    of the buffer parameter. The input value of Buffer is used to determine if
    the output buffer is caller allocated or is dynamically allocated by this
    routine. If the input value of the buffer is not NULL, it indicates that
    the output buffer is caller allocated. In this case, the input value of
    *BufferSize indicates the size of the caller-allocated output buffer. If
    the output buffer is not large enough to contain the entire requested
    output, it is filled up to the point that the output buffer is exhausted
    and EFI_WARN_BUFFER_TOO_SMALL is returned, and then BufferSize is returned
    with the size that is required to successfully complete the read. All other
    output parameters are returned with valid values. If the input value of the
    buffer is NULL, it indicates the output buffer is to be allocated by this
    routine. In this case, this routine will allocate an appropriately sized
    buffer from boot services pool memory, which will be returned in the buffer
    value. The size of the new buffer is returned in *BufferSize and all other
    output parameters are returned with valid values. This routine is callable
    only from TPL_NOTIFY and below. Behavior of this routine at any EFI_TPL
    above TPL_NOTIFY is undefined.

Arguments:

    This - Supplies the protocol instance.

    NameGuid - Supplies a pointer to a GUID containing the name of the file.
        All firmware file names are EFI_GUIDs. Each firmware volume file name
        must be unique.

    SectionType - Supplies the type of section to return.

    SectionInstance - Supplies the occurrence of the given section type to
        return.

    Buffer - Supplies a pointer to a buffer in which the file contents are
        returned, not including the file header.

    BufferSize - Supplies a pointer that on input indicates the size of the
        supplied buffer in bytes.

    AuthenticationStatus - Supplies a pointer where the file authentication
        status will be returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_WARN_TOO_SMALL if the buffer is too small to contain the requested
    output. The buffer is filled and truncated.

    EFI_OUT_OF_RESOURCES if an allocation failed.

    EFI_NOT_FOUND if the requested file or section was not found in the
    firmware volume or file.

    EFI_DEVICE_ERROR if a hardware error occurred accessing the device.

    EFI_ACCESS_DENIED if the firmware volume is configured to disallow reads.

    EFI_PROTOCOL_ERROR if the requested section was not found, but the file
    could not be fully parsed because a required
    GUIDED_SECTION_EXTRACTION_PROTOCOL was not found. The requested section
    might have been found if only it could be extracted.

--*/

EFIAPI
EFI_STATUS
EfiFvReadFile (
    CONST EFI_FIRMWARE_VOLUME2_PROTOCOL *This,
    CONST EFI_GUID *NameGuid,
    VOID **Buffer,
    UINTN *BufferSize,
    EFI_FV_FILETYPE *FoundType,
    EFI_FV_FILE_ATTRIBUTES *FileAttributes,
    UINT32 *AuthenticationStatus
    );

/*++

Routine Description:

    This routine retrieves a file and/or file information from the firmware
    volume. This routine is used to retrieve any file from a firmware volume
    during the DXE phase. The actual binary encoding of the file in the
    firmware volume media may be in any arbitrary format as long as it does the
    following: It is accessed using the Firmware Volume Protocol. The image
    that is returned follows the image format defined in Code Definitions:
    PI Firmware File Format. If the input buffer is NULL, it indicates the
    caller is requesting only that the type, attributes, and size of the file
    be returned and that there is no output buffer. In this case, the following
    occurs:
    - BufferSize is returned with the size that is required to
      successfully complete the read.
    - The output parameters FoundType and *FileAttributes are
      returned with valid values.
    - The returned value of *AuthenticationStatus is undefined.

    If the input buffer is not NULL, the output buffer is specified by a double
    indirection of the Buffer parameter. The input value of *Buffer is used to
    determine if the output buffer is caller allocated or is dynamically
    allocated by this routine. If the input value of *Buffer is not NULL, it
    indicates the output buffer is caller allocated. In this case, the input
    value of *BufferSize indicates the size of the caller-allocated output
    buffer. If the output buffer is not large enough to contain the entire
    requested output, it is filled up to the point that the output buffer is
    exhausted and EFI_WARN_BUFFER_TOO_SMALL is returned, and then BufferSize is
    returned with the size required to successfully complete the read. All
    other output parameters are returned with valid values. If the input buffer
    is NULL, it indicates the output buffer is to be allocated by this routine.
    In this case, this routine will allocate an appropriately sized buffer from
    boot services pool memory, which will be returned in Buffer. The size of
    the new buffer is returned in the buffer size parameter and all other
    output parameters are returned with valid values. This routine is callable
    only from TPL_NOTIFY and below. Behavior of this routine at any EFI_TPL
    above TPL_NOTIFY is undefined.

Arguments:

    This - Supplies the protocol instance.

    NameGuid - Supplies a pointer to a GUID containing the name of the file.
        All firmware file names are EFI_GUIDs. Each firmware volume file name
        must be unique.

    Buffer - Supplies a pointer to a buffer in which the file contents are
        returned, not including the file header.

    BufferSize - Supplies a pointer that on input indicates the size of the
        supplied buffer in bytes.

    FoundType - Supplies a pointer where the file type will be returned.

    FileAttributes - Supplies a pointer where the file attributes will be
        returned.

    AuthenticationStatus - Supplies a pointer where the file authentication
        status will be returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_WARN_TOO_SMALL if the buffer is too small to contain the requested
    output. The buffer is filled and truncated.

    EFI_OUT_OF_RESOURCES if an allocation failed.

    EFI_NOT_FOUND if the given name was not in the firmware volume.

    EFI_DEVICE_ERROR if a hardware error occurred accessing the device.

    EFI_ACCESS_DENIED if the firmware volume is configured to disallow reads.

--*/

EFIAPI
EFI_STATUS
EfiFvGetNextFile (
    CONST EFI_FIRMWARE_VOLUME2_PROTOCOL *This,
    VOID *Key,
    EFI_FV_FILETYPE *FileType,
    EFI_GUID *NameGuid,
    EFI_FV_FILE_ATTRIBUTES *Attributes,
    UINTN *Size
    );

/*++

Routine Description:

    This routine retrieves information about the next file in the firmware
    volume store that matches the search criteria. This routine is the
    interface that is used to search a firmware volume for a particular file.
    It is called successively until the desired file is located or the function
    returns EFI_NOT_FOUND. To filter uninteresting files from the output, the
    type of file to search for may be specified in FileType. For example, if the
    file type is EFI_FV_FILETYPE_DRIVER, only files of this type will be
    returned in the output. If the file type is EFI_FV_FILETYPE_ALL, no
    filtering of file types is done. The key parameter is used to indicate a
    starting point of the search. If the value of the key parameter is
    completely initialized to zero, the search re-initialized and starts at the
    beginning. Subsequent calls to this routine must maintain the value of *Key
    returned by the immediately previous call. The actual contents of *Key are
    implementation specific and no semantic content is implied. This routine is
    callable only from TPL_NOTIFY and below. Behavior of this routine at any
    EFI_TPL above TPL_NOTIFY is undefined.

Arguments:

    This - Supplies the protocol instance.

    Key - Supplies a pointer to a caller-allocated buffer containing the
        implementation-specific data used to track the current position of the
        search. The size of the buffer must be at least This->KeySize bytes
        long. To re-initialize the search and begin from the beginning of the
        firmware volume, the entire buffer must be cleared to zero. Other than
        clearing the buffer to initiate a new search, the caller must not
        modify the data in the buffer between calls to this routine.

    FileType - Supplies a pointer that on input contains the filter of file
        types to search for. If a file is found, its type will be returned
        here.

    NameGuid - Supplies a pointer where the name GUID of the file will be
        returned on success.

    Attributes - Supplies a pointer where the file attributes will be returned
        on success.

    Size - Supplies a pointer where the size of the file is returned on success.

Return Value:

    EFI_SUCCESS on success.

    EFI_NOT_FOUND if no files of the given file type were found.

    EFI_DEVICE_ERROR if a hardware error occurred accessing the device.

    EFI_ACCESS_DENIED if the firmware volume does not allow reads.

--*/

EFIAPI
EFI_STATUS
EfiFvWriteFile (
    CONST EFI_FIRMWARE_VOLUME2_PROTOCOL *This,
    UINT32 NumberOfFiles,
    EFI_FV_WRITE_POLICY WritePolicy,
    EFI_FV_WRITE_FILE_DATA *FileData
    );

/*++

Routine Description:

    This routine is used to write one or more files to a firmware volume. Each
    file to be written is described by an EFI_FV_WRITE_FILE_DATA structure.
    The caller must ensure that any required alignment for all files listed in
    the file data array is compatible with the firmware volume. Firmware volume
    capabilities can be determined using the get volume attributes function.
    Similarly, if the write policy is set to EFI_FV_RELIABLE_WRITE, the caller
    must check the firmware volume capabilities to ensure EFI_FV_RELIABLE_WRITE
    is supported by the firmware volume. EFI_FV_UNRELIABLE_WRITE must always be
    supported. Writing a file with a size of zero (FileData[n].BufferSize == 0)
    deletes the file from the firmware volume if it exists. Deleting a file
    must be done one at a time. Deleting a file as part of a multiple file
    write is not allowed. This routine is callable only from TPL_NOTIFY and
    below. Behavior of this routine at any EFI_TPL above TPL_NOTIFY is
    undefined.

Arguments:

    This - Supplies the protocol instance.

    NumberOfFiles - Supplies the number of elements in the write file data
        array.

    WritePolicy - Supplies the level of reliability for the write in the event
        of a power failure or other system failure during the write operation.

    FileData - Supplies a pointer to an array of file data structures, where
        each element represents a file to be written.

Return Value:

    EFI_SUCCESS on success.

    EFI_OUT_OF_RESOURCES if an allocation failed.

    EFI_NOT_FOUND if a delete was requested but the specified file was not
    found.

    EFI_DEVICE_ERROR if a hardware error occurred accessing the device.

    EFI_WRITE_PROTECTED if the firmware volume is configured to disallow writes.

    EFI_INVALID_PARAMETER if a delete was requested with multiple file writes,
    the write policy was unsupported, an unknown file type was specified, or a
    file system specific error occurred.

--*/

