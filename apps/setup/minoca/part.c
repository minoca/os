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
#include <stdlib.h>
#include <string.h>

#include "../setup.h"
#include <minoca/lib/mlibc.h>

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

UUID SetupPartitionDeviceInformationUuid = PARTITION_DEVICE_INFORMATION_UUID;

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

    UINTN AllocationSize;
    UINTN ArrayIndex;
    IO_BOOT_INFORMATION BootInformation;
    UINTN BootInformationSize;
    INT CompareResult;
    UINTN DataSize;
    SETUP_DESTINATION_TYPE DestinationType;
    PSETUP_PARTITION_DESCRIPTION Device;
    PSETUP_PARTITION_DESCRIPTION Devices;
    ULONG ResultCount;
    UINTN ResultIndex;
    PDEVICE_INFORMATION_RESULT Results;
    KSTATUS Status;

    ArrayIndex = 0;
    Devices = NULL;
    ResultCount = 0;
    Results = NULL;

    //
    // Get the boot information.
    //

    BootInformationSize = sizeof(IO_BOOT_INFORMATION);
    Status = OsGetSetSystemInformation(SystemInformationIo,
                                       IoInformationBoot,
                                       &BootInformation,
                                       &BootInformationSize,
                                       FALSE);

    if (!KSUCCESS(Status)) {
        goto OsEnumerateDevicesEnd;
    }

    //
    // Enumerate all the devices that support getting partition device
    // information.
    //

    Status = OsLocateDeviceInformation(&SetupPartitionDeviceInformationUuid,
                                       NULL,
                                       NULL,
                                       &ResultCount);

    if (Status != STATUS_BUFFER_TOO_SMALL) {
        goto OsEnumerateDevicesEnd;
    }

    if (ResultCount == 0) {
        Status = STATUS_SUCCESS;
        goto OsEnumerateDevicesEnd;
    }

    AllocationSize = sizeof(DEVICE_INFORMATION_RESULT) * ResultCount;
    Results = malloc(AllocationSize);
    if (Results == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto OsEnumerateDevicesEnd;
    }

    memset(Results, 0, AllocationSize);
    Status = OsLocateDeviceInformation(&SetupPartitionDeviceInformationUuid,
                                       NULL,
                                       Results,
                                       &ResultCount);

    if (!KSUCCESS(Status)) {
        goto OsEnumerateDevicesEnd;
    }

    if (ResultCount == 0) {
        Status = STATUS_SUCCESS;
        goto OsEnumerateDevicesEnd;
    }

    //
    // Allocate the real array.
    //

    AllocationSize = sizeof(SETUP_PARTITION_DESCRIPTION) * ResultCount;
    Devices = malloc(AllocationSize);
    if (Devices == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto OsEnumerateDevicesEnd;
    }

    memset(Devices, 0, AllocationSize);

    //
    // Loop through the results setting up the structure elements.
    //

    for (ResultIndex = 0; ResultIndex < ResultCount; ResultIndex += 1) {
        Device = &(Devices[ArrayIndex]);

        //
        // Get the partition information.
        //

        DataSize = sizeof(PARTITION_DEVICE_INFORMATION);
        Status = OsGetSetDeviceInformation(Results[ResultIndex].DeviceId,
                                           &SetupPartitionDeviceInformationUuid,
                                           &(Device->Partition),
                                           &DataSize,
                                           FALSE);

        //
        // If that worked, create the destination structure.
        //

        if (KSUCCESS(Status)) {
            if ((Device->Partition.Flags & PARTITION_FLAG_RAW_DISK) != 0) {
                DestinationType = SetupDestinationDisk;
                CompareResult = memcmp(
                                 &(Device->Partition.DiskId),
                                 &(BootInformation.SystemDiskIdentifier),
                                 sizeof(BootInformation.SystemDiskIdentifier));

                if (CompareResult == 0) {
                    Device->Flags |= SETUP_DEVICE_FLAG_SYSTEM;
                }

            } else {
                DestinationType = SetupDestinationPartition;
                CompareResult = memcmp(
                            &(Device->Partition.PartitionId),
                            &(BootInformation.SystemPartitionIdentifier),
                            sizeof(BootInformation.SystemPartitionIdentifier));

                if (CompareResult == 0) {
                    Device->Flags |= SETUP_DEVICE_FLAG_SYSTEM;
                }
            }

            Device->Destination = SetupCreateDestination(
                                                DestinationType,
                                                NULL,
                                                Results[ResultIndex].DeviceId);

            //
            // If that worked, advance the array index.
            //

            if (Device->Destination != NULL) {
                ArrayIndex += 1;
            }
        }
    }

    Status = STATUS_SUCCESS;

