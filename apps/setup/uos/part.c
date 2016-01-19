/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    part.c

Abstract:

    This module implements partition support for the setup app on Minoca OS.

Author:

    Evan Green 19-Jan-2016

Environment:

    User

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "../setup.h"

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

INT
SetupOsEnumerateDevices (
    PSETUP_PARTITION_DESCRIPTION *DeviceArray,
    PULONG DeviceCount
    )

/*++

Routine Description:

    This routine enumerates all the disks and partitions on the system.

Arguments:

    DeviceArray - Supplies a pointer where an array of partition structures
        will be returned on success.

    DeviceCount - Supplies a pointer where the number of elements in the
        partition array will be returned on success.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    fprintf(stderr,
            "Device enumeration not currently implemented on this OS.\n");

    return ENOSYS;
}

INT
SetupOsGetPartitionInformation (
    PSETUP_DESTINATION Destination,
    PPARTITION_DEVICE_INFORMATION Information
    )

/*++

Routine Description:

    This routine returns the partition information for the given destination.

Arguments:

    Destination - Supplies a pointer to the partition to query.

    Information - Supplies a pointer where the information will be returned
        on success.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    fprintf(stderr,
            "Partition information not currently implemented on this OS.\n");

    return ENOSYS;
}

PVOID
SetupOsOpenBootVolume (
    PSETUP_CONTEXT Context
    )

/*++

Routine Description:

    This routine opens the boot volume on the current machine.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    Returns the open handle to the boot volume on success.

    NULL on failure.

--*/

{

    errno = ENOSYS;
    return NULL;
}

//
// --------------------------------------------------------- Internal Functions
//

