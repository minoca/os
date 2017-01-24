/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

#include "../setup.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _DISK_MAX_PARTITIONS {
    INT Major;
    INT Partitions;
} DISK_MAX_PARTITIONS, *PDISK_MAX_PARTITIONS;

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// For each of the defined Linux major numbers, define the maximum number of
// partitions for that major number.
//

DISK_MAX_PARTITIONS SetupLinuxMaxPartitions[] = {
    {3, 64},
    {8, 16},
    {21, 64},
    {22, 64},
    {28, 16},
    {33, 64},
    {34, 64},
    {44, 16},
    {45, 16},
    {48, 8},
    {49, 8},
    {50, 8},
    {51, 8},
    {52, 8},
    {53, 8},
    {54, 8},
    {55, 8},
    {56, 64},
    {57, 64},
    {65, 16},
    {66, 16},
    {67, 16},
    {68, 16},
    {69, 16},
    {70, 16},
    {71, 16},
    {72, 16},
    {73, 16},
    {74, 16},
    {75, 16},
    {76, 16},
    {77, 16},
    {78, 16},
    {79, 16},
    {80, 16},
    {81, 16},
    {82, 16},
    {83, 16},
    {84, 16},
    {85, 16},
    {86, 16},
    {87, 16},
    {88, 64},
    {89, 64},
    {90, 64},
    {91, 64},
    {98, 16},
    {101, 16},
    {102, 16},
    {104, 16},
    {105, 16},
    {106, 16},
    {107, 16},
    {108, 16},
    {109, 16},
    {110, 16},
    {111, 16},
    {114, 16},
    {116, 16},
    {128, 16},
    {129, 16},
    {130, 16},
    {131, 16},
    {132, 16},
    {133, 16},
    {134, 16},
    {135, 16},
    {136, 8},
    {137, 8},
    {138, 8},
    {139, 8},
    {140, 8},
    {141, 8},
    {142, 8},
    {143, 8},
    {153, 16},
    {160, 32},
    {161, 32},
    {179, 8},
    {202, 16},
    {0, 0}
};

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

    LONGLONG Blocks;
    int Count;
    PSETUP_PARTITION_DESCRIPTION Description;
    SETUP_DESTINATION_TYPE DestinationType;
    CHAR Device[256];
    FILE *File;
    INTN Index;
    CHAR Line[256];
    int Major;
    PDISK_MAX_PARTITIONS MaxPartitions;
    int Minor;
    PVOID NewBuffer;
    CHAR Path[256];
    INTN ResultCapacity;
    INTN ResultCount;
    PSETUP_PARTITION_DESCRIPTION Results;
    INT Status;

    ResultCapacity = 0;
    ResultCount = 0;
    Results = NULL;
    File = fopen("/proc/partitions", "r");
    if (File == NULL) {
        Status = errno;
        fprintf(stderr,
                "Error: Failed to open /proc/partitions: %s.\n",
                strerror(Status));

        goto OsEnumerateDevicesEnd;
    }

    //
    // Skip the first line, which has the legend.
    //

    if (fgets(Line, sizeof(Line), File) == NULL) {
        Status = errno;
        goto OsEnumerateDevicesEnd;
    }

    //
    // Loop reading all the lines.
    //

    while (fgets(Line, sizeof(Line), File) != NULL) {
        if (Line[0] == '\n') {
            continue;
        }

        Line[sizeof(Line) - 1] = '\0';
        Count = sscanf(Line, "%d %d %lld %s", &Major, &Minor, &Blocks, Device);
        if (Count != 4) {
            fprintf(stderr,
                    "Warning: Only scanned %d items in /proc/partitions.\n",
                    Count);

            continue;
        }

        if (ResultCount >= ResultCapacity) {
            if (ResultCapacity == 0) {
                ResultCapacity = 16;

            } else {
                ResultCapacity *= 2;
            }

            NewBuffer = realloc(
                         Results,
                         ResultCapacity * sizeof(SETUP_PARTITION_DESCRIPTION));

            if (NewBuffer == NULL) {
                Status = errno;
                goto OsEnumerateDevicesEnd;
            }

            Results = NewBuffer;
        }

        Description = &(Results[ResultCount]);
        memset(Description, 0, sizeof(SETUP_PARTITION_DESCRIPTION));
        snprintf(Path, sizeof(Path), "/dev/%s", Device);

        //
        // Figure out if this thing is a partition or a disk. If no entry is
        // found in the table, assume it's a disk.
        //

        DestinationType = SetupDestinationDisk;
        MaxPartitions = SetupLinuxMaxPartitions;
        while (MaxPartitions->Major != 0) {
            if (MaxPartitions->Major == Major) {
                if ((Minor % MaxPartitions->Partitions) != 0) {
                    DestinationType = SetupDestinationPartition;
                    Description->Partition.Number =
                                             Minor % MaxPartitions->Partitions;
                }

                break;
            }

            MaxPartitions += 1;
        }

        Description->Destination = SetupCreateDestination(DestinationType,
                                                          Path,
                                                          0);

        if (Description->Destination == NULL) {
            Status = ENOMEM;
            goto OsEnumerateDevicesEnd;
        }

        Description->Partition.Version = PARTITION_DEVICE_INFORMATION_VERSION;

        //
        // Set the block size to 512. /proc/partitions reports things in 1k
        // blocks.
        //

        Description->Partition.BlockSize = 512;
        Description->Partition.LastBlock = (Blocks * 2) - 1;
        Description->Partition.PartitionType = PartitionTypeUnknown;
        if (DestinationType == SetupDestinationDisk) {
            Description->Partition.Flags |= PARTITION_FLAG_RAW_DISK;
        }

        ResultCount += 1;
    }

    if (ferror(File) != 0) {
        Status = errno;

    } else {
        Status = 0;
    }

OsEnumerateDevicesEnd:
    if (File != NULL) {
        fclose(File);
    }

    if (Status != 0) {
        if (Results != NULL) {
            for (Index = 0; Index < ResultCount; Index += 1) {
                if (Results[Index].Destination != NULL) {
                    SetupDestroyDestination(Results[Index].Destination);
                }
            }

            free(Results);
            Results = NULL;
        }

        ResultCount = 0;
    }

    *DeviceArray = Results;
    *DeviceCount = ResultCount;
    return Status;
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