OsEnumerateDevicesEnd:
    if (Results != NULL) {
        free(Results);
    }

    if (!KSUCCESS(Status)) {
        if (Devices != NULL) {
            SetupDestroyDeviceDescriptions(Devices, ArrayIndex);
            Devices = NULL;
        }

        return ClConvertKstatusToErrorNumber(Status);
    }

    *DeviceArray = Devices;
    *DeviceCount = ArrayIndex;
    return 0;
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

    UINTN Size;
    KSTATUS Status;

    if (Destination->Path != NULL) {
        return EINVAL;
    }

    Size = sizeof(PARTITION_DEVICE_INFORMATION);
    Status = OsGetSetDeviceInformation(Destination->DeviceId,
                                       &SetupPartitionDeviceInformationUuid,
                                       Information,
                                       &Size,
                                       FALSE);

    if (!KSUCCESS(Status)) {
        fprintf(stderr, "Failed to get partition information: %d\n", Status);
        return ClConvertKstatusToErrorNumber(Status);
    }

    return 0;
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

    PSETUP_PARTITION_DESCRIPTION BootPartition;
    PVOID BootVolume;
    INT Compare;
    ULONG Index;
    BOOL MultipleNonSystem;
    PSETUP_PARTITION_DESCRIPTION Partition;
    ULONG PartitionCount;
    PARTITION_DEVICE_INFORMATION PartitionInformation;
    PSETUP_PARTITION_DESCRIPTION Partitions;
    INT Result;
    PSETUP_PARTITION_DESCRIPTION SecondBest;
    PSETUP_PARTITION_DESCRIPTION SystemDisk;

    BootVolume = NULL;
    Partitions = NULL;
    Result = SetupOsEnumerateDevices(&Partitions, &PartitionCount);
    if (Result != 0) {
        fprintf(stderr, "Failed to enumerate partitions.\n");
        goto OsOpenBootVolumeEnd;
    }

    //
    // First find the system disk, or at least try to.
    //

    SystemDisk = NULL;
    for (Index = 0; Index < PartitionCount; Index += 1) {
        Partition = &(Partitions[Index]);
        if ((Partition->Destination->Type == SetupDestinationDisk) &&
            ((Partition->Flags & SETUP_DEVICE_FLAG_SYSTEM) != 0)) {

            SystemDisk = Partition;
            break;
        }
    }

    //
    // Loop across all partitions and disks looking for the EFI system
    // partition.
    //

    MultipleNonSystem = FALSE;
    BootPartition = NULL;
    SecondBest = NULL;
    for (Index = 0; Index < PartitionCount; Index += 1) {
        Partition = &(Partitions[Index]);
        if ((Partition->Destination->Type == SetupDestinationPartition) &&
            (((Partition->Partition.Flags & PARTITION_FLAG_BOOT) != 0) ||
             (Partition->Partition.PartitionType == PartitionTypeEfiSystem))) {

            Compare = -1;
            if (SystemDisk != NULL) {
                Compare = memcmp(Partition->Partition.DiskId,
                                 SystemDisk->Partition.DiskId,
                                 DISK_IDENTIFIER_SIZE);
            }

            //
            // If it is on the system disk, try to assign it directly to
            // the winner, and fail if it's already set.
            //

            if (Compare == 0) {
                if (BootPartition == NULL) {
                    BootPartition = Partition;

                } else {
                    Result = ENODEV;
                    printf("Error: Setup found multiple boot "
                           "partitions.\n");

                    goto OsOpenBootVolumeEnd;
                }

            //
            // If it's not on the boot device, track it as the second best
            // and remember if there are multiple of these.
            //

            } else {
                if (SecondBest == NULL) {
                    SecondBest = Partition;

                } else {
                    MultipleNonSystem = TRUE;
                }
            }
        }
    }

    if ((BootPartition == NULL) && (SecondBest != NULL)) {
        if (MultipleNonSystem != FALSE) {
            fprintf(stderr,
                    "Error: Found more than one boot partition on the system "
                    "disk.\n");

            Result = ENODEV;
            goto OsOpenBootVolumeEnd;
        }

        BootPartition = SecondBest;
    }

    if (BootPartition == NULL) {
        fprintf(stderr, "Failed to find boot partition.\n");
        goto OsOpenBootVolumeEnd;
    }

    assert(Context->Disk == NULL);

    memset(&PartitionInformation, 0, sizeof(PartitionInformation));
    Context->Disk = SetupPartitionOpen(Context,
                                       BootPartition->Destination,
                                       &PartitionInformation);

    if (Context->Disk == NULL) {
        fprintf(stderr, "Failed to open boot partition.\n");
        goto OsOpenBootVolumeEnd;
    }

    //
    // If the disk identifier has not yet been set, set it now. This assumes
    // that if installing to a directory, the directory resides on the same
    // disk as the boot partition.
    //

    Compare = memcmp(PartitionInformation.DiskId,
                     &SetupZeroDiskIdentifier,
                     DISK_IDENTIFIER_SIZE);

    if (Compare == 0) {
        memcpy(&(Context->PartitionContext.DiskIdentifier),
               PartitionInformation.DiskId,
               DISK_IDENTIFIER_SIZE);
    }

    Context->CurrentPartitionOffset = 0;
    Context->CurrentPartitionSize = BootPartition->Partition.LastBlock + 1 -
                                    BootPartition->Partition.FirstBlock;

    //
    // Always open the boot volume in compatibility mode, since firmware and
    // others may be looking into it.
    //

    BootVolume = SetupVolumeOpen(Context,
                                 BootPartition->Destination,
                                 SetupVolumeFormatNever,
                                 TRUE);

OsOpenBootVolumeEnd:
    if (Partitions != NULL) {
        SetupDestroyDeviceDescriptions(Partitions, PartitionCount);
    }

    return BootVolume;
}

//
// --------------------------------------------------------- Internal Functions
//

