/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    fv2.h

Abstract:

    This header contains definitions for the UEFI Firmware Volume 2 Protocol.

Author:

    Evan Green 11-Mar-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_FIRMWARE_VOLUME2_PROTOCOL_GUID                  \
    {                                                       \
        0x220E73B6, 0x6BDB, 0x4413,                         \
        {0x84, 0x05, 0xB9, 0x74, 0xB1, 0x08, 0x61, 0x9A}    \
    }

#define EFI_FV2_READ_DISABLE_CAP        0x0000000000000001ULL
#define EFI_FV2_READ_ENABLE_CAP         0x0000000000000002ULL
#define EFI_FV2_READ_STATUS             0x0000000000000004ULL
#define EFI_FV2_WRITE_DISABLE_CAP       0x0000000000000008ULL
#define EFI_FV2_WRITE_ENABLE_CAP        0x0000000000000010ULL
#define EFI_FV2_WRITE_STATUS            0x0000000000000020ULL
#define EFI_FV2_LOCK_CAP                0x0000000000000040ULL
#define EFI_FV2_LOCK_STATUS             0x0000000000000080ULL
#define EFI_FV2_WRITE_POLICY_RELIABLE   0x0000000000000100ULL
#define EFI_FV2_READ_LOCK_CAP           0x0000000000001000ULL
#define EFI_FV2_READ_LOCK_STATUS        0x0000000000002000ULL
#define EFI_FV2_WRITE_LOCK_CAP          0x0000000000004000ULL
#define EFI_FV2_WRITE_LOCK_STATUS       0x0000000000008000ULL
#define EFI_FV2_ALIGNMENT               0x00000000001F0000ULL
#define EFI_FV2_ALIGNMENT_1             0x0000000000000000ULL
#define EFI_FV2_ALIGNMENT_2             0x0000000000010000ULL
#define EFI_FV2_ALIGNMENT_4             0x0000000000020000ULL
#define EFI_FV2_ALIGNMENT_8             0x0000000000030000ULL
#define EFI_FV2_ALIGNMENT_16            0x0000000000040000ULL
#define EFI_FV2_ALIGNMENT_32            0x0000000000050000ULL
#define EFI_FV2_ALIGNMENT_64            0x0000000000060000ULL
#define EFI_FV2_ALIGNMENT_128           0x0000000000070000ULL
#define EFI_FV2_ALIGNMENT_256           0x0000000000080000ULL
#define EFI_FV2_ALIGNMENT_512           0x0000000000090000ULL
#define EFI_FV2_ALIGNMENT_1K            0x00000000000A0000ULL
#define EFI_FV2_ALIGNMENT_2K            0x00000000000B0000ULL
#define EFI_FV2_ALIGNMENT_4K            0x00000000000C0000ULL
#define EFI_FV2_ALIGNMENT_8K            0x00000000000D0000ULL
#define EFI_FV2_ALIGNMENT_16K           0x00000000000E0000ULL
#define EFI_FV2_ALIGNMENT_32K           0x00000000000F0000ULL
#define EFI_FV2_ALIGNMENT_64K           0x0000000000100000ULL
#define EFI_FV2_ALIGNMENT_128K          0x0000000000110000ULL
#define EFI_FV2_ALIGNMENT_256K          0x0000000000120000ULL
#define EFI_FV2_ALIGNMENT_512K          0x0000000000130000ULL
#define EFI_FV2_ALIGNMENT_1M            0x0000000000140000ULL
#define EFI_FV2_ALIGNMENT_2M            0x0000000000150000ULL
#define EFI_FV2_ALIGNMENT_4M            0x0000000000160000ULL
#define EFI_FV2_ALIGNMENT_8M            0x0000000000170000ULL
#define EFI_FV2_ALIGNMENT_16M           0x0000000000180000ULL
#define EFI_FV2_ALIGNMENT_32M           0x0000000000190000ULL
#define EFI_FV2_ALIGNMENT_64M           0x00000000001A0000ULL
#define EFI_FV2_ALIGNMENT_128M          0x00000000001B0000ULL
#define EFI_FV2_ALIGNMENT_256M          0x00000000001C0000ULL
#define EFI_FV2_ALIGNMENT_512M          0x00000000001D0000ULL
#define EFI_FV2_ALIGNMENT_1G            0x00000000001E0000ULL
#define EFI_FV2_ALIGNMENT_2G            0x00000000001F0000ULL

