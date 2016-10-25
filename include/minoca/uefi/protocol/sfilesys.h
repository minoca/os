/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    sfilesys.h

Abstract:

    This header contains definitions for the UEFI Simple File System Protocol.

Author:

    Evan Green 8-Feb-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID                \
    {                                                       \
        0x964E5B22, 0x6459, 0x11D2,                         \
        {0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B}    \
    }

//
// Protocol GUID name defined in EFI1.1.
//

#define SIMPLE_FILE_SYSTEM_PROTOCOL EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID

#define EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_REVISION 0x00010000

//
// Revision defined in EFI1.1
//

#define EFI_FILE_IO_INTERFACE_REVISION EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_REVISION

//
// Open modes
//

#define EFI_FILE_MODE_READ    0x0000000000000001ULL
#define EFI_FILE_MODE_WRITE   0x0000000000000002ULL
#define EFI_FILE_MODE_CREATE  0x8000000000000000ULL

//
// File attributes
//

#define EFI_FILE_READ_ONLY  0x0000000000000001ULL
#define EFI_FILE_HIDDEN     0x0000000000000002ULL
#define EFI_FILE_SYSTEM     0x0000000000000004ULL
#define EFI_FILE_RESERVED   0x0000000000000008ULL
#define EFI_FILE_DIRECTORY  0x0000000000000010ULL
#define EFI_FILE_ARCHIVE    0x0000000000000020ULL
#define EFI_FILE_VALID_ATTR 0x0000000000000037ULL

#define EFI_FILE_PROTOCOL_REVISION    0x00010000
#define EFI_FILE_PROTOCOL_REVISION2   0x00020000

//
// Revision defined in EFI1.1.
//

#define EFI_FILE_REVISION   EFI_FILE_PROTOCOL_REVISION

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _EFI_SIMPLE_FILE_SYSTEM_PROTOCOL EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;
typedef struct _EFI_FILE_PROTOCOL EFI_FILE_PROTOCOL;
typedef struct _EFI_FILE_PROTOCOL *EFI_FILE_HANDLE;

//
// Protocol name defined in EFI1.1.
//

typedef EFI_SIMPLE_FILE_SYSTEM_PROTOCOL EFI_FILE_IO_INTERFACE;
typedef EFI_FILE_PROTOCOL EFI_FILE;

typedef
EFI_STATUS
(EFIAPI *EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_OPEN_VOLUME) (
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *This,
    EFI_FILE_PROTOCOL **Root
    );

/*++

Routine Description:

    This routine opens the root directory on a volume.

Arguments:

    This - Supplies a pointer to the protocol instance.

    Root - Supplies a pointer where the opened file handle will be returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_UNSUPPORTED if the volume does not support the requested file system
    type.

    EFI_NO_MEDIA if the device has no medium.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request.

    EFI_VOLUME_CORRUPTED if the file system structures are corrupted.

    EFI_ACCESS_DENIED if the service denied access to the file.

    EFI_OUT_OF_RESOURCES if resources could not be allocated.

    EFI_MEDIA_CHANGED if the device has a different medium in it or the medium
    is no longer supported. Any existing file handles for this volume are no
    longer valid. The volume must be reopened.

--*/

/*++

Structure Description:

    This structure defines the Simple File System protocol.

Members:

    Revision - Stores the protocol revision number. All future revisions are
        backwards compatible. Set to EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_REVISION.

    OpenVolume - Stores a pointer to a function used to open the volume.

--*/

struct _EFI_SIMPLE_FILE_SYSTEM_PROTOCOL {
    UINT64 Revision;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_OPEN_VOLUME OpenVolume;
};

typedef
EFI_STATUS
(EFIAPI *EFI_FILE_OPEN) (
    EFI_FILE_PROTOCOL *This,
    EFI_FILE_PROTOCOL **NewHandle,
    CHAR16 *FileName,
    UINT64 OpenMode,
    UINT64 Attributes
    );

