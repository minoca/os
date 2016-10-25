/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    partmbr.c

Abstract:

    This module implements support for parsing MBR-style partitioned disks.

Author:

    Evan Green 20-Mar-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "ueficore.h"
#include "part.h"

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

BOOLEAN
EfipPartitionIsValidMbr (
    EFI_MASTER_BOOT_RECORD *Mbr,
    EFI_LBA LastLba
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfiPartitionDetectMbr (
    EFI_DRIVER_BINDING_PROTOCOL *This,
    EFI_HANDLE Handle,
    EFI_DISK_IO_PROTOCOL *DiskIo,
    EFI_BLOCK_IO_PROTOCOL *BlockIo,
    EFI_DEVICE_PATH_PROTOCOL *DevicePath
    )

/*++

Routine Description:

    This routine attempts to detect an El Torito partitioned disk, and exposes
    child block devices for each partition it finds.

Arguments:

    This - Supplies the driver binding protocol instance.

    Handle - Supplies the new controller handle.

    DiskIo - Supplies a pointer to the disk I/O protocol.

    BlockIo - Supplies a pointer to the block I/O protocol.

    DevicePath - Supplies a pointer to the device path.

Return Value:

    EFI status code.

--*/

{

    UINT32 BlockSize;
    UINT64 ChildSize;
    EFI_DEVICE_PATH_PROTOCOL *DevicePathNode;
    HARDDRIVE_DEVICE_PATH DrivePath;
    UINT32 ExtMbrStartingLba;
    EFI_STATUS Found;
    UINTN Index;
    EFI_LBA LastBlock;
    EFI_DEVICE_PATH_PROTOCOL *LastDevicePathNode;
    EFI_MASTER_BOOT_RECORD *Mbr;
    UINT32 MediaId;
    HARDDRIVE_DEVICE_PATH ParentPath;
    UINT32 PartitionNumber;
    EFI_STATUS Status;
    BOOLEAN SystemPartition;

    Found = EFI_NOT_FOUND;
    BlockSize = BlockIo->Media->BlockSize;
    MediaId = BlockIo->Media->MediaId;
    LastBlock = BlockIo->Media->LastBlock;
    Mbr = EfiCoreAllocateBootPool(BlockSize);
    if (Mbr == NULL) {
        return Found;
    }

    Status = DiskIo->ReadDisk(DiskIo, MediaId, 0, BlockSize, Mbr);
    if (EFI_ERROR(Status)) {
        Found = Status;
        goto PartitionDetectMbrEnd;
    }

    if (EfipPartitionIsValidMbr(Mbr, LastBlock) == FALSE) {
        goto PartitionDetectMbrEnd;
    }

    //
    // This is a valid MBR. Add each partition. Start by getting the starting
    // and ending LBA of the parent block device.
    //

    LastDevicePathNode = NULL;
    EfiSetMem(&ParentPath, sizeof(ParentPath), 0);
    DevicePathNode = DevicePath;
    while (EfiCoreIsDevicePathEnd(DevicePathNode) == FALSE) {
        LastDevicePathNode = DevicePathNode;
        DevicePathNode = EfiCoreGetNextDevicePathNode(DevicePathNode);
    }

    if (LastDevicePathNode != NULL) {
        if ((EfiCoreGetDevicePathType(LastDevicePathNode) ==
             MEDIA_DEVICE_PATH) &&
            (EfiCoreGetDevicePathSubType(LastDevicePathNode) ==
             MEDIA_HARDDRIVE_DP)) {

            EfiCopyMem(&ParentPath, LastDevicePathNode, sizeof(ParentPath));

        } else {
            LastDevicePathNode = NULL;
        }
    }

    PartitionNumber = 0;
    EfiSetMem(&DrivePath, sizeof(DrivePath), 0);
    DrivePath.Header.Type = MEDIA_DEVICE_PATH;
    DrivePath.Header.SubType = MEDIA_HARDDRIVE_DP;
    EfiCoreSetDevicePathNodeLength(&(DrivePath.Header), sizeof(DrivePath));
    DrivePath.MBRType = MBR_TYPE_PCAT;
    DrivePath.SignatureType = SIGNATURE_TYPE_MBR;

    //
    // If this is an MBR, add each partition.
    //

    if (LastDevicePathNode == NULL) {
        for (Index = 0; Index < EFI_MAX_MBR_PARTITIONS; Index += 1) {

            //
            // Skip null/free entries.
            //

            if ((Mbr->Partition[Index].OsIndicator == 0) ||
                (EFI_UNPACK_UINT32(Mbr->Partition[Index].SizeInLba) == 0)) {

                continue;
            }

            //
            // Skip GPT guards. Code can get here if there's a GPT disk with
            // zero partitions.
            //

            if (Mbr->Partition[Index].OsIndicator ==
                EFI_PROTECTIVE_MBR_PARTITION) {

                continue;
            }

            PartitionNumber += 1;
            DrivePath.PartitionNumber = PartitionNumber;
            DrivePath.PartitionStart =
                          EFI_UNPACK_UINT32(Mbr->Partition[Index].StartingLba);

            DrivePath.PartitionSize =
                            EFI_UNPACK_UINT32(Mbr->Partition[Index].SizeInLba);

            EfiCopyMem(DrivePath.Signature,
                       &(Mbr->UniqueMbrSignature[0]),
                       sizeof(Mbr->UniqueMbrSignature));

            SystemPartition = FALSE;
            if (Mbr->Partition[Index].OsIndicator == EFI_PARTITION) {
                SystemPartition = TRUE;
            }

            Status = EfiPartitionInstallChildHandle(
                        This,
                        Handle,
                        DiskIo,
                        BlockIo,
                        DevicePath,
                        (EFI_DEVICE_PATH_PROTOCOL *)&DrivePath,
                        DrivePath.PartitionStart,
                        DrivePath.PartitionStart + DrivePath.PartitionSize - 1,
                        EFI_MBR_SIZE,
                        SystemPartition);

            if (!EFI_ERROR(Status)) {
                Found = EFI_SUCCESS;
            }
        }

    //
    // This is an extended partition. Follow the extended partition chain to
    // get all logical drives.
    //

    } else {
        ExtMbrStartingLba = 0;
        do {
            Status = DiskIo->ReadDisk(DiskIo,
                                      MediaId,
                                      ExtMbrStartingLba * BlockSize,
                                      BlockSize,
                                      Mbr);

            if (EFI_ERROR(Status)) {
                Found = Status;
                goto PartitionDetectMbrEnd;
            }

            if (EFI_UNPACK_UINT32(Mbr->Partition[0].SizeInLba) == 0) {
                break;
            }

            if ((Mbr->Partition[0].OsIndicator ==
                 EFI_EXTENDED_DOS_PARTITION) ||
                (Mbr->Partition[0].OsIndicator ==
                 EFI_EXTENDED_WINDOWS_PARTITION)) {

                ExtMbrStartingLba =
                              EFI_UNPACK_UINT32(Mbr->Partition[0].StartingLba);

                continue;
            }

            PartitionNumber += 1;
            DrivePath.PartitionNumber = PartitionNumber;
            DrivePath.PartitionStart =
                             EFI_UNPACK_UINT32(Mbr->Partition[0].StartingLba) +
                             ExtMbrStartingLba;

            DrivePath.PartitionSize =
                                EFI_UNPACK_UINT32(Mbr->Partition[0].SizeInLba);

            if ((DrivePath.PartitionStart + DrivePath.PartitionSize - 1 >=
                 ParentPath.PartitionStart + ParentPath.PartitionSize) ||
                (DrivePath.PartitionStart <= ParentPath.PartitionStart)) {

                break;
            }

            EfiSetMem(DrivePath.Signature, sizeof(DrivePath.Signature), 0);
            SystemPartition = FALSE;
            if (Mbr->Partition[0].OsIndicator == EFI_PARTITION) {
                SystemPartition = TRUE;
            }

            ChildSize = DrivePath.PartitionStart - ParentPath.PartitionStart +
                        DrivePath.PartitionSize - 1;

            Status = EfiPartitionInstallChildHandle(
                          This,
                          Handle,
                          DiskIo,
                          BlockIo,
                          DevicePath,
                          (EFI_DEVICE_PATH_PROTOCOL *)&DrivePath,
                          DrivePath.PartitionStart - ParentPath.PartitionStart,
                          ChildSize,
                          EFI_MBR_SIZE,
                          SystemPartition);

            if (!EFI_ERROR(Status)) {
                Found = EFI_SUCCESS;
            }

            if ((Mbr->Partition[1].OsIndicator !=
                 EFI_EXTENDED_DOS_PARTITION) &&
                (Mbr->Partition[1].OsIndicator !=
                 EFI_EXTENDED_WINDOWS_PARTITION)) {

                break;
            }

            ExtMbrStartingLba =
                              EFI_UNPACK_UINT32(Mbr->Partition[1].StartingLba);

            if (ExtMbrStartingLba == 0) {
                break;
            }

        } while (ExtMbrStartingLba < ParentPath.PartitionSize);
    }

PartitionDetectMbrEnd:
    EfiFreePool(Mbr);
    return Found;
}

//
// --------------------------------------------------------- Internal Functions
//

BOOLEAN
EfipPartitionIsValidMbr (
    EFI_MASTER_BOOT_RECORD *Mbr,
    EFI_LBA LastLba
    )

/*++

Routine Description:

    This routine validates the given MBR.

Arguments:

    Mbr - Supplies a pointer to the MBR to validate.

    LastLba - Supplies the last valid block address on the disk.

Return Value:

    TRUE if the MBR is valid.

    FALSE if the MBR is garbage.

--*/

{

    UINT32 EndingLba;
    UINT32 NewEndingLba;
    UINT32 OtherStart;
    UINTN PartitionIndex;
    UINTN SearchIndex;
    UINT32 Size;
    UINT32 StartingLba;
    BOOLEAN Valid;

    if (Mbr->Signature != EFI_MBR_SIGNATURE) {
        return FALSE;
    }

    Valid = FALSE;
    for (PartitionIndex = 0;
         PartitionIndex < EFI_MAX_MBR_PARTITIONS;
         PartitionIndex += 1) {

        Size = EFI_UNPACK_UINT32(Mbr->Partition[PartitionIndex].SizeInLba);
        if ((Mbr->Partition[PartitionIndex].OsIndicator == 0x00) ||
            (Size == 0)) {

            continue;
        }

        Valid = TRUE;
        StartingLba =
                 EFI_UNPACK_UINT32(Mbr->Partition[PartitionIndex].StartingLba);

        EndingLba = StartingLba + Size - 1;
        if (EndingLba > LastLba) {
            return FALSE;
        }

        //
        // Search the other entries for overlap.
        //

        for (SearchIndex = PartitionIndex + 1;
             SearchIndex < EFI_MAX_MBR_PARTITIONS;
             SearchIndex += 1) {

            Size = EFI_UNPACK_UINT32(Mbr->Partition[SearchIndex].SizeInLba);
            if ((Mbr->Partition[SearchIndex].OsIndicator == 0x00) ||
                (Size == 0)) {

                continue;
            }

            OtherStart =
                    EFI_UNPACK_UINT32(Mbr->Partition[SearchIndex].StartingLba);

            NewEndingLba = OtherStart + Size - 1;
            if ((NewEndingLba >= StartingLba) && (OtherStart <= EndingLba)) {
                return FALSE;
            }
        }
    }

    return Valid;
}

