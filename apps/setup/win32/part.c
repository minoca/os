/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    part.c

Abstract:

    This module implements partition support for the setup app on Minoca OS.

Author:

    Evan Green 10-Apr-2014

Environment:

    User

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../setup.h"
#include "win32sup.h"

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

    SETUP_DESTINATION_TYPE DestinationType;
    PSETUP_PARTITION_DESCRIPTION Device;
    DEVICE_ID DeviceId;
    PSETUP_PARTITION_DESCRIPTION Devices;
    ULONG Index;
    INT Result;
    ULONG WinDeviceCount;
    PSETUP_WIN32_PARTITION_DESCRIPTION WinDevices;

    *DeviceArray = NULL;
    *DeviceCount = 0;
    Devices = NULL;
    WinDevices = NULL;
    WinDeviceCount = 0;
    Result = SetupWin32EnumerateDevices(&WinDevices, &WinDeviceCount);
    if (Result != 0) {
        goto OsEnumerateDevicesEnd;
    }

    Devices = malloc(WinDeviceCount * sizeof(SETUP_PARTITION_DESCRIPTION));
    if (Devices == NULL) {
        Result = ENOMEM;
        goto OsEnumerateDevicesEnd;
    }

    memset(Devices, 0, WinDeviceCount * sizeof(SETUP_PARTITION_DESCRIPTION));
    Device = Devices;
    for (Index = 0; Index < WinDeviceCount; Index += 1) {
        memcpy(&(Device->Partition),
               &(WinDevices[Index].Partition),
               sizeof(PARTITION_DEVICE_INFORMATION));

        if ((WinDevices[Index].Partition.Flags &
             PARTITION_FLAG_RAW_DISK) != 0) {

            DestinationType = SetupDestinationDisk;
            Device->Partition.PartitionType = PartitionTypeNone;

        } else {
            DestinationType = SetupDestinationPartition;
            Device->Partition.PartitionType = PartConvertToPartitionType(
                                            Device->Partition.PartitionFormat,
                                            Device->Partition.PartitionTypeId);
        }

        assert((WinDevices[Index].DiskNumber < 0x10000) &&
               (WinDevices[Index].PartitionNumber < 0x10000));

        DeviceId = (WinDevices[Index].DiskNumber << 16) |
                   WinDevices[Index].PartitionNumber;

        Device->Destination = SetupCreateDestination(
                                                  DestinationType,
                                                  NULL,
                                                  DeviceId);

        if (Device->Destination == NULL) {
            Result = ENOMEM;
            goto OsEnumerateDevicesEnd;
        }

        Device += 1;
    }

OsEnumerateDevicesEnd:
    if (WinDevices != NULL) {
        for (Index = 0; Index < WinDeviceCount; Index += 1) {
            if (WinDevices[Index].DevicePath != NULL) {
                free(WinDevices[Index].DevicePath);
            }
        }

        free(WinDevices);
    }

    if (Result != 0) {
        if (Devices != NULL) {
            for (Index = 0; Index < WinDeviceCount; Index += 1) {
                if (Devices[Index].Destination != NULL) {
                    SetupDestroyDestination(Devices[Index].Destination);
                }
            }

            free(Devices);
            Device = NULL;
        }

        WinDeviceCount = 0;
    }

    *DeviceArray = Devices;
    *DeviceCount = WinDeviceCount;
    return Result;
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

    return NULL;
}

//
// --------------------------------------------------------- Internal Functions
//

