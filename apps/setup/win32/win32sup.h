/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    win32sup.h

Abstract:

    This header contains isolated definitions for Windows support functions in
    the setup application. This is needed because windows.h defines don't
    gel nicely with Minoca OS defines.

Author:

    Evan Green 8-Oct-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure describes a partition.

Members:

    Partition - Stores the partition information.

    DiskNumber - Stores the disk number.

    PartitionNumber - Stores the partition number.

    DevicePath - Stores a pointer to the device path.

--*/

typedef struct _SETUP_WIN32_PARTITION_DESCRIPTION {
    PARTITION_DEVICE_INFORMATION Partition;
    ULONG DiskNumber;
    ULONG PartitionNumber;
    PSTR DevicePath;
} SETUP_WIN32_PARTITION_DESCRIPTION, *PSETUP_WIN32_PARTITION_DESCRIPTION;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

INT
SetupWin32EnumerateDevices (
    PSETUP_WIN32_PARTITION_DESCRIPTION *Devices,
    PULONG DeviceCount
    );

/*++

Routine Description:

    This routine enumerates all devices and partitions in the system.

Arguments:

    Devices - Supplies a pointer where an array of partition descriptions will
        be returned on success. The caller is responsible for freeing this
        array when finished (it's just a single free plus each device path).

    DeviceCount - Supplies a pointer where the number of elements in the
        array will be returned.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

VOID
SetupWin32PrintLastError (
    VOID
    );

/*++

Routine Description:

    This routine prints a description of GetLastError to standard error and
    also prints a newline.

Arguments:

    None.

Return Value:

    None.

--*/

PVOID
SetupWin32OpenDeviceId (
    ULONGLONG DeviceId
    );

/*++

Routine Description:

    This routine opens a handle to a disk or partition ID.

Arguments:

    DeviceId - Supplies the device ID to open.

Return Value:

    Returns the open handle on success.

    NULL or (PVOID)-1 on failure.

--*/

VOID
SetupWin32Close (
    PVOID Handle
    );

/*++

Routine Description:

    This routine closes a handle.

Arguments:

    Handle - Supplies a pointer to the destination to open.

Return Value:

    None.

--*/

ssize_t
SetupWin32Read (
    PVOID Handle,
    void *Buffer,
    size_t ByteCount
    );

/*++

Routine Description:

    This routine reads from an open handle.

Arguments:

    Handle - Supplies the handle.

    Buffer - Supplies a pointer where the read bytes will be returned.

    ByteCount - Supplies the number of bytes to read.

Return Value:

    Returns the number of bytes read.

    -1 on failure.

--*/

ssize_t
SetupWin32Write (
    PVOID Handle,
    void *Buffer,
    size_t ByteCount
    );

/*++

Routine Description:

    This routine writes data to an open handle.

Arguments:

    Handle - Supplies the handle.

    Buffer - Supplies a pointer to the bytes to write.

    ByteCount - Supplies the number of bytes to read.

Return Value:

    Returns the number of bytes written.

    -1 on failure.

--*/

LONGLONG
SetupWin32Seek (
    PVOID Handle,
    LONGLONG Offset
    );

/*++

Routine Description:

    This routine seeks in the current file or device.

Arguments:

    Handle - Supplies the handle.

    Offset - Supplies the new offset to set.

Return Value:

    Returns the resulting file offset after the operation.

    -1 on failure, and errno will contain more information. The file offset
    will remain unchanged.

--*/

LONGLONG
SetupWin32Tell (
    PVOID Handle
    );

/*++

Routine Description:

    This routine returns the current file pointer.

Arguments:

    Handle - Supplies the handle.

Return Value:

    Returns the file offset on success.

    -1 on failure, and errno will contain more information. The file offset
    will remain unchanged.

--*/

INT
SetupWin32FileStat (
    PVOID Handle,
    PULONGLONG FileSize
    );

/*++

Routine Description:

    This routine gets details for the given open file.

Arguments:

    Handle - Supplies the handle.

    FileSize - Supplies an optional pointer where the file size will be
        returned on success.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

