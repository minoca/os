/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    partio.c

Abstract:

    This module implements support for doing I/O directly to a partition in the
    setup application.

Author:

    Evan Green 11-Apr-2014

Environment:

    User

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

#include "setup.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

PVOID
SetupPartitionOpen (
    PSETUP_CONTEXT Context,
    PSETUP_DESTINATION Destination,
    PPARTITION_DEVICE_INFORMATION PartitionInformation
    )

/*++

Routine Description:

    This routine opens a handle to a given partition destination.

Arguments:

    Context - Supplies a pointer to the application context.

    Destination - Supplies a pointer to the destination to open.

    PartitionInformation - Supplies an optional pointer where the partition
        information will be returned on success.

Return Value:

    Returns a pointer to an opaque context on success.

    NULL on failure.

--*/

{

    PVOID Handle;
    INT Result;

    Handle = SetupOpenDestination(Destination, O_RDWR, 0);
    if (Handle != NULL) {
        SetupPartitionSeek(Context, Handle, 0);
        if (PartitionInformation != NULL) {
            Result = SetupOsGetPartitionInformation(Destination,
                                                    PartitionInformation);

            if (Result != 0) {
                SetupPartitionClose(Context, Handle);
                Handle = NULL;
            }
        }
    }

    return Handle;
}

VOID
SetupPartitionClose (
    PSETUP_CONTEXT Context,
    PVOID Handle
    )

/*++

Routine Description:

    This routine closes a partition.

Arguments:

    Context - Supplies a pointer to the application context.

    Handle - Supplies the open handle.

Return Value:

    None.

--*/

{

    SetupClose(Handle);
    return;
}

ssize_t
SetupPartitionRead (
    PSETUP_CONTEXT Context,
    PVOID Handle,
    void *Buffer,
    size_t ByteCount
    )

/*++

Routine Description:

    This routine reads from a partition.

Arguments:

    Context - Supplies a pointer to the application context.

    Handle - Supplies the handle.

    Buffer - Supplies a pointer where the read bytes will be returned.

    ByteCount - Supplies the number of bytes to read.

Return Value:

    Returns the number of bytes read on success.

--*/

{

    return SetupRead(Handle, Buffer, ByteCount);
}

ssize_t
SetupPartitionWrite (
    PSETUP_CONTEXT Context,
    PVOID Handle,
    void *Buffer,
    size_t ByteCount
    )

/*++

Routine Description:

    This routine writes to a partition.

Arguments:

    Context - Supplies a pointer to the application context.

    Handle - Supplies the handle.

    Buffer - Supplies a pointer to the data to write.

    ByteCount - Supplies the number of bytes to read.

Return Value:

    Returns the number of bytes written.

--*/

{

    return SetupWrite(Handle, Buffer, ByteCount);
}

ULONGLONG
SetupPartitionSeek (
    PSETUP_CONTEXT Context,
    PVOID Handle,
    ULONGLONG Offset
    )

/*++

Routine Description:

    This routine seeks in the current file or device.

Arguments:

    Context - Supplies a pointer to the application context.

    Handle - Supplies the handle.

    Offset - Supplies the offset in blocks to seek to.

Return Value:

    Returns the resulting file offset in blocks after the operation.

    -1 on failure, and errno will contain more information. The file offset
    will remain unchanged.

--*/

{

    ULONGLONG NewOffset;

    NewOffset = SetupSeek(
                Handle,
                (Offset + Context->CurrentPartitionOffset) * SETUP_BLOCK_SIZE);

    NewOffset -= Context->CurrentPartitionOffset * SETUP_BLOCK_SIZE;
    return NewOffset / SETUP_BLOCK_SIZE;
}

//
// --------------------------------------------------------- Internal Functions
//

