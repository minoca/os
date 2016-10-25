/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    diskio.h

Abstract:

    This header contains definitions for the UEFI Disk I/O Protocol.

Author:

    Evan Green 19-Mar-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_DISK_IO_PROTOCOL_GUID                           \
    {                                                       \
        0xCE345171, 0xBA0B, 0x11D2,                         \
        {0x8E, 0x4F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B}    \
    }

//
// Protocol GUID name defined in EFI1.1.
//

#define DISK_IO_PROTOCOL EFI_DISK_IO_PROTOCOL_GUID

#define EFI_DISK_IO_PROTOCOL_REVISION 0x00010000

//
// Revision defined in EFI1.1
//

#define EFI_DISK_IO_INTERFACE_REVISION EFI_DISK_IO_PROTOCOL_REVISION

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _EFI_DISK_IO_PROTOCOL EFI_DISK_IO_PROTOCOL;

//
// Protocol defined in EFI1.1.
//

typedef EFI_DISK_IO_PROTOCOL EFI_DISK_IO;

typedef
EFI_STATUS
(EFIAPI *EFI_DISK_READ) (
    EFI_DISK_IO_PROTOCOL *This,
    UINT32 MediaId,
    UINT64 Offset,
    UINTN BufferSize,
    VOID *Buffer
    );

/*++

Routine Description:

    This routine reads bytes from the disk.

Arguments:

    This - Supplies the protocol instance.

    MediaId - Supplies the ID of the media, which changes every time the media
        is replaced.

    Offset - Supplies the starting byte offset to read from.

    BufferSize - Supplies the size of the given buffer.

    Buffer - Supplies a pointer where the read data will be returned.

Return Value:

    EFI_SUCCESS if all data was successfully read.

    EFI_DEVICE_ERROR if a hardware error occurred while performing the
    operation.

    EFI_NO_MEDIA if there is no media in the device.

    EFI_MEDIA_CHANGED if the current media ID doesn't match the one passed in.

    EFI_INVALID_PARAMETER if the offset is invalid.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_DISK_WRITE) (
    EFI_DISK_IO_PROTOCOL *This,
    UINT32 MediaId,
    UINT64 Offset,
    UINTN BufferSize,
    VOID *Buffer
    );

/*++

Routine Description:

    This routine writes bytes to the disk.

Arguments:

    This - Supplies the protocol instance.

    MediaId - Supplies the ID of the media, which changes every time the media
        is replaced.

    Offset - Supplies the starting byte offset to write to.

    BufferSize - Supplies the size of the given buffer.

    Buffer - Supplies a pointer containing the data to write.

Return Value:

    EFI_SUCCESS if all data was successfully written.

    EFI_WRITE_PROTECTED if the device cannot be written to.

    EFI_DEVICE_ERROR if a hardware error occurred while performing the
    operation.

    EFI_NO_MEDIA if there is no media in the device.

    EFI_MEDIA_CHANGED if the current media ID doesn't match the one passed in.

    EFI_INVALID_PARAMETER if the offset is invalid.

--*/

/*++

Structure Description:

    This structure defines the disk I/O protocol, used to abstract Block I/O
    interfaces.

Members:

    Revision - Stores the revision number. All future revisions are backwards
        compatible.

    ReadDisk - Stores a pointer to a function used to read from the disk.

    WriteDisk - Stores a pointer to a function used to write to the disk.

--*/

struct _EFI_DISK_IO_PROTOCOL {
    UINT64 Revision;
    EFI_DISK_READ ReadDisk;
    EFI_DISK_WRITE WriteDisk;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