/*++

Routine Description:

    This routine opens a file relative to the source file's location.

Arguments:

    This - Supplies a pointer to the protocol instance.

    NewHandle - Supplies a pointer where the new handl will be returned on
        success.

    FileName - Supplies a pointer to a null-terminated string containing the
        name of the file to open. The file name may contain the path modifiers
        "\", ".", and "..".

    OpenMode - Supplies the open mode of the file. The only valid combinations
        are Read, Read/Write, or Create/Read/Write. See EFI_FILE_MODE_*
        definitions.

    Attributes - Supplies the attributes to create the file with, which are
        only valid if the EFI_FILE_MODE_CREATE flag is set. See EFI_FILE_*
        definitions.

Return Value:

    EFI_SUCCESS on success.

    EFI_NOT_FOUND if the file could not be found on the device.

    EFI_NO_MEDIA if the device has no medium.

    EFI_MEDIA_CHANGED if the device has a different medium in it or the medium
    is no longer supported.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request.

    EFI_VOLUME_CORRUPTED if the file system structures are corrupted.

    EFI_ACCESS_DENIED if the service denied access to the file.

    EFI_OUT_OF_RESOURCES if resources could not be allocated.

    EFI_VOLUME_FULL if the volume is full.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_FILE_CLOSE) (
    EFI_FILE_PROTOCOL *This
    );

/*++

Routine Description:

    This routine closes an open file.

Arguments:

    This - Supplies a pointer to the protocol instance, the handle to close.

Return Value:

    EFI_SUCCESS on success.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_FILE_DELETE) (
    EFI_FILE_PROTOCOL *This
    );

/*++

Routine Description:

    This routine deletes an open file handle. This also closes the handle.

Arguments:

    This - Supplies a pointer to the protocol instance.

Return Value:

    EFI_SUCCESS on success.

    EFI_WARN_DELETE_FAILURE if the handle was closed but the file was not
    deleted.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_FILE_READ) (
    EFI_FILE_PROTOCOL *This,
    UINTN *BufferSize,
    VOID *Buffer
    );

/*++

Routine Description:

    This routine reads data from a file.

Arguments:

    This - Supplies a pointer to the protocol instance.

    BufferSize - Supplies a pointer that on input contains the size of the
        buffer in bytes. On output, the number of bytes successfully read will
        be returned.

    Buffer - Supplies the buffer where the read data will be returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_NO_MEDIA if the device has no medium.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request, or an attempt was made to read from a deleted file.

    EFI_VOLUME_CORRUPTED if the file system structures are corrupted.

    EFI_BUFFER_TOO_SMALL if the buffer size is too small to read the current
    directory entry. The buffer size will be updated with the needed size.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_FILE_WRITE) (
    EFI_FILE_PROTOCOL *This,
    UINTN *BufferSize,
    VOID *Buffer
    );

/*++

Routine Description:

    This routine writes data to a file.

Arguments:

    This - Supplies a pointer to the protocol instance.

    BufferSize - Supplies a pointer that on input contains the size of the
        buffer in bytes. On output, the number of bytes successfully written
        will be returned.

    Buffer - Supplies the buffer where the read data will be returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_UNSUPPORTED if the open handle is a directory.

    EFI_NO_MEDIA if the device has no medium.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request, or an attempt was made to write to a deleted file.

    EFI_VOLUME_CORRUPTED if the file system structures are corrupted.

    EFI_WRITE_PROTECTED if the file or medium is write-protected.

    EFI_ACCESS_DENIED if the file was opened read only.

    EFI_VOLUME_FULL if the volume was full.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_FILE_SET_POSITION) (
    EFI_FILE_PROTOCOL *This,
    UINT64 Position
    );

/*++

Routine Description:

    This routine sets the file position of an open file handle.

Arguments:

    This - Supplies a pointer to the protocol instance.

    Position - Supplies the new position in bytes to set.

Return Value:

    EFI_SUCCESS on success.

    EFI_UNSUPPORTED if the open handle is a directory.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request, or the file was deleted.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_FILE_GET_POSITION) (
    EFI_FILE_PROTOCOL *This,
    UINT64 *Position
    );

/*++

Routine Description:

    This routine gets the file position for an open file handle.

Arguments:

    This - Supplies a pointer to the protocol instance.

    Position - Supplies a pointer where the position in bytes from the
        beginning of the file is returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_UNSUPPORTED if the open handle is a directory.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request, or the file was deleted.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_FILE_GET_INFO) (
    EFI_FILE_PROTOCOL *This,
    EFI_GUID *InformationType,
    UINTN *BufferSize,
    VOID *Buffer
    );

/*++

Routine Description:

    This routine gets information about a file.

Arguments:

    This - Supplies a pointer to the protocol instance.

    InformationType - Supplies a pointer to the GUID identifying the
        information being requested.

    BufferSize - Supplies a pointer that on input contains the size of the
        supplied buffer in bytes. On output, the size of the data returned will
        be returned.

    Buffer - Supplies a pointer where the data is returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_UNSUPPORTED if the information type is not known.

    EFI_NO_MEDIA if the device has no media.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request.

    EFI_VOLUME_CORRUPTED if the file system structures are corrupted.

    EFI_BUFFER_TOO_SMALL if the supplied buffer was not large enough. The size
    needed will be returned in the size parameter.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_FILE_SET_INFO) (
    EFI_FILE_PROTOCOL *This,
    EFI_GUID *InformationType,
    UINTN BufferSize,
    VOID *Buffer
    );

/*++

Routine Description:

    This routine sets information about a file.

Arguments:

    This - Supplies a pointer to the protocol instance.

    InformationType - Supplies a pointer to the GUID identifying the
        information being set.

    BufferSize - Supplies the size of the data buffer.

    Buffer - Supplies a pointer to the data, whose type is defined by the
        information type.

Return Value:

    EFI_SUCCESS on success.

    EFI_UNSUPPORTED if the information type is not known.

    EFI_NO_MEDIA if the device has no media.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request.

    EFI_VOLUME_CORRUPTED if the file system structures are corrupted.

    EFI_WRITE_PROTECTED if the information type is EFI_FILE_INFO_ID,
    EFI_FILE_PROTOCOL_SYSTEM_INFO_ID, or EFI_FILE_SYSTEM_VOLUME_LABEL_ID and
    the media is read-only.

    EFI_ACCESS_DENIED if an attempt is made to change the name of a file to a
    file that already exists, an attempt is made to change the
    EFI_FILE_DIRECTORY attribute, an attempt is made to change the size of a
    directory, or the information type is EFI_FILE_INFO_ID, the file was opened
    read-only, and attempt is being made to modify a field other than Attribute.

    EFI_VOLUME_FULL if the volume is full.

    EFI_BAD_BUFFER_SIZE if the buffer size is smaller than the size required by
    the type.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_FILE_FLUSH) (
    EFI_FILE_PROTOCOL *This
    );

/*++

Routine Description:

    This routine flushes all modified data associated with a file to a device.

Arguments:

    This - Supplies a pointer to the protocol instance.

Return Value:

    EFI_SUCCESS on success.

    EFI_NO_MEDIA if the device has no media.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request.

    EFI_VOLUME_CORRUPTED if the file system structures are corrupted.

    EFI_WRITE_PROTECTED if the file or medium is write-protected.

    EFI_ACCESS_DENIED if the file is opened read-only.

    EFI_VOLUME_FULL if the volume is full.

--*/