#define EFI_FV_UNRELIABLE_WRITE   0x00000000
#define EFI_FV_RELIABLE_WRITE     0x00000001

#define EFI_FV_FILE_ATTRIB_ALIGNMENT  0x0000001F

#define EFI_FV_FILE_ATTRIB_FIXED           0x00000100
#define EFI_FV_FILE_ATTRIB_MEMORY_MAPPED   0x00000200

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _EFI_FIRMWARE_VOLUME2_PROTOCOL EFI_FIRMWARE_VOLUME2_PROTOCOL;
typedef UINT64 EFI_FV_ATTRIBUTES;
typedef UINT32 EFI_FV_FILE_ATTRIBUTES;
typedef UINT32 EFI_FV_WRITE_POLICY;

/*++

Structure Description:

    This structure defines firmware volume write file data.

Members:

    NameGuid - Stores a pointer to the GUID containing the name of the file to
        be written.

    Type - Stores the file type of the file to be written.

    FileAttributes - Stores the file attributes for the file to be written.

    Buffer - Stores a pointer to the buffer containing the file contents to be
        written.

    BufferSize - Stores the size of the file image contained in the buffer.

--*/

typedef struct {
    EFI_GUID *NameGuid;
    EFI_FV_FILETYPE Type;
    EFI_FV_FILE_ATTRIBUTES FileAttributes;
    VOID *Buffer;
    UINT32 BufferSize;
} EFI_FV_WRITE_FILE_DATA;

typedef
EFI_STATUS
(EFIAPI *EFI_FV_GET_ATTRIBUTES) (
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

typedef
EFI_STATUS
(EFIAPI *EFI_FV_SET_ATTRIBUTES) (
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

typedef
EFI_STATUS
(EFIAPI *EFI_FV_READ_FILE) (
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

typedef
EFI_STATUS
(EFIAPI *EFI_FV_READ_SECTION) (
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

typedef
EFI_STATUS
(EFIAPI *EFI_FV_WRITE_FILE) (
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

typedef
EFI_STATUS
(EFIAPI *EFI_FV_GET_NEXT_FILE) (
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

typedef
EFI_STATUS
(EFIAPI *EFI_FV_GET_INFO) (
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

typedef
EFI_STATUS
(EFIAPI *EFI_FV_SET_INFO) (
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

/*++

Structure Description:

    This structure defines firmware volume protocol.

Members:

    GetVolumeAttributes - Stores a pointer to a function used to get volume
        properties.

    SetVolumeAttributes - Stores a pointer to a function used to set volume
        properties.

    ReadFile - Stores a pointer to a function used to a firmware volume file.

    ReadSection - Stores a pointer to a function used to read a section of a
        firmware file.

    WriteFile - Stores a pointer to a function used to write one or more
        files to a firmware volume.

    GetNextFile - Stores a pointer to a function used to enumerate files on a
        firmware volume.

    KeySize - Stores the size of the search key buffer supplied to the get next
        file function. This buffer is used internally to indicate the position
        in the enumeration.

    ParentHandle - Stores the parent firmware volume handle.

    GetInfo - Stores a pointer to a function used to get volume information.

    SetInfo - Stores a pointer to a function used to set volume information.

--*/

struct _EFI_FIRMWARE_VOLUME2_PROTOCOL {
    EFI_FV_GET_ATTRIBUTES GetVolumeAttributes;
    EFI_FV_SET_ATTRIBUTES SetVolumeAttributes;
    EFI_FV_READ_FILE ReadFile;
    EFI_FV_READ_SECTION ReadSection;
    EFI_FV_WRITE_FILE WriteFile;
    EFI_FV_GET_NEXT_FILE GetNextFile;
    UINT32 KeySize;
    EFI_HANDLE ParentHandle;
    EFI_FV_GET_INFO GetInfo;
    EFI_FV_SET_INFO SetInfo;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
