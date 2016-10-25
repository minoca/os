/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    info.c

Abstract:

    This module implements support for handling I/O subsystem information
    requests.

Author:

    Evan Green 10-Apr-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "iop.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
IopGetSetBootInformation (
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

KSTATUS
IopGetCacheStatistics (
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store the saved boot information.
//

IO_BOOT_INFORMATION IoBootInformation;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
IoGetSetSystemInformation (
    BOOL FromKernelMode,
    IO_INFORMATION_TYPE InformationType,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    )

/*++

Routine Description:

    This routine gets or sets system information.

Arguments:

    FromKernelMode - Supplies a boolean indicating whether or not this request
        (and the buffer associated with it) originates from user mode (FALSE)
        or kernel mode (TRUE).

    InformationType - Supplies the information type.

    Data - Supplies a pointer to the data buffer where the data is either
        returned for a get operation or given for a set operation.

    DataSize - Supplies a pointer that on input contains the size of the
        data buffer. On output, contains the required size of the data buffer.

    Set - Supplies a boolean indicating if this is a get operation (FALSE) or
        a set operation (TRUE).

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    switch (InformationType) {
    case IoInformationBoot:
        Status = IopGetSetBootInformation(Data, DataSize, Set);
        break;

    case IoInformationMountPoints:
        Status = IopGetSetMountPointInformation(Data, DataSize, Set);
        break;

    case IoInformationCacheStatistics:
        Status = IopGetCacheStatistics(Data, DataSize, Set);
        break;

    default:
        Status = STATUS_INVALID_PARAMETER;
        *DataSize = 0;
        break;
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
IopGetSetBootInformation (
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    )

/*++

Routine Description:

    This routine gets or sets boot information.

Arguments:

    Data - Supplies a pointer to the data buffer where the data is either
        returned for a get operation or given for a set operation.

    DataSize - Supplies a pointer that on input contains the size of the
        data buffer. On output, contains the required size of the data buffer.

    Set - Supplies a boolean indicating if this is a get operation (FALSE) or
        a set operation (TRUE).

Return Value:

    Status code.

--*/

{

    if (*DataSize != sizeof(IO_BOOT_INFORMATION)) {
        *DataSize = sizeof(IO_BOOT_INFORMATION);
        return STATUS_DATA_LENGTH_MISMATCH;
    }

    if (Set != FALSE) {
        *DataSize = 0;
        return STATUS_ACCESS_DENIED;
    }

    RtlCopyMemory(Data, &IoBootInformation, sizeof(IO_BOOT_INFORMATION));
    return STATUS_SUCCESS;
}

KSTATUS
IopGetCacheStatistics (
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    )

/*++

Routine Description:

    This routine gets page cache statistics.

Arguments:

    Data - Supplies a pointer to the data buffer where the data is either
        returned for a get operation or given for a set operation.

    DataSize - Supplies a pointer that on input contains the size of the
        data buffer. On output, contains the required size of the data buffer.

    Set - Supplies a boolean indicating if this is a get operation (FALSE) or
        a set operation (TRUE).

Return Value:

    Status code.

--*/

{

    if (*DataSize != sizeof(IO_CACHE_STATISTICS)) {
        *DataSize = sizeof(IO_CACHE_STATISTICS);
        return STATUS_DATA_LENGTH_MISMATCH;
    }

    if (Set != FALSE) {
        return STATUS_ACCESS_DENIED;
    }

    return IoGetCacheStatistics(Data);
}