/*++

Structure Description:

    This structure defines a File I/O Token.

Members:

    Event - Stores a pointer to an event used for non-blocking I/O. If the
        event is NULL, blocking I/O is performed. If the event is not NULL and
        non-blocking I/O is supported, then non-blocking I/O is performed, and
        the event will be signaled when the read request is completed. The
        caller must be prepared to handle the case where the callback
        associated with the event occurs before the original asynchronous I/O
        request call returns.

    Status - Stores whether or not the signaled event encountered an error.

    BufferSize - Stores the size of a buffer for calls to ReadEx and WriteEx.
        On output, the amount of data returned or actually written will be
        returned. The units are bytes.

    Buffer - Stores a buffer used by ReadEx and WriteEx.

--*/

typedef struct {
    EFI_EVENT Event;
    EFI_STATUS Status;
    UINTN BufferSize;
    VOID *Buffer;
} EFI_FILE_IO_TOKEN;

typedef
EFI_STATUS
(EFIAPI *EFI_FILE_OPEN_EX) (
    EFI_FILE_PROTOCOL *This,
    EFI_FILE_PROTOCOL **NewHandle,
    CHAR16 *FileName,
    UINT64 OpenMode,
    UINT64 Attributes,
    EFI_FILE_IO_TOKEN *Token
    );

/*++

Routine Description:

    This routine opens a file relative to the source directory's location.

Arguments:

    This - Supplies a pointer to the protocol instance that is the source
        location.

    NewHandle - Supplies a pointer where the new open handle will be returned
        on success.

    FileName - Supplies a pointer to a null-terminated string containing the
        name of the file to open. The file name may contain the path modifiers
        "\", ".", and "..".

    OpenMode - Stores the open mode of the file. The only valid combinations
        are Read, Read/Write, or Create/Read/Write. See EFI_FILE_MODE_*
        definitions.

    Attributes - Stores the attributes to create the file with, which are only
        valid if the EFI_FILE_MODE_CREATE flag is set. See EFI_FILE_*
        definitions.

    Token - Supplies a pointer to the token associated with the transaction.

Return Value:

    EFI_SUCCESS on success.

    EFI_NOT_FOUND if the file could not be found on the device.

    EFI_NO_MEDIA if the device has no medium.

    EFI_MEDIA_CHANGED if the device has a different medium in it or the medium
    is no longer supported.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request.

    EFI_VOLUME_CORRUPTED if the file system structures are corrupted.

    EFI_ACCESS_DENIED if the service denied access to the file.

    EFI_OUT_OF_RESOURCES if resources could not be allocated.

    EFI_VOLUME_FULL if the volume is full.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_FILE_READ_EX) (
    EFI_FILE_PROTOCOL *This,
    EFI_FILE_IO_TOKEN *Token
    );

/*++

Routine Description:

    This routine reads data from a file.

Arguments:

    This - Supplies a pointer to the protocol instance.

    Token - Supplies a pointer to the token associated with the transaction.

Return Value:

    EFI_SUCCESS on success. If the event is NULL, then success means the data
    was read successfully. If the event is non-NULL, then success means the
    request was successfully queued for processing.

    EFI_NO_MEDIA if the device has no medium.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request, or an attempt was made to read from a deleted file.

    EFI_VOLUME_CORRUPTED if the file system structures are corrupted.

    EFI_BUFFER_TOO_SMALL if the buffer size is too small to read the current
    directory entry. The buffer size will be updated with the needed size.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_FILE_WRITE_EX) (
    EFI_FILE_PROTOCOL *This,
    EFI_FILE_IO_TOKEN *Token
    );

/*++

Routine Description:

    This routine writes data to a file.

Arguments:

    This - Supplies a pointer to the protocol instance.

    Token - Supplies a pointer to the token associated with the transaction.

Return Value:

    EFI_SUCCESS on success. If the event is NULL, then success means the data
    was written successfully. If the event is non-NULL, then success means the
    request was successfully queued for processing.

    EFI_UNSUPPORTED if the open handle is a directory.

    EFI_NO_MEDIA if the device has no medium.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request, or an attempt was made to write to a deleted file.

    EFI_VOLUME_CORRUPTED if the file system structures are corrupted.

    EFI_WRITE_PROTECTED if the file or medium is write-protected.

    EFI_ACCESS_DENIED if the file was opened read only.

    EFI_VOLUME_FULL if the volume was full.

    EFI_OUT_OF_RESOURCES if an allocation failed.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_FILE_FLUSH_EX) (
    EFI_FILE_PROTOCOL *This,
    EFI_FILE_IO_TOKEN *Token
    );

/*++

Routine Description:

    This routine flushes all modified data associated with a file to a device.

Arguments:

    This - Supplies a pointer to the protocol instance.

    Token - Supplies a pointer to the token associated with the transaction.

Return Value:

    EFI_SUCCESS on success. If the event is NULL, then success means the data
    was written successfully. If the event is non-NULL, then success means the
    request was successfully queued for processing.

    EFI_NO_MEDIA if the device has no media.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request.

    EFI_VOLUME_CORRUPTED if the file system structures are corrupted.

    EFI_WRITE_PROTECTED if the file or medium is write-protected.

    EFI_ACCESS_DENIED if the file is opened read-only.

    EFI_VOLUME_FULL if the volume is full.

--*/

/*++

Structure Description:

    This structure defines the EFI File Protocol, which provides file IO access
    to supported file systems. An EFI_FILE_PROTOCOL provides access to a file's
    or directory's contents, and is also a reference to a location in the
    directory tree of the file system in which the file resides. With any given
    file handle, other files may be opened relative to this file's location,
    yielding new file handles.

Members:

    Revision - Stores the protocol revision number. All future revisions are
        backwards compatible.

    Open - Stores a pointer to a function used to open a file relative to a
        source directory.

    Close - Stores a pointer to a function used to close an open file.

    Delete - Stores a pointer to a function used to delete a file associated
        with an open handle.

    Read - Stores a pointer to a function used to read from a file.

    Write - Stores a pointer to a function used to write to a file.

    GetPosition - Stores a pointer to a function used to get the current
        position of an open file.

    SetPosition - Stores a pointer to a function used to set the current
        position of an open file.

    GetInfo - Stores a pointer to a function used to get information about an
        open file.

    SetInfo - Stores a pointer to a function used to set file information.

    Flush - Stores a pointer to a function used to flush outstanding file I/O
        to the disk.

    OpenEx - Stores a pointer to a function used to open a file with possible
        asynchronous I/O.

    ReadEx - Stores a pointer to a function used to read from a file,
        possibly asynchronously.

    WriteEx - Stores a pointer to a function use to write to a file,
        possibly asynchronously.

    FlushEx - Stores a pointer to a function used to flush a file,
        possibly asynchronously.

--*/

struct _EFI_FILE_PROTOCOL {
    UINT64 Revision;
    EFI_FILE_OPEN Open;
    EFI_FILE_CLOSE Close;
    EFI_FILE_DELETE Delete;
    EFI_FILE_READ Read;
    EFI_FILE_WRITE Write;
    EFI_FILE_GET_POSITION GetPosition;
    EFI_FILE_SET_POSITION SetPosition;
    EFI_FILE_GET_INFO GetInfo;
    EFI_FILE_SET_INFO SetInfo;
    EFI_FILE_FLUSH Flush;
    EFI_FILE_OPEN_EX OpenEx;
    EFI_FILE_READ_EX ReadEx;
    EFI_FILE_WRITE_EX WriteEx;
    EFI_FILE_FLUSH_EX FlushEx;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
