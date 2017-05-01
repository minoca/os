/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    fat.c

Abstract:

    This module implements library support for the File Allocation Table file
    system.

Author:

    Evan Green 23-Sep-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/fat/fatlib.h>
#include <minoca/lib/fat/fat.h>
#include "fatlibp.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the default file permissions for FAT files, since they don't store
// permissions on their own.
//

#define FAT_DEFAULT_FILE_PERMISSIONS                              \
    (FILE_PERMISSION_USER_READ | FILE_PERMISSION_USER_WRITE |     \
     FILE_PERMISSION_USER_EXECUTE |                               \
     FILE_PERMISSION_GROUP_READ | FILE_PERMISSION_GROUP_EXECUTE | \
     FILE_PERMISSION_OTHER_READ | FILE_PERMISSION_OTHER_EXECUTE)

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores a choice for FAT cluster size given the disk size.

Members:

    MaximumSize - Stores the maximum disk size in bytes for which this entry
        applies.

    ClusterSize - Stores the default cluster size for disks of this size.

--*/

typedef struct _FAT_CLUSTER_SIZE_ENTRY {
    ULONGLONG MaximumSize;
    ULONG ClusterSize;
} FAT_CLUSTER_SIZE_ENTRY, *PFAT_CLUSTER_SIZE_ENTRY;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
FatpPerformFileIo (
    PVOID FileToken,
    BOOL Write,
    PFAT_SEEK_INFORMATION FatSeekInformation,
    PFAT_IO_BUFFER IoBuffer,
    UINTN SizeInBytes,
    ULONG IoFlags,
    PVOID Irp,
    PUINTN BytesCompleted
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Set this to TRUE to prevent the use of encoded non-standard file properties.
// If encoded properties are not in use, a random short file name will be
// created when needed.
//

BOOL FatDisableEncodedProperties = FALSE;

//
// Set this to TRUE to get a debug print whenever a user or group ID is
// truncated to 16 bits.
//

BOOL FatPrintTruncatedUserIds = FALSE;

//
// Define default cluster sizes for disks up to each size. 4kB is used for all
// small disks except floppy disks to enable direct mapping of pages from disk.
//

FAT_CLUSTER_SIZE_ENTRY FatClusterSizeDefaults[] = {
    {2 * _1MB, 512},
    {8ULL * _1GB, 4 * _1KB},
    {16ULL * _1GB, 8 * _1KB},
    {32ULL * _1GB, 16 * _1KB},
    {-1ULL, 32 * _1KB}
};

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
FatFormat (
    PBLOCK_DEVICE_PARAMETERS BlockDeviceParameters,
    ULONG ClusterSize,
    ULONG Alignment
    )

/*++

Routine Description:

    This routine formats a block device, making an initial FAT file system.
    This function will render the previous contents of the disk unreadable.

Arguments:

    BlockDeviceParameters - Supplies a pointer to a structure describing the
        underlying device.

    ClusterSize - Supplies the size of each cluster. Supply 0 to use a
        default cluster size chosen based on the disk size.

    Alignment - Supplies the byte alignment for volume. This is used to byte
        align the clusters and the FATs. If knowledge of the target system's
        cache behavior is known, aligning the volume to a cache size can help
        improve performance. Supply 0 to use the default alignment of 4096
        bytes.

Return Value:

    Status code.

--*/

{

    ULONG BlockSize;
    ULONG BlocksPerByteAlignment;
    ULONG BlocksPerFat;
    PFAT_BOOT_SECTOR BootSector;
    PFAT_CLUSTER_SIZE_ENTRY ClusterSizeEntry;
    ULONGLONG CurrentBlock;
    ULONG CurrentCluster;
    ULONGLONG DiskClusters;
    ULONGLONG DiskSize;
    ULONG EndCluster;
    PVOID Fat;
    ULONGLONG FatBlock;
    ULONG FatIndex;
    FAT_FORMAT Format;
    ULONGLONG Identifier;
    PFAT32_INFORMATION_SECTOR Information;
    ULONGLONG InformationSector;
    ULONG IoFlags;
    BYTE Media;
    UCHAR NumberOfFats;
    ULONG ReservedBlockCount;
    ULONGLONG RootDirectoryBlock;
    ULONG RootDirectoryCluster;
    ULONG RootDirectorySize;
    PVOID Scratch;
    PFAT_IO_BUFFER ScratchIoBuffer;
    ULONG ScratchSize;
    KSTATUS Status;
    ULONG ThisCluster;
    ULONGLONG TotalClusters;

    IoFlags = IO_FLAG_FS_DATA | IO_FLAG_FS_METADATA;
    Scratch = NULL;
    ScratchIoBuffer = NULL;
    Media = FAT_MEDIA_DISK;

    //
    // Pick a default cluster size based on the disk size if none was specified.
    //

    if (ClusterSize == 0) {
        ClusterSizeEntry = &(FatClusterSizeDefaults[0]);
        DiskSize = BlockDeviceParameters->BlockSize *
                   BlockDeviceParameters->BlockCount;

        while ((DiskSize >= ClusterSizeEntry->MaximumSize) &&
               (ClusterSizeEntry->MaximumSize != -1ULL)) {

            ClusterSizeEntry += 1;
        }

        ClusterSize = ClusterSizeEntry->ClusterSize;
    }

    if (Alignment == 0) {
        Alignment = FAT_DEFAULT_ALIGNMENT;
    }

    if (!POWER_OF_2(ClusterSize)) {
        Status = STATUS_INVALID_PARAMETER;
        goto FormatEnd;
    }

    if ((ClusterSize % BlockDeviceParameters->BlockSize) != 0) {
        Status = STATUS_INVALID_PARAMETER;
        goto FormatEnd;
    }

    if ((Alignment % BlockDeviceParameters->BlockSize) != 0) {
        Status = STATUS_INVALID_PARAMETER;
        goto FormatEnd;
    }

    if (BlockDeviceParameters->BlockSize < 512) {
        Status = STATUS_NOT_SUPPORTED;
        goto FormatEnd;
    }

    if (!POWER_OF_2(BlockDeviceParameters->BlockSize)) {
        Status = STATUS_NOT_SUPPORTED;
        goto FormatEnd;
    }

    if (BlockDeviceParameters->BlockCount < FAT_MINIMUM_BLOCK_COUNT) {
        Status = STATUS_INVALID_PARAMETER;
        goto FormatEnd;
    }

    //
    // Allocate some scratch space.
    //

    BlockSize = BlockDeviceParameters->BlockSize;
    ScratchSize = BlockSize;
    if (ScratchSize < FAT12_MAX_FAT_SIZE) {
        ScratchSize = FAT12_MAX_FAT_SIZE;
    }

    Scratch = FatAllocatePagedMemory(BlockDeviceParameters->DeviceToken,
                                     ScratchSize);

    if (Scratch == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto FormatEnd;
    }

    ScratchIoBuffer = FatCreateIoBuffer(Scratch, ScratchSize);
    if (ScratchIoBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto FormatEnd;
    }

    //
    // Align the reserved block count up so that the FAT blocks are
    // byte-aligned.
    //

    BlocksPerByteAlignment = Alignment / BlockSize;

    ASSERT(POWER_OF_2(BlocksPerByteAlignment) != FALSE);

    ReservedBlockCount = ALIGN_RANGE_UP(32, BlocksPerByteAlignment);

    //
    // Assume the root directory sits in the reserved region, and compute its
    // size.
    //

    RootDirectoryCluster = 0;
    RootDirectorySize = FAT_MINIMUM_ROOT_DIRECTORY_SIZE;
    if (RootDirectorySize < ClusterSize) {
        RootDirectorySize = ClusterSize;
    }

    RootDirectorySize = RootDirectorySize / BlockSize;

    //
    // First compute how many clusters are on the disk, total.
    //

    DiskClusters = ((BlockDeviceParameters->BlockCount - ReservedBlockCount) *
                     BlockSize) / ClusterSize;

    DiskClusters += FAT_CLUSTER_BEGIN;

    //
    // Compute the size of the FAT for that many clusters, assuming FAT32.
    // Align the size of the FATs up to the blocks per alignment so that the
    // root directory block is byte-aligned.
    //

    Format = Fat32Format;
    TotalClusters = DiskClusters;
    BlocksPerFat = TotalClusters * FAT32_CLUSTER_WIDTH;
    BlocksPerFat = ALIGN_RANGE_UP(BlocksPerFat, BlockSize) / BlockSize;
    BlocksPerFat = ALIGN_RANGE_UP(BlocksPerFat, BlocksPerByteAlignment);
    NumberOfFats = 2;

    //
    // Now that the size of the FAT is known, recompute the number of clusters.
    //

    TotalClusters = ((ULONGLONG)(BlockDeviceParameters->BlockCount -
                                 ReservedBlockCount -
                                 (BlocksPerFat * NumberOfFats)) * BlockSize) /
                     ClusterSize;

    TotalClusters += FAT_CLUSTER_BEGIN;
    if (TotalClusters > FAT32_CLUSTER_CUTOFF) {
        TotalClusters = FAT32_CLUSTER_CUTOFF;
    }

    //
    // If the new cluster count is less than the FAT16 cutoff, recompute the
    // FAT size and cluster count for FAT16.
    //

    if (TotalClusters < FAT16_CLUSTER_CUTOFF) {
        Format = Fat16Format;
        TotalClusters = DiskClusters;
        BlocksPerFat = TotalClusters * FAT16_CLUSTER_WIDTH;
        BlocksPerFat = ALIGN_RANGE_UP(BlocksPerFat, BlockSize) / BlockSize;
        BlocksPerFat = ALIGN_RANGE_UP(BlocksPerFat, BlocksPerByteAlignment);
        TotalClusters = ((ULONGLONG)(BlockDeviceParameters->BlockCount -
                                     ReservedBlockCount -
                                     (BlocksPerFat * NumberOfFats) -
                                     RootDirectorySize) * BlockSize) /
                         ClusterSize;

        TotalClusters += FAT_CLUSTER_BEGIN;

        //
        // Don't let the cluster count bump back into FAT32 territory, as it's
        // already known that won't work out.
        //

        if (TotalClusters >= FAT16_CLUSTER_CUTOFF) {
            TotalClusters = FAT16_CLUSTER_CUTOFF - 1;
        }

        ASSERT(TotalClusters * FAT16_CLUSTER_WIDTH < BlocksPerFat * BlockSize);

        //
        // If the new cluster count is less than the FAT12 cutoff, recompute
        // the FAT size and cluster count for FAT12.
        //

        if (TotalClusters < FAT12_CLUSTER_CUTOFF) {
            Format = Fat12Format;
            TotalClusters = DiskClusters;
            BlocksPerFat = TotalClusters + ((TotalClusters + 1) >> 1);
            BlocksPerFat = ALIGN_RANGE_UP(BlocksPerFat, BlockSize) / BlockSize;
            BlocksPerFat = ALIGN_RANGE_UP(BlocksPerFat, BlocksPerByteAlignment);
            TotalClusters = ((ULONGLONG)(BlockDeviceParameters->BlockCount -
                                         ReservedBlockCount -
                                         (BlocksPerFat * NumberOfFats) -
                                         RootDirectorySize) * BlockSize) /
                             ClusterSize;

            TotalClusters += FAT_CLUSTER_BEGIN;

            //
            // Don't bump back up into FAT16 territory.
            //

            if (TotalClusters >= FAT12_CLUSTER_CUTOFF) {
                TotalClusters = FAT12_CLUSTER_CUTOFF - 1;
            }
        }

    //
    // This is FAT32, so the root directory is a normal data cluster.
    //

    } else {

        ASSERT(TotalClusters * FAT32_CLUSTER_WIDTH < BlocksPerFat * BlockSize);

        RootDirectorySize = ClusterSize / BlockSize;
        if (RootDirectorySize == 0) {
            RootDirectorySize = 1;
        }

        RootDirectoryCluster = FAT_CLUSTER_BEGIN;
    }

    //
    // Get the root directory's block offset. It should be cache-aligned.
    //

    RootDirectoryBlock = ReservedBlockCount + (BlocksPerFat * NumberOfFats);

    ASSERT(IS_ALIGNED(RootDirectoryBlock, BlocksPerByteAlignment) != FALSE);
    ASSERT(IS_ALIGNED(RootDirectoryBlock * BlockSize, Alignment) != FALSE);

    //
    // Zero out the reserved sectors.
    //

    RtlZeroMemory(Scratch, BlockSize);
    for (CurrentBlock = 1;
         CurrentBlock < ReservedBlockCount;
         CurrentBlock += 1) {

        Status = FatWriteDevice(BlockDeviceParameters->DeviceToken,
                                CurrentBlock,
                                1,
                                IoFlags,
                                NULL,
                                ScratchIoBuffer);

        if (!KSUCCESS(Status)) {
            goto FormatEnd;
        }
    }

    //
    // Create the FATs. For FAT12, write out the whole FAT at once to avoid
    // messing around with marking clusters bad that span block boundaries.
    //

    Fat = Scratch;
    if (Format == Fat12Format) {

        //
        // Initialize the FAT all at once.
        //

        ASSERT(BlocksPerFat * BlockSize <= ScratchSize);

        RtlZeroMemory(Fat, BlocksPerFat * BlockSize);
        FAT12_WRITE_CLUSTER(Fat, 0, 0xF00 | Media);
        FAT12_WRITE_CLUSTER(Fat, 1, FAT12_CLUSTER_END_STAMP);

        //
        // Mark any clusters beyond the end of the actual cluster count as bad.
        //

        CurrentCluster = TotalClusters;
        EndCluster = ((BlocksPerFat * BlockSize) * 2) / 3;
        while (CurrentCluster < EndCluster) {
            FAT12_WRITE_CLUSTER(Fat, CurrentCluster, FAT12_CLUSTER_BAD);
            CurrentCluster += 1;
        }

        for (FatIndex = 0; FatIndex < NumberOfFats; FatIndex += 1) {
            FatBlock = ReservedBlockCount + (FatIndex * BlocksPerFat);
            Status = FatWriteDevice(BlockDeviceParameters->DeviceToken,
                                    FatBlock,
                                    BlocksPerFat,
                                    IoFlags,
                                    NULL,
                                    ScratchIoBuffer);

            if (!KSUCCESS(Status)) {
                goto FormatEnd;
            }
        }

    //
    // For FAT16 and FAT32, write out the FATs a block at a time.
    //

    } else {
        for (FatIndex = 0; FatIndex < NumberOfFats; FatIndex += 1) {
            RtlZeroMemory(Fat, BlockSize);
            for (CurrentBlock = 0;
                 CurrentBlock < BlocksPerFat;
                 CurrentBlock += 1) {

                //
                // The first cluster is used by the root directory.
                //

                if (CurrentBlock == 0) {
                    if (Format == Fat16Format) {
                        ((PUSHORT)Fat)[0] = 0xFF00 | Media;
                        ((PUSHORT)Fat)[1] = FAT16_CLUSTER_END_STAMP;

                    } else {
                        ((PULONG)Fat)[0] = 0x0FFFFF00 | Media;
                        ((PULONG)Fat)[1] = FAT32_CLUSTER_END_STAMP;
                        ((PULONG)Fat)[RootDirectoryCluster] = FAT32_CLUSTER_END;
                    }
                }

                //
                // Compute the cluster number for the start of this block and
                // the start of the next block.
                //

                if (Format == Fat16Format) {
                    ThisCluster = (CurrentBlock * BlockSize) /
                                  FAT16_CLUSTER_WIDTH;

                    EndCluster = ((CurrentBlock + 1) * BlockSize) /
                                 FAT16_CLUSTER_WIDTH;

                } else {
                    ThisCluster = (CurrentBlock * BlockSize) /
                                  FAT32_CLUSTER_WIDTH;

                    EndCluster = ((CurrentBlock + 1) * BlockSize) /
                                 FAT32_CLUSTER_WIDTH;
                }

                if (EndCluster >= TotalClusters) {
                    CurrentCluster = ThisCluster;
                    if (CurrentCluster < TotalClusters) {
                        CurrentCluster = TotalClusters;
                    }

                    if (Format == Fat16Format) {
                        while (CurrentCluster < EndCluster) {
                            ((PUSHORT)Fat)[CurrentCluster - ThisCluster] =
                                                             FAT16_CLUSTER_BAD;

                            CurrentCluster += 1;
                        }

                    } else {
                        while (CurrentCluster < EndCluster) {
                            ((PULONG)Fat)[CurrentCluster - ThisCluster] =
                                                             FAT32_CLUSTER_BAD;

                            CurrentCluster += 1;
                        }
                    }
                }

                //
                // Write out the sector.
                //

                FatBlock = ReservedBlockCount + (FatIndex * BlocksPerFat) +
                           CurrentBlock;

                Status = FatWriteDevice(BlockDeviceParameters->DeviceToken,
                                        FatBlock,
                                        1,
                                        IoFlags,
                                        NULL,
                                        ScratchIoBuffer);

                if (!KSUCCESS(Status)) {
                    goto FormatEnd;
                }

                //
                // Put the buffer back to be all free.
                //

                if (CurrentBlock == 0) {
                    RtlZeroMemory(Fat, BlockSize);
                }
            }
        }
    }

    //
    // Clear out the root directory. This is either a fixed size or one cluster.
    //

    RtlZeroMemory(Scratch, BlockSize);
    for (CurrentBlock = 0;
         CurrentBlock < RootDirectorySize;
         CurrentBlock += 1) {

        Status = FatWriteDevice(BlockDeviceParameters->DeviceToken,
                                RootDirectoryBlock + CurrentBlock,
                                1,
                                IoFlags,
                                NULL,
                                ScratchIoBuffer);

        if (!KSUCCESS(Status)) {
            goto FormatEnd;
        }
    }

    //
    // Write out the FS information sector.
    //

    InformationSector = 0;
    if (Format == Fat32Format) {
        RtlZeroMemory(Scratch, BlockSize);
        Information = (PFAT32_INFORMATION_SECTOR)Scratch;
        Information->Signature1 = FAT32_SIGNATURE1;
        Information->Signature2 = FAT32_SIGNATURE2;
        Information->FreeClusters = TotalClusters - FAT_CLUSTER_BEGIN;
        Information->LastClusterAllocated = RootDirectoryCluster;
        Information->BootSignature = FAT_BOOT_SIGNATURE;
        InformationSector = ReservedBlockCount - 1;
        Status = FatWriteDevice(BlockDeviceParameters->DeviceToken,
                                InformationSector,
                                1,
                                IoFlags,
                                NULL,
                                ScratchIoBuffer);

        if (!KSUCCESS(Status)) {
            goto FormatEnd;
        }
    }

    //
    // Create the boot sector.
    //

    RtlZeroMemory(Scratch, BlockSize);
    BootSector = (PFAT_BOOT_SECTOR)Scratch;
    BootSector->Jump[0] = FAT_FIRST_JUMP_BYTE;
    BootSector->Jump[2] = FAT_THIRD_JUMP_BYTE;
    RtlStringCopy((PCHAR)BootSector->OemName, "MSDOS5.0", 9);
    BootSector->BytesPerSector = BlockSize;
    BootSector->SectorsPerCluster = ClusterSize / BlockSize;
    BootSector->ReservedSectorCount = ReservedBlockCount;
    BootSector->AllocationTableCount = NumberOfFats;
    if (Format != Fat32Format) {
        BootSector->RootDirectoryCount = (RootDirectorySize * BlockSize) /
                                         sizeof(FAT_DIRECTORY_ENTRY);
    }

    //
    // There are two fields for the total number of sectors. Use the "small"
    // field if the number of blocks fits, or set the small field to 0 and use
    // the "big" field.
    //

    if (BlockDeviceParameters->BlockCount < MAX_USHORT) {
        BootSector->SmallTotalSectors =
                                    (USHORT)BlockDeviceParameters->BlockCount;

        BootSector->BigTotalSectors = 0;

    } else {
        BootSector->SmallTotalSectors = 0;
        if (BlockDeviceParameters->BlockCount < MAX_ULONG) {
            BootSector->BigTotalSectors =
                                     (ULONG)BlockDeviceParameters->BlockCount;

        } else {
            BootSector->BigTotalSectors = MAX_ULONG;
        }
    }

    BootSector->MediaDescriptor = Media;
    BootSector->SectorsPerFileAllocationTable = 0;
    BootSector->SectorsPerTrack = 0x3F;
    BootSector->HeadCount = 0xFF;
    BootSector->HiddenSectors = 0;

    //
    // Write the FAT32-specific Extended Bios Parameter Block (EBPB).
    //

    if (Format == Fat32Format) {
        BootSector->Fat32Parameters.SectorsPerAllocationTable = BlocksPerFat;
        BootSector->Fat32Parameters.FatFlags = 0;
        BootSector->Fat32Parameters.Version = 0;
        BootSector->Fat32Parameters.RootDirectoryCluster = RootDirectoryCluster;

        ASSERT(InformationSector < MAX_ULONG);

        BootSector->Fat32Parameters.InformationSector =
                                                      (ULONG)InformationSector;

        BootSector->Fat32Parameters.BootSectorCopy = 0;
        BootSector->Fat32Parameters.PhysicalDriveNumber = 0x80;
        BootSector->Fat32Parameters.ExtendedBootSignature =
                                                   FAT_EXTENDED_BOOT_SIGNATURE;

        BootSector->Fat32Parameters.SerialNumber = FatpGetRandomNumber();
        RtlCopyMemory(BootSector->Fat32Parameters.VolumeLabel,
                      "MinocaOS   ",
                      11);

        Identifier = FAT32_IDENTIFIER;
        RtlCopyMemory(BootSector->Fat32Parameters.FatType,
                      &Identifier,
                      sizeof(ULONGLONG));

        BootSector->Fat32Parameters.Signature = FAT_BOOT_SIGNATURE;

    //
    // Write out the FAT12/16 Extended Bios Parameter Block (EBPB).
    //

    } else {
        BootSector->SectorsPerFileAllocationTable = BlocksPerFat;
        BootSector->FatParameters.ExtendedBootSignature =
                                                   FAT_EXTENDED_BOOT_SIGNATURE;

        BootSector->FatParameters.SerialNumber = FatpGetRandomNumber();
        RtlCopyMemory(BootSector->FatParameters.VolumeLabel,
                      "MinocaOS   ",
                      11);

        if (Format == Fat12Format) {
            Identifier = FAT12_IDENTIFIER;

        } else {
            Identifier = FAT16_IDENTIFIER;
        }

        RtlCopyMemory(BootSector->FatParameters.FatType,
                      &Identifier,
                      sizeof(ULONGLONG));

        BootSector->FatParameters.Signature = FAT_BOOT_SIGNATURE;
    }

    //
    // Write out the boot sector. The FAT file system is now valid.
    //

    Status = FatWriteDevice(BlockDeviceParameters->DeviceToken,
                            0,
                            1,
                            IoFlags,
                            NULL,
                            ScratchIoBuffer);

    if (!KSUCCESS(Status)) {
        goto FormatEnd;
    }

    Status = STATUS_SUCCESS;

FormatEnd:
    if (ScratchIoBuffer != NULL) {
        FatFreeIoBuffer(ScratchIoBuffer);
    }

    if (Scratch != NULL) {
        FatFreePagedMemory(BlockDeviceParameters->DeviceToken, Scratch);
    }

    return Status;
}

KSTATUS
FatMount (
    PBLOCK_DEVICE_PARAMETERS BlockDeviceParameters,
    ULONG Flags,
    PVOID *VolumeToken
    )

/*++

Routine Description:

    This routine attempts to load FAT as the file system for the given storage
    device.

Arguments:

    BlockDeviceParameters - Supplies a pointer to a structure describing the
        underlying device.

    Flags - Supplies a bitmask of FAT mount flags. See FAT_MOUNT_FLAG_* for
        definitions.

    VolumeToken - Supplies a pointer where the FAT file system will return
        an opaque token identifying the volume on success. This token will be
        passed to future calls to the file system.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES if memory could not be allocated for the
    internal file system state.

    STATUS_UNRECOGNIZED_FILE_SYSTEM if the file system on the device is not FAT.

    Other error codes.

--*/

{

    PFAT_BOOT_SECTOR BootSector;
    PFAT_IO_BUFFER BootSectorIoBuffer;
    ULONG ClusterCount;
    ULONG ClusterSize;
    ULONG DataSectorCount;
    PVOID Device;
    BOOL Fat32ExtendedBiosParameters;
    PFAT_VOLUME FatVolume;
    ULONGLONG Identifier;
    PFAT32_INFORMATION_SECTOR Information;
    ULONGLONG InformationBlock;
    PFAT_IO_BUFFER InformationIoBuffer;
    USHORT InformationSector;
    ULONG RootDirectorySize;
    ULONG SectorSize;
    ULONG SectorsPerAllocationTable;
    KSTATUS Status;
    ULONG SystemSectorCount;
    ULONG TotalSectors;

    BootSector = NULL;
    BootSectorIoBuffer = NULL;
    Device = BlockDeviceParameters->DeviceToken;
    FatVolume = NULL;
    InformationIoBuffer = NULL;
    if ((BlockDeviceParameters->BlockSize < 512) ||
        (BlockDeviceParameters->BlockCount == 0) ||
        (!POWER_OF_2(BlockDeviceParameters->BlockSize))) {

        Status = STATUS_UNRECOGNIZED_FILE_SYSTEM;
        goto MountEnd;
    }

    BootSectorIoBuffer = FatAllocateIoBuffer(Device,
                                             BlockDeviceParameters->BlockSize);

    if (BootSectorIoBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto MountEnd;
    }

    Status = FatReadDevice(Device,
                           0,
                           1,
                           IO_FLAG_FS_DATA | IO_FLAG_FS_METADATA,
                           NULL,
                           BootSectorIoBuffer);

    if (!KSUCCESS(Status)) {
        goto MountEnd;
    }

    BootSector = FatMapIoBuffer(BootSectorIoBuffer);
    if (BootSector == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto MountEnd;
    }

    //
    // Check the 0xAA55 signature first, which isn't conclusively positive at
    // all.
    //

    if (BootSector->Fat32Parameters.Signature != FAT_BOOT_SIGNATURE) {
        Status = STATUS_UNRECOGNIZED_FILE_SYSTEM;
        goto MountEnd;
    }

    //
    // Look for the FAT signature, which will be in one of the two extended
    // BIOS parameter blocks.
    //

    Fat32ExtendedBiosParameters = FALSE;
    if ((BootSector->Fat32Parameters.ExtendedBootSignature ==
         FAT_EXTENDED_BOOT_SIGNATURE) ||
        (BootSector->Fat32Parameters.ExtendedBootSignature ==
         FAT_EXTENDED_BOOT_SIGNATURE2)) {

        Fat32ExtendedBiosParameters = TRUE;
        RtlCopyMemory(&Identifier,
                      BootSector->Fat32Parameters.FatType,
                      sizeof(ULONGLONG));

    } else {
        RtlCopyMemory(&Identifier,
                      BootSector->FatParameters.FatType,
                      sizeof(ULONGLONG));
    }

    if ((Identifier != FAT_IDENTIFIER) &&
        (Identifier != FAT12_IDENTIFIER) &&
        (Identifier != FAT16_IDENTIFIER) &&
        (Identifier != FAT32_IDENTIFIER)) {

        Status = STATUS_UNRECOGNIZED_FILE_SYSTEM;
        goto MountEnd;
    }

    //
    // Validate some parameters.
    //

    SectorSize = FAT_READ_INT16(&(BootSector->BytesPerSector));
    ClusterSize = SectorSize * BootSector->SectorsPerCluster;
    if (POWER_OF_2(ClusterSize) == FALSE) {
        Status = STATUS_UNRECOGNIZED_FILE_SYSTEM;
        goto MountEnd;
    }

    //
    // The cluster size should never be less than the block size.
    //

    if (ClusterSize < BlockDeviceParameters->BlockSize) {
        Status = STATUS_NOT_SUPPORTED;
        goto MountEnd;
    }

    //
    // This is a FAT volume. Allocate and initialize accounting structures.
    //

    FatVolume = FatAllocateNonPagedMemory(Device, sizeof(FAT_VOLUME));
    if (FatVolume == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto MountEnd;
    }

    RtlZeroMemory(FatVolume, sizeof(FAT_VOLUME));
    RtlCopyMemory(&(FatVolume->Device),
                  BlockDeviceParameters,
                  sizeof(BLOCK_DEVICE_PARAMETERS));

    FatpInitializeFileMappingTree(FatVolume);
    FatVolume->BlockShift =
                          RtlCountTrailingZeros32(FatVolume->Device.BlockSize);

    FatVolume->ClusterSize = ClusterSize;
    FatVolume->ClusterShift = RtlCountTrailingZeros32(ClusterSize);
    FatVolume->SectorSize = SectorSize;
    FatVolume->ReservedSectorCount =
                            FAT_READ_INT16(&(BootSector->ReservedSectorCount));

    FatVolume->RootDirectoryCount =
                             FAT_READ_INT16(&(BootSector->RootDirectoryCount));

    SectorsPerAllocationTable = BootSector->SectorsPerFileAllocationTable;
    if (Fat32ExtendedBiosParameters != FALSE) {
        FatVolume->RootDirectoryCluster =
                              BootSector->Fat32Parameters.RootDirectoryCluster;

        InformationSector = BootSector->Fat32Parameters.InformationSector;
        if ((InformationSector != 0) && (InformationSector != 0xFFFF)) {
            FatVolume->InformationByteOffset = InformationSector * SectorSize;
        }

        SectorsPerAllocationTable =
                         BootSector->Fat32Parameters.SectorsPerAllocationTable;

        //
        // Fail to recognize unknown versions.
        //

        if (BootSector->Fat32Parameters.Version != FAT32_VERSION) {
            Status = STATUS_UNRECOGNIZED_FILE_SYSTEM;
            goto MountEnd;
        }
    }

    FatVolume->FatSize = SectorSize * SectorsPerAllocationTable;
    FatVolume->FatByteStart = FatVolume->ReservedSectorCount * SectorSize;
    FatVolume->FatCount = BootSector->AllocationTableCount;
    if ((FatVolume->FatSize == 0) || (FatVolume->FatCount == 0) ||
        (FatVolume->FatByteStart == 0)) {

        Status = STATUS_VOLUME_CORRUPT;
        goto MountEnd;
    }

    if ((Flags & FAT_MOUNT_FLAG_COMPATIBILITY_MODE) != 0) {
        FatVolume->Flags |= FAT_VOLUME_FLAG_COMPATIBILITY_MODE;
    }

    TotalSectors = FAT_READ_INT16(&(BootSector->SmallTotalSectors));
    if (TotalSectors == 0) {
        TotalSectors = BootSector->BigTotalSectors;
    }

    //
    // Figure out the size of the data area, and therefore the cluster count.
    //

    RootDirectorySize = (sizeof(FAT_DIRECTORY_ENTRY) *
                         FatVolume->RootDirectoryCount);

    RootDirectorySize = ALIGN_RANGE_UP(RootDirectorySize, SectorSize) /
                        SectorSize;

    SystemSectorCount = FatVolume->ReservedSectorCount +
                        (SectorsPerAllocationTable *
                         BootSector->AllocationTableCount);

    if (FatVolume->RootDirectoryCluster == 0) {
        FatVolume->RootDirectoryByteOffset = SystemSectorCount * SectorSize;
    }

    SystemSectorCount += RootDirectorySize;
    if (SystemSectorCount >= TotalSectors) {
        Status = STATUS_VOLUME_CORRUPT;
        goto MountEnd;
    }

    DataSectorCount = TotalSectors - SystemSectorCount;
    ClusterCount = (DataSectorCount / BootSector->SectorsPerCluster) +
                   FAT_CLUSTER_BEGIN;

    FatVolume->ClusterCount = ClusterCount;

    //
    // The cluster count alone determines which FAT format is used. According
    // to the spec, these values and the strictly less than are correct and
    // are not to be monkeyed with.
    //

    if (ClusterCount < FAT12_CLUSTER_CUTOFF) {
        FatVolume->Format = Fat12Format;
        FatVolume->ClusterBad = FAT12_CLUSTER_BAD;
        FatVolume->ClusterEnd = FAT12_CLUSTER_END_STAMP;

    } else if (ClusterCount < FAT16_CLUSTER_CUTOFF) {
        FatVolume->Format = Fat16Format;
        FatVolume->ClusterBad = FAT16_CLUSTER_BAD;
        FatVolume->ClusterEnd = FAT16_CLUSTER_END_STAMP;
        FatVolume->ClusterWidthShift = FAT16_CLUSTER_WIDTH_SHIFT;

    } else {
        FatVolume->Format = Fat32Format;
        FatVolume->ClusterBad = FAT32_CLUSTER_BAD;
        FatVolume->ClusterEnd = FAT32_CLUSTER_END_STAMP;
        FatVolume->ClusterWidthShift = FAT32_CLUSTER_WIDTH_SHIFT;
    }

    //
    // The offset to the first cluster (cluster 2) is the reserved sectors plus
    // the size of a FAT times the number of FATs.
    //

    FatVolume->ClusterByteOffset = SystemSectorCount * SectorSize;
    if (FatVolume->ClusterByteOffset == 0) {
        Status = STATUS_VOLUME_CORRUPT;
        goto MountEnd;
    }

    //
    // For FAT32, compute the offset of the root directory.
    //

    if (FatVolume->RootDirectoryCluster != 0) {
        FatVolume->RootDirectoryByteOffset =
                          FAT_CLUSTER_TO_BYTE(FatVolume,
                                              FatVolume->RootDirectoryCluster);
    }

    Status = FatCreateLock(&(FatVolume->Lock));
    if (!KSUCCESS(Status)) {
        goto MountEnd;
    }

    //
    // With the FAT start and end calculated, initialize its cache.
    //

    Status = FatpCreateFatCache(FatVolume);
    if (!KSUCCESS(Status)) {
        goto MountEnd;
    }

    //
    // Read in and validate the FS information block.
    //

    if (FatVolume->InformationByteOffset != 0) {
        InformationBlock = FatVolume->InformationByteOffset >>
                           FatVolume->BlockShift;

        ASSERT((FatVolume->InformationByteOffset %
                FatVolume->Device.BlockSize) == 0);

        InformationIoBuffer = FatAllocateIoBuffer(FatVolume->Device.DeviceToken,
                                                  FatVolume->Device.BlockSize);

        if (InformationIoBuffer == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto MountEnd;
        }

        Status = FatReadDevice(FatVolume->Device.DeviceToken,
                               InformationBlock,
                               1,
                               IO_FLAG_FS_DATA | IO_FLAG_FS_METADATA,
                               NULL,
                               InformationIoBuffer);

        if (!KSUCCESS(Status)) {
            goto MountEnd;
        }

        Information = FatMapIoBuffer(InformationIoBuffer);
        if (Information == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto MountEnd;
        }

        if ((Information->Signature1 != FAT32_SIGNATURE1) ||
            (Information->Signature2 != FAT32_SIGNATURE2) ||
            (Information->BootSignature != FAT_BOOT_SIGNATURE)) {

            Status = STATUS_VOLUME_CORRUPT;
        }

        FatVolume->ClusterSearchStart = Information->LastClusterAllocated;
    }

    Status = STATUS_SUCCESS;

MountEnd:
    if (KSUCCESS(Status)) {
        *VolumeToken = FatVolume;

    } else {
        if (FatVolume != NULL) {
            if (FatVolume->Lock != NULL) {
                FatDestroyLock(FatVolume->Lock);
            }

            FatFreeNonPagedMemory(Device, FatVolume);
        }
    }

    if (BootSectorIoBuffer != NULL) {
        FatFreeIoBuffer(BootSectorIoBuffer);
    }

    if (InformationIoBuffer != NULL) {
        FatFreeIoBuffer(InformationIoBuffer);
    }

    return Status;
}

KSTATUS
FatUnmount (
    PVOID Volume
    )

/*++

Routine Description:

    This routine attempts to unmount a FAT volume.

Arguments:

    Volume - Supplies the token identifying the volume.

Return Value:

    Status code.

--*/

{

    PFAT_VOLUME FatVolume;

    FatVolume = (PFAT_VOLUME)Volume;
    FatpDestroyFatCache(FatVolume);
    FatpDestroyFileMappingTree(FatVolume);
    FatDestroyLock(FatVolume->Lock);
    FatFreeNonPagedMemory(FatVolume->Device.DeviceToken, FatVolume);
    return STATUS_SUCCESS;
}

KSTATUS
FatOpenFileId (
    PVOID Volume,
    FILE_ID FileId,
    ULONG DesiredAccess,
    ULONG Flags,
    PVOID *FileToken
    )

/*++

Routine Description:

    This routine attempts to open an existing file.

Arguments:

    Volume - Supplies the token identifying the volume.

    FileId - Supplies the file ID to open.

    DesiredAccess - Supplies the desired access flags. See IO_ACCESS_*
        definitions.

    Flags - Supplies additional flags about how the file should be opened.
        See OPEN_FLAG_* definitions.

    FileToken - Supplies a pointer where an opaque token will be returned that
        uniquely identifies the opened file instance.

Return Value:

    Status code.

--*/

{

    ULONG CacheDataSize;
    ULONG Cluster;
    ULONG ClusterBad;
    PFAT_FILE FatFile;
    PFAT_VOLUME FatVolume;
    ULONG FirstCluster;
    ULONG PageSize;
    PFAT_IO_BUFFER ScratchIoBuffer;
    PVOID ScratchIoBufferLock;
    KSTATUS Status;

    FatFile = NULL;
    FatVolume = (PFAT_VOLUME)Volume;
    ClusterBad = FatVolume->ClusterBad;
    FirstCluster = (ULONG)FileId;
    ScratchIoBuffer = NULL;
    ScratchIoBufferLock = NULL;
    if ((FirstCluster < FAT_CLUSTER_BEGIN) ||
        (FirstCluster >= FatVolume->ClusterCount)) {

        if (FirstCluster != FatVolume->RootDirectoryCluster) {
            RtlDebugPrint("FAT: Tried to open invalid cluster 0x%I64x "
                          "(total 0x%x)\n",
                          FileId,
                          FatVolume->ClusterCount);

            Status = STATUS_INVALID_PARAMETER;
            goto OpenFileIdEnd;
        }
    }

    //
    // Allocate the FAT file bookkeeping structure, doing some special things
    // for the page file.
    //

    if ((Flags & OPEN_FLAG_PAGE_FILE) != 0) {

        //
        // The the disk byte offset of the clusters must be cached-aligned for
        // the page cache to not interfere with paging. Otherwise the page
        // cache may cache portions of the page file and potentially overwrite
        // valid page file data. For example, if a page file cluster goes from
        // disk offset 0x2400 to 0x33ff, then a normal disk I/O operation to
        // the block at 0x2200 will cache all data from 0x2000 to 0x2fff. When
        // that data gets flushed, it will overwrite the page file data from
        // 0x2400 to 0x2fff.
        //

        CacheDataSize = FatGetIoCacheEntryDataSize();
        if (IS_ALIGNED(FatVolume->ClusterByteOffset, CacheDataSize) == FALSE) {
            RtlDebugPrint("FAT: Page files are not supported on volumes whose "
                          "clusters are not cache-aligned:\n"
                          "\tCluster Byte Offset: 0x%I64x\n"
                          "\tRequired Alignment: 0x%x\n\n",
                          FatVolume->ClusterByteOffset,
                          CacheDataSize);

            Status = STATUS_NOT_SUPPORTED;
            goto OpenFileIdEnd;
        }

        //
        // A page file requires a scratch buffer if the page size is greater
        // than the cluster size or less than then block size. For the former,
        // the buffer is required to window portions of the page into
        // potentially non-contiguous clusters. For the latter, the buffer is
        // required to perform partial block writes. If the current FAT
        // environment returns a page size of 0, then it is unknown. Allocate
        // the buffer to be safe.
        //

        ASSERT(FatVolume->Device.BlockSize <= FatVolume->ClusterSize);

        PageSize = FatGetPageSize();
        if ((PageSize == 0) ||
            (PageSize < FatVolume->Device.BlockSize) ||
            (PageSize > FatVolume->ClusterSize)) {

            ScratchIoBuffer = FatAllocateIoBuffer(FatVolume->Device.DeviceToken,
                                                  FatVolume->Device.BlockSize);

            if (ScratchIoBuffer == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto OpenFileIdEnd;
            }

            Status = FatCreateLock(&ScratchIoBufferLock);
            if (!KSUCCESS(Status)) {
                goto OpenFileIdEnd;
            }
        }

        //
        // Before a page file can be opened for business, all of the FAT
        // entries for its clusters need to be read in.
        //
        // TODO: Lock FAT cache once memory notifications are introduced.
        //

        Cluster = FirstCluster;
        while (Cluster < ClusterBad) {
            Status = FatpGetNextCluster(Volume, 0, Cluster, &Cluster);
            if (!KSUCCESS(Status)) {
                goto OpenFileIdEnd;
            }
        }

        FatFile = FatAllocateNonPagedMemory(FatVolume->Device.DeviceToken,
                                            sizeof(FAT_FILE));

    } else {
        FatFile = FatAllocatePagedMemory(FatVolume->Device.DeviceToken,
                                         sizeof(FAT_FILE));
    }

    if (FatFile == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto OpenFileIdEnd;
    }

    RtlZeroMemory(FatFile, sizeof(FAT_FILE));
    FatFile->Volume = FatVolume;
    FatFile->OpenFlags = Flags;
    FatFile->SeekTable[0] = FirstCluster;
    FatFile->ScratchIoBuffer = ScratchIoBuffer;
    FatFile->ScratchIoBufferLock = ScratchIoBufferLock;

    //
    // If this is the root directory and the root directory is outside the
    // main clusters, mark this file as special.
    //

    if ((FirstCluster < FAT_CLUSTER_BEGIN) &&
        (FirstCluster == FatVolume->RootDirectoryCluster)) {

        FatFile->IsRootDirectory = TRUE;
    }

    *FileToken = FatFile;
    Status = STATUS_SUCCESS;

OpenFileIdEnd:
    if (!KSUCCESS(Status)) {
        if (ScratchIoBuffer != NULL) {
            FatFreeIoBuffer(ScratchIoBuffer);
        }

        if (ScratchIoBufferLock != NULL) {
            FatDestroyLock(ScratchIoBufferLock);
        }

        if (FatFile != NULL) {
            if ((Flags & OPEN_FLAG_PAGE_FILE) != 0) {
                FatFreeNonPagedMemory(FatVolume->Device.DeviceToken, FatFile);

            } else {
                FatFreePagedMemory(FatVolume->Device.DeviceToken, FatFile);
            }
        }
    }

    return Status;
}

VOID
FatCloseFile (
    PVOID FileToken
    )

/*++

Routine Description:

    This routine closes a FAT file.

Arguments:

    FileToken - Supplies the opaque token returned when the file was opened.

Return Value:

    None.

--*/

{

    PFAT_FILE FatFile;

    FatFile = (PFAT_FILE)FileToken;

    ASSERT(FatFile != NULL);

    if (FatFile->ScratchIoBuffer != NULL) {
        FatFreeIoBuffer(FatFile->ScratchIoBuffer);
    }

    if (FatFile->ScratchIoBufferLock != NULL) {
        FatDestroyLock(FatFile->ScratchIoBufferLock);
    }

    if ((FatFile->OpenFlags & OPEN_FLAG_PAGE_FILE) != 0) {
        FatFreeNonPagedMemory(FatFile->Volume->Device.DeviceToken, FatFile);

    } else {
        FatFreePagedMemory(FatFile->Volume->Device.DeviceToken, FatFile);
    }

    return;
}

KSTATUS
FatReadFile (
    PVOID FileToken,
    PFAT_SEEK_INFORMATION FatSeekInformation,
    PFAT_IO_BUFFER IoBuffer,
    UINTN BytesToRead,
    ULONG IoFlags,
    PVOID Irp,
    PUINTN BytesRead
    )

/*++

Routine Description:

    This routine reads the specified number of bytes from an open FAT file,
    updating the seek information.

Arguments:

    FileToken - Supplies the opaque token returned when the file was opened.

    FatSeekInformation - Supplies a pointer to current seek data for the given
        file, indicating where to begin the read.

    IoBuffer - Supplies a pointer to a FAT I/O buffer where the bytes read from
        the file will be returned.

    BytesToRead - Supplies the number of bytes to read from the file.

    IoFlags - Supplies flags regarding the I/O operation. See IO_FLAG_*
        definitions.

    Irp - Supplies an optional pointer to an IRP to use for transfers.

    BytesRead - Supplies the number of bytes that were actually read from the
        file.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if no buffer was passed.

    STATUS_END_OF_FILE if the file end was reached. In this case the remaining
        data in the buffer as well as the bytes read field will be valid.

    STATUS_FILE_CORRUPT if the file system was internally inconsistent.

    Other error codes on device I/O error.

--*/

{

    KSTATUS Status;

    Status = FatpPerformFileIo(FileToken,
                               FALSE,
                               FatSeekInformation,
                               IoBuffer,
                               BytesToRead,
                               IoFlags,
                               Irp,
                               BytesRead);

    return Status;
}

KSTATUS
FatWriteFile (
    PVOID FileToken,
    PFAT_SEEK_INFORMATION FatSeekInformation,
    PFAT_IO_BUFFER IoBuffer,
    UINTN BytesToWrite,
    ULONG IoFlags,
    PVOID Irp,
    PUINTN BytesWritten
    )

/*++

Routine Description:

    This routine writes the specified number of bytes from an open FAT file.

Arguments:

    FileToken - Supplies the opaque token returned when the file was opened.

    FatSeekInformation - Supplies a pointer to current seek data for the given
        file, indicating where to begin the write.

    IoBuffer - Supplies a pointer to a FAT I/O buffer containing the data to
        write to the file.

    BytesToWrite - Supplies the number of bytes to write to the file.

    IoFlags - Supplies flags regarding the I/O operation. See IO_FLAG_*
        definitions.

    Irp - Supplies an optional pointer to an IRP to use for disk transfers.

    BytesWritten - Supplies the number of bytes that were actually written to
        the file.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if no buffer was passed.

    STATUS_END_OF_FILE if the file end was reached. In this case the remaining
        data in the buffer as well as the bytes read field will be valid.

    STATUS_FILE_CORRUPT if the file system was internally inconsistent.

    Other error codes on device I/O error.

--*/

{

    KSTATUS Status;

    Status = FatpPerformFileIo(FileToken,
                               TRUE,
                               FatSeekInformation,
                               IoBuffer,
                               BytesToWrite,
                               IoFlags,
                               Irp,
                               BytesWritten);

    return Status;
}

KSTATUS
FatLookup (
    PVOID Volume,
    BOOL Root,
    FILE_ID DirectoryFileId,
    PCSTR FileName,
    ULONG FileNameSize,
    PFILE_PROPERTIES Properties
    )

/*++

Routine Description:

    This routine attempts to lookup an entry for a file or directory.

Arguments:

    Volume - Supplies the token identifying the volume.

    Root - Supplies a boolean indicating if the system would like to look up
        the root entry for this device. If so, the directory file ID, file name,
        and file name size should be ignored.

    DirectoryFileId - Supplies the file ID of the directory to search in.

    FileName - Supplies a pointer to the name of the file, which may not be
        null terminated.

    FileNameSize - Supplies the size of the file name buffer including space
        for a null terminator (which may be a null terminator or may be a
        garbage character).

    Properties - Supplies the a pointer where the file properties will be
        returned if the file was found.

Return Value:

    Status code.

--*/

{

    ULONG Cluster;
    PFAT_FILE Directory;
    FAT_DIRECTORY_CONTEXT DirectoryContext;
    BOOL DirectoryContextInitialized;
    FAT_ENCODED_PROPERTIES EncodedProperties;
    FAT_DIRECTORY_ENTRY Entry;
    ULONGLONG EntryOffset;
    PFAT_VOLUME FatVolume;
    KSTATUS Status;

    Directory = NULL;
    DirectoryContextInitialized = FALSE;
    FatVolume = (PFAT_VOLUME)Volume;

    //
    // Look up the root directory if requested.
    //

    if (Root != FALSE) {
        Properties->FileId = FatVolume->RootDirectoryCluster;
        Properties->Type = IoObjectRegularDirectory;
        Properties->UserId = 0;
        Properties->GroupId = 0;
        Properties->Permissions = FAT_DEFAULT_FILE_PERMISSIONS;
        Properties->HardLinkCount = 1;
        Properties->Size = 0;
        Properties->BlockSize = FatVolume->ClusterSize;
        Properties->BlockCount = 0;
        FatGetCurrentSystemTime(&(Properties->StatusChangeTime));
        RtlCopyMemory(&(Properties->ModifiedTime),
                      &(Properties->StatusChangeTime),
                      sizeof(SYSTEM_TIME));

        RtlCopyMemory(&(Properties->AccessTime),
                      &(Properties->StatusChangeTime),
                      sizeof(SYSTEM_TIME));

        Status = STATUS_SUCCESS;
        goto LookupEnd;
    }

    //
    // Open the directory.
    //

    Status = FatOpenFileId(Volume,
                           DirectoryFileId,
                           IO_ACCESS_READ,
                           OPEN_FLAG_DIRECTORY,
                           (PVOID)&Directory);

    if (!KSUCCESS(Status)) {
        goto LookupEnd;
    }

    FatpInitializeDirectoryContext(&DirectoryContext, Directory);
    DirectoryContextInitialized = TRUE;

    //
    // Ask for the requested file within the directory.
    //

    Status = FatpLookupDirectoryEntry(FatVolume,
                                      &DirectoryContext,
                                      FileName,
                                      FileNameSize,
                                      &Entry,
                                      &EntryOffset);

    if (!KSUCCESS(Status)) {
        goto LookupEnd;
    }

    ASSERT((DirectoryContext.FatFlags & FAT_DIRECTORY_FLAG_DIRTY) == 0);

    Cluster = (Entry.ClusterHigh << 16) | Entry.ClusterLow;

    //
    // If there is currently no cluster associated with this file, allocate one.
    // This is needed because the cluster is the file ID, so without a cluster
    // it's impossible to uniquely identify this file. This file system library
    // would never leave things like that as a cluster is allocated when the
    // file is created, but other file system implementations might.
    //

    if ((Cluster < FAT_CLUSTER_BEGIN) || (Cluster >= FatVolume->ClusterBad)) {

        ASSERT((FileNameSize != 3) ||
               (RtlAreStringsEqual(FileName, "..", FileNameSize) == FALSE));

        Status = FatpAllocateClusterForEmptyFile(Volume,
                                                 &DirectoryContext,
                                                 DirectoryFileId,
                                                 &Entry,
                                                 EntryOffset);

        if (!KSUCCESS(Status)) {
            goto LookupEnd;
        }

        Cluster = (Entry.ClusterHigh << 16) | Entry.ClusterLow;
    }

    //
    // Convert the directory entry into file properties.
    //

    Properties->FileId = Cluster;
    Properties->Type = IoObjectRegularFile;
    if ((Entry.FileAttributes & FAT_SUBDIRECTORY) != 0) {
        Properties->Type = IoObjectRegularDirectory;
    }

    Properties->UserId = 0;
    Properties->GroupId = 0;
    Properties->Permissions = FAT_DEFAULT_FILE_PERMISSIONS;
    if ((Entry.FileAttributes & FAT_READ_ONLY) != 0) {
        Properties->Permissions &= ~(FILE_PERMISSION_USER_WRITE |
                                     FILE_PERMISSION_GROUP_WRITE |
                                     FILE_PERMISSION_OTHER_WRITE);
    }

    Properties->HardLinkCount = 1;
    Properties->Size = Entry.FileSizeInBytes;
    Properties->BlockSize = FatVolume->ClusterSize;

    ASSERT(POWER_OF_2(Properties->BlockSize) != FALSE);

    Properties->BlockCount =
                ALIGN_RANGE_UP(Entry.FileSizeInBytes, Properties->BlockSize) /
                Properties->BlockSize;

    FatpConvertFatTimeToSystemTime(Entry.CreationDate,
                                   Entry.CreationTime,
                                   Entry.CreationTime10ms,
                                   &(Properties->StatusChangeTime));

    FatpConvertFatTimeToSystemTime(Entry.LastModifiedDate,
                                   Entry.LastModifiedTime,
                                   0,
                                   &(Properties->ModifiedTime));

    FatpConvertFatTimeToSystemTime(Entry.LastAccessDate,
                                   0,
                                   0,
                                   &(Properties->AccessTime));

    //
    // Try to convert the entry to encoded properties. If it appears consistent,
    // use it.
    //

    if (FatDisableEncodedProperties == FALSE) {
        FatpReadEncodedProperties(&Entry, &EncodedProperties);
        if (EncodedProperties.Cluster == Cluster) {
            Properties->UserId = EncodedProperties.Owner;
            Properties->GroupId = EncodedProperties.Group;
            Properties->Permissions = EncodedProperties.Permissions &
                                      FAT_ENCODED_PROPERTY_PERMISSION_MASK;

            if ((EncodedProperties.Permissions &
                 FAT_ENCODED_PROPERTY_SYMLINK) != 0) {

                ASSERT(Properties->Type == IoObjectRegularFile);

                Properties->Type = IoObjectSymbolicLink;
            }

            //
            // Steal the least significant bit of the 10ms creation time to
            // have second-level granularity on modification time.
            //

            Properties->ModifiedTime.Seconds |= Entry.CreationTime10ms & 0x1;
        }
    }

    Status = STATUS_SUCCESS;

LookupEnd:
    if (DirectoryContextInitialized != FALSE) {

        ASSERT(!KSUCCESS(Status) ||
               ((DirectoryContext.FatFlags & FAT_DIRECTORY_FLAG_DIRTY) == 0));

        FatpDestroyDirectoryContext(&DirectoryContext);
    }

    if (Directory != NULL) {
        FatCloseFile(Directory);
    }

    return Status;
}

KSTATUS
FatCreate (
    PVOID Volume,
    FILE_ID DirectoryFileId,
    PCSTR Name,
    ULONG NameSize,
    PULONGLONG DirectorySize,
    PFILE_PROPERTIES Properties
    )

/*++

Routine Description:

    This routine attempts to create a file or directory.

Arguments:

    Volume - Supplies the token identifying the volume.

    DirectoryFileId - Supplies the file ID of the directory to create the file
        in.

    Name - Supplies a pointer to the name of the file or directory to create,
        which may not be null terminated.

    NameSize - Supplies the size of the name buffer including space for a null
        terminator (which may be a null terminator or may be a  garbage
        character).

    DirectorySize - Supplies a pointer that receives the updated size of the
        directory.

    Properties - Supplies the file properties of the created file on success.
        The permissions, object type, user ID, group ID, and access times are
        all valid from the system.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    Status = FatpCreateFile(Volume,
                            DirectoryFileId,
                            Name,
                            NameSize,
                            DirectorySize,
                            Properties);

    return Status;
}

KSTATUS
FatEnumerateDirectory (
    PVOID FileToken,
    ULONGLONG EntryOffset,
    PFAT_IO_BUFFER IoBuffer,
    UINTN BytesToRead,
    BOOL ReadSingleEntry,
    BOOL IncludeDotDirectories,
    PVOID Irp,
    PUINTN BytesRead,
    PULONG ElementsRead
    )

/*++

Routine Description:

    This routine lists the contents of a directory.

Arguments:

    FileToken - Supplies the opaque token returned when the file was opened.

    EntryOffset - Supplies an offset into the directory, in terms of entries,
        where the enumerate should begin.

    IoBuffer - Supplies a pointer to a FAT I/O buffer where the bytes read from
        the file will be returned.

    BytesToRead - Supplies the number of bytes to read from the file.

    ReadSingleEntry - Supplies a boolean indicating if only one entry should
        be read (TRUE) or if as many entries as fit in the buffer should be
        read (FALSE).

    IncludeDotDirectories - Supplies a boolean indicating if the dot and dot
        dot directories should be returned as well.

    Irp - Supplies an optional pointer to an IRP to use for transfers.

    BytesRead - Supplies a pointer that on input contains the number of bytes
        already read from the buffer. On output, accumulates any additional
        bytes read during this function.

    ElementsRead - Supplies the number of directory entries that were
        returned from this read.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if no buffer was passed.

    STATUS_END_OF_FILE if the file end was reached. In this case the remaining
        data in the buffer as well as the bytes read field will be valid.

    STATUS_FILE_CORRUPT if the file system was internally inconsistent.

    Other error codes on device I/O error.

--*/

{

    UINTN BytesWritten;
    ULONG Cluster;
    FAT_DIRECTORY_CONTEXT DirectoryContext;
    FAT_ENCODED_PROPERTIES EncodedProperties;
    ULONG EntriesRead;
    UINTN EntrySize;
    FAT_DIRECTORY_ENTRY FatDirectoryEntry;
    PFAT_FILE File;
    PSTR Name;
    ULONG NameBufferSize;
    ULONG NameSize;
    ULONGLONG ShortEntryOffset;
    UINTN SpaceLeft;
    KSTATUS Status;
    DIRECTORY_ENTRY UserDirectoryEntry;
    PFAT_VOLUME Volume;

    *ElementsRead = 0;
    BytesWritten = *BytesRead;
    File = (PFAT_FILE)FileToken;
    Name = NULL;
    SpaceLeft = BytesToRead - BytesWritten;
    Volume = File->Volume;

    ASSERT(BytesWritten <= BytesToRead);

    //
    // Initialize the directory context to use for reading the directory.
    //

    FatpInitializeDirectoryContext(&DirectoryContext, File);

    //
    // Seek to the desired offset within the directory.
    //

    Status = FatpDirectorySeek(&DirectoryContext, EntryOffset);
    if (!KSUCCESS(Status)) {
        goto EnumerateDirectoryEnd;
    }

    //
    // Allocate a buffer for the name.
    //

    NameBufferSize = FAT_MAX_LONG_FILE_LENGTH + 1;
    Name = FatAllocatePagedMemory(Volume->Device.DeviceToken, NameBufferSize);
    if (Name == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto EnumerateDirectoryEnd;
    }

    //
    // Loop through each directory entry.
    //

    while (TRUE) {
        NameSize = NameBufferSize;
        Status = FatpReadNextDirectoryEntry(&DirectoryContext,
                                            Irp,
                                            Name,
                                            &NameSize,
                                            &FatDirectoryEntry,
                                            &EntriesRead);

        if (!KSUCCESS(Status)) {
            if (Status == STATUS_END_OF_FILE) {
                if (BytesWritten != 0) {
                    Status = STATUS_SUCCESS;
                }

                break;

            } else {
                goto EnumerateDirectoryEnd;
            }
        }

        *ElementsRead += EntriesRead;
        if (IncludeDotDirectories == FALSE) {
            if ((RtlAreStringsEqual(Name, ".", NameSize) != FALSE) ||
                (RtlAreStringsEqual(Name, "..", NameSize) != FALSE)) {

                EntryOffset += EntriesRead;
                continue;
            }
        }

        //
        // If there's not enough room for this directory entry, back up to
        // repeat the entry just read and leave.
        //

        EntrySize = ALIGN_RANGE_UP(sizeof(DIRECTORY_ENTRY) + NameSize, 8);
        if (EntrySize > SpaceLeft) {
            *ElementsRead -= EntriesRead;
            Status = STATUS_MORE_PROCESSING_REQUIRED;
            break;
        }

        //
        // If there is currently no cluster associated with this file, allocate
        // one. This is needed because the cluster is the file ID, so without a
        // cluster it's impossible to uniquely identify this file. This file
        // system library would never leave things like that as a cluster is
        // allocated when the file is created, but other file system
        // implementations might.
        //

        Cluster = (FatDirectoryEntry.ClusterHigh << 16) |
                  FatDirectoryEntry.ClusterLow;

        if ((Cluster < FAT_CLUSTER_BEGIN) || (Cluster >= Volume->ClusterBad)) {

            //
            // The short entry (the one to change) is always the last one just
            // read.
            //

            ShortEntryOffset = EntryOffset + EntriesRead - 1;
            Status = FatpAllocateClusterForEmptyFile(Volume,
                                                     &DirectoryContext,
                                                     File->SeekTable[0],
                                                     &FatDirectoryEntry,
                                                     ShortEntryOffset);

            if (!KSUCCESS(Status)) {
                goto EnumerateDirectoryEnd;
            }

            Cluster = (FatDirectoryEntry.ClusterHigh << 16) |
                      FatDirectoryEntry.ClusterLow;
        }

        EntryOffset += EntriesRead;

        //
        // Write out the directory entry.
        //

        UserDirectoryEntry.Size = EntrySize;
        UserDirectoryEntry.FileId = Cluster;
        UserDirectoryEntry.NextOffset = EntryOffset;
        UserDirectoryEntry.Type = IoObjectRegularFile;
        if ((FatDirectoryEntry.FileAttributes & FAT_SUBDIRECTORY) != 0) {
            UserDirectoryEntry.Type = IoObjectRegularDirectory;

        } else if (FatDisableEncodedProperties == FALSE) {
            FatpReadEncodedProperties(&FatDirectoryEntry, &EncodedProperties);
            if (EncodedProperties.Cluster == Cluster) {
                if ((EncodedProperties.Permissions &
                     FAT_ENCODED_PROPERTY_SYMLINK) != 0) {

                    UserDirectoryEntry.Type = IoObjectSymbolicLink;
                }
            }
        }

        Status = FatCopyIoBufferData(IoBuffer,
                                     &UserDirectoryEntry,
                                     BytesWritten,
                                     sizeof(DIRECTORY_ENTRY),
                                     TRUE);

        if (!KSUCCESS(Status)) {
            goto EnumerateDirectoryEnd;
        }

        ASSERT(Name[NameSize - 1] == '\0');

        Status = FatCopyIoBufferData(IoBuffer,
                                     Name,
                                     BytesWritten + sizeof(DIRECTORY_ENTRY),
                                     NameSize,
                                     TRUE);

        if (!KSUCCESS(Status)) {
            goto EnumerateDirectoryEnd;
        }

        BytesWritten += EntrySize;
        SpaceLeft -= EntrySize;
        if (ReadSingleEntry != FALSE) {
            break;
        }
    }

    *BytesRead = BytesWritten;

EnumerateDirectoryEnd:

    ASSERT((DirectoryContext.FatFlags & FAT_DIRECTORY_FLAG_DIRTY) == 0);

    FatpDestroyDirectoryContext(&DirectoryContext);
    if (Name != NULL) {
        FatFreePagedMemory(Volume->Device.DeviceToken, Name);
    }

    return Status;
}

KSTATUS
FatGetFileDirectory (
    PVOID Volume,
    FILE_ID FileId,
    PFILE_ID DirectoryId
    )

/*++

Routine Description:

    This routine attempts to look up the file ID of the directory that the
    given file is in. The file must have been previously looked up.

Arguments:

    Volume - Supplies the token identifying the volume.

    FileId - Supplies the file ID whose directory is desired.

    DirectoryId - Supplies a pointer where the file ID of the directory
        containing the given file will be returned on success.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NOT_FOUND if no file mapping exists.

--*/

{

    ULONG Cluster;
    ULONG DirectoryCluster;
    ULONGLONG EntryOffset;
    PFAT_VOLUME FatVolume;
    KSTATUS Status;

    Cluster = (ULONG)FileId;
    DirectoryCluster = 0;
    FatVolume = (PFAT_VOLUME)Volume;

    ASSERT((Cluster == FileId) && (Cluster >= FAT_CLUSTER_BEGIN) &&
           (Cluster < FatVolume->ClusterCount));

    Status = FatpGetFileMapping(FatVolume,
                                Cluster,
                                &DirectoryCluster,
                                &EntryOffset);

    *DirectoryId = DirectoryCluster;
    return Status;
}

VOID
FatGetDeviceInformation (
    PVOID Volume,
    PBLOCK_DEVICE_PARAMETERS BlockDeviceParameters
    )

/*++

Routine Description:

    This routine returns a copy of the volume's block device information.

Arguments:

    Volume - Supplies the token identifying the volume.

    BlockDeviceParameters - Supplies a pointer that receives the information
        describing the device backing the volume.

Return Value:

    None.

--*/

{

    PFAT_VOLUME FatVolume;

    ASSERT(BlockDeviceParameters != NULL);
    ASSERT(Volume != NULL);

    FatVolume = (PFAT_VOLUME)Volume;
    *BlockDeviceParameters = FatVolume->Device;
    return;
}

KSTATUS
FatUnlink (
    PVOID Volume,
    FILE_ID DirectoryFileId,
    PCSTR FileName,
    ULONG FileNameSize,
    FILE_ID FileId,
    PBOOL Unlinked
    )

/*++

Routine Description:

    This routine deletes a file entry from a directory. It does not free the
    clusters associated with the file. The call must hold the queued lock for
    both the directory and the file.

Arguments:

    Volume - Supplies the token identifying the volume.

    DirectoryFileId - Supplies the ID of the directory containing the file.

    FileName - Supplies the name of the file to unlink.

    FileNameSize - Supplies the length of the file name buffer in bytes,
        including the null terminator.

    FileId - Supplies the file ID of the file to unlink.

    Unlinked - Supplies a pointer that receives a boolean indicating whether or
        not the file entry was unlinked. This can get set to TRUE even if the
        routine returns a failure status.

Return Value:

    Status code.

--*/

{

    ULONG Cluster;
    PVOID Directory;
    FAT_DIRECTORY_CONTEXT DirectoryContext;
    BOOL DirectoryContextInitialized;
    BOOL DirectoryEmpty;
    FAT_DIRECTORY_ENTRY DirectoryEntry;
    ULONGLONG EntryOffset;
    KSTATUS Status;

    DirectoryContextInitialized = FALSE;
    *Unlinked = FALSE;

    //
    // Start by opening up the directory.
    //

    Status = FatOpenFileId(Volume,
                           DirectoryFileId,
                           IO_ACCESS_READ | IO_ACCESS_WRITE,
                           OPEN_FLAG_DIRECTORY,
                           &Directory);

    if (!KSUCCESS(Status)) {
        goto UnlinkEnd;
    }

    FatpInitializeDirectoryContext(&DirectoryContext, Directory);
    DirectoryContextInitialized = TRUE;

    //
    // Look up the file.
    //

    Status = FatpLookupDirectoryEntry(Volume,
                                      &DirectoryContext,
                                      FileName,
                                      FileNameSize,
                                      &DirectoryEntry,
                                      &EntryOffset);

    if (!KSUCCESS(Status)) {
        goto UnlinkEnd;
    }

    Cluster = ((ULONG)(DirectoryEntry.ClusterHigh) << 16) |
              DirectoryEntry.ClusterLow;

    if (Cluster != FileId) {
        Status = STATUS_NO_SUCH_FILE;
        goto UnlinkEnd;
    }

    //
    // If the entry to delete is a directory itself, verify that the directory
    // is empty.
    //

    if ((DirectoryEntry.FileAttributes & FAT_SUBDIRECTORY) != 0) {
        Status = FatpIsDirectoryEmpty(Volume, FileId, &DirectoryEmpty);
        if (!KSUCCESS(Status)) {
            goto UnlinkEnd;
        }

        if (DirectoryEmpty == FALSE) {
            Status = STATUS_DIRECTORY_NOT_EMPTY;
            goto UnlinkEnd;
        }
    }

    //
    // Remove the directory entry, wiping out the link to the file but not any
    // clusters within the file.
    //

    Status = FatpEraseDirectoryEntry(&DirectoryContext, EntryOffset, Unlinked);
    if (!KSUCCESS(Status)) {
        goto UnlinkEnd;
    }

    Status = STATUS_SUCCESS;

UnlinkEnd:
    if (DirectoryContextInitialized != FALSE) {

        ASSERT((*Unlinked == FALSE) ||
               ((DirectoryContext.FatFlags & FAT_DIRECTORY_FLAG_DIRTY) == 0));

        ASSERT(!KSUCCESS(Status) ||
               ((DirectoryContext.FatFlags & FAT_DIRECTORY_FLAG_DIRTY) == 0));

        FatpDestroyDirectoryContext(&DirectoryContext);
    }

    if (Directory != NULL) {
        FatCloseFile(Directory);
    }

    return Status;
}

KSTATUS
FatRename (
    PVOID Volume,
    FILE_ID SourceDirectoryId,
    FILE_ID SourceFileId,
    PBOOL SourceErased,
    FILE_ID DestinationDirectoryId,
    PBOOL DestinationCreated,
    PULONGLONG DestinationDirectorySize,
    PSTR FileName,
    ULONG FileNameSize
    )

/*++

Routine Description:

    This routine attempts to rename a file. The destination file must not
    already exist, otherwise multiple files will share the same name in a
    directory.

Arguments:

    Volume - Supplies the token identifying the volume.

    SourceDirectoryId - Supplies the file ID of the directory containing the
        source file.

    SourceFileId - Supplies the file ID of the file to rename.

    SourceErased - Supplies a pointer that receives a boolean indicating
        whether or not the source file was erased from its parent directory.

    DestinationDirectoryId - Supplies the file ID of the directory where the
        newly renamed file will reside.

    DestinationCreated - Supplies a pointer that receives a boolean indicating
        whether or not a directory entry was created for the destination file.

    DestinationDirectorySize - Supplies a pointer that receives the updated
        size of the directory.

    FileName - Supplies the name of the newly renamed file.

    FileNameSize - Supplies the length of the file name buffer in bytes,
        including the null terminator.

Return Value:

    Status code.

--*/

{

    ULONG DestinationDirectoryCluster;
    ULONG EntriesRead;
    FAT_DIRECTORY_ENTRY Entry;
    PFAT_VOLUME FatVolume;
    PVOID SourceDirectory;
    ULONG SourceDirectoryCluster;
    FAT_DIRECTORY_CONTEXT SourceDirectoryContext;
    BOOL SourceDirectoryContextInitialized;
    ULONGLONG SourceDirectoryOffset;
    KSTATUS Status;

    *DestinationCreated = FALSE;
    FatVolume = (PFAT_VOLUME)Volume;
    SourceDirectory = NULL;
    SourceDirectoryContextInitialized = FALSE;
    *SourceErased = FALSE;

    //
    // Figure out where this file resides in the source directory.
    //

    Status = FatpGetFileMapping(FatVolume,
                                (ULONG)SourceFileId,
                                &SourceDirectoryCluster,
                                &SourceDirectoryOffset);

    if (!KSUCCESS(Status)) {
        goto RenameEnd;
    }

    //
    // Read the short source directory entry in.
    //

    ASSERT(SourceDirectoryCluster == SourceDirectoryId);

    Status = FatOpenFileId(Volume,
                           SourceDirectoryId,
                           IO_ACCESS_READ | IO_ACCESS_WRITE,
                           OPEN_FLAG_DIRECTORY,
                           &SourceDirectory);

    if (!KSUCCESS(Status)) {
        goto RenameEnd;
    }

    FatpInitializeDirectoryContext(&SourceDirectoryContext, SourceDirectory);
    SourceDirectoryContextInitialized = TRUE;
    Status = FatpDirectorySeek(&SourceDirectoryContext, SourceDirectoryOffset);
    if (!KSUCCESS(Status)) {
        goto RenameEnd;
    }

    Status = FatpReadDirectory(&SourceDirectoryContext,
                               &Entry,
                               1,
                               &EntriesRead);

    if (!KSUCCESS(Status)) {
        goto RenameEnd;
    }

    if (EntriesRead != 1) {
        Status = STATUS_FILE_CORRUPT;
        goto RenameEnd;
    }

    ASSERT((SourceDirectoryContext.FatFlags & FAT_DIRECTORY_FLAG_DIRTY) == 0);
    ASSERT((((ULONG)(Entry.ClusterHigh) << 16) | Entry.ClusterLow) ==
           SourceFileId);

    //
    // Remove the directory entry at the old location.
    //

    Status = FatpEraseDirectoryEntry(&SourceDirectoryContext,
                                     SourceDirectoryOffset,
                                     SourceErased);

    if (!KSUCCESS(Status)) {
        goto RenameEnd;
    }

    //
    // The erase entry routine should have handled any flushing for the
    // directory context. And since the destination directory may be the same
    // as the source, all the writes better be flushed out.
    //

    ASSERT((SourceDirectoryContext.FatFlags & FAT_DIRECTORY_FLAG_DIRTY) == 0);
    ASSERT(*SourceErased != FALSE);

    FatpDestroyDirectoryContext(&SourceDirectoryContext);
    SourceDirectoryContextInitialized = FALSE;

    //
    // If the file that was just erased is a directory, change its dot dot
    // entry to point at the new location.
    //

    if (((Entry.FileAttributes & FAT_VOLUME_LABEL) == 0) &&
        ((Entry.FileAttributes & FAT_SUBDIRECTORY) != 0)) {

        DestinationDirectoryCluster = (ULONG)DestinationDirectoryId;

        ASSERT(DestinationDirectoryCluster == DestinationDirectoryId);

        Status = FatpFixupDotDot(Volume,
                                 SourceFileId,
                                 DestinationDirectoryCluster);

        if (!KSUCCESS(Status)) {
            goto RenameEnd;
        }
    }

    //
    // Create a new directory entry at the destination.
    //

    Status = FatpCreateDirectoryEntry(FatVolume,
                                      DestinationDirectoryId,
                                      FileName,
                                      FileNameSize,
                                      DestinationDirectorySize,
                                      &Entry);

    if (!KSUCCESS(Status)) {
        goto RenameEnd;
    }

    *DestinationCreated = TRUE;
    Status = STATUS_SUCCESS;

RenameEnd:
    if (SourceDirectoryContextInitialized != FALSE) {
        FatpDestroyDirectoryContext(&SourceDirectoryContext);
    }

    if (SourceDirectory != NULL) {
        FatCloseFile(SourceDirectory);
    }

    return Status;
}

KSTATUS
FatTruncate (
    PVOID Volume,
    PVOID FileToken,
    FILE_ID FileId,
    ULONGLONG OldSize,
    ULONGLONG NewSize
    )

/*++

Routine Description:

    This routine truncates a file to the given file size. This can be used to
    both shrink and grow the file.

Arguments:

    Volume - Supplies a pointer to the volume.

    FileToken - Supplies the file context of the file to operate on.

    FileId - Supplies the file ID of the file to operate on.

    OldSize - Supplies the original size of the file.

    NewSize - Supplies the new size to make the file. If smaller, then
        unused clusters will be freed. If larger, then the file will be
        zeroed to make room.

Return Value:

    Status code.

--*/

{

    UINTN BytesThisRound;
    UINTN BytesWritten;
    ULONG ClusterSize;
    PFAT_VOLUME FatVolume;
    FAT_SEEK_INFORMATION Seek;
    KSTATUS Status;
    PFAT_IO_BUFFER ZeroBuffer;

    FatVolume = Volume;
    ClusterSize = FatVolume->ClusterSize;
    ZeroBuffer = NULL;
    if (NewSize < OldSize) {
        return FatDeleteFileBlocks(Volume, FileToken, FileId, NewSize, TRUE);
    }

    //
    // Create a cluster sized buffer full of zeros.
    //

    ZeroBuffer = FatAllocateIoBuffer(FatVolume->Device.DeviceToken,
                                     ClusterSize);

    if (ZeroBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto TruncateEnd;
    }

    Status = FatZeroIoBuffer(ZeroBuffer, 0, ClusterSize);
    if (!KSUCCESS(Status)) {
        goto TruncateEnd;
    }

    //
    // It's time to grow the file. Seek to the old end of the file.
    //

    RtlZeroMemory(&Seek, sizeof(FAT_SEEK_INFORMATION));
    Status = FatFileSeek(FileToken,
                         NULL,
                         0,
                         SeekCommandFromBeginning,
                         OldSize,
                         &Seek);

    if (!KSUCCESS(Status)) {
        goto TruncateEnd;
    }

    while (OldSize < NewSize) {
        if (!IS_ALIGNED(OldSize, ClusterSize)) {
            BytesThisRound = ClusterSize - REMAINDER(OldSize, ClusterSize);

        } else {
            BytesThisRound = ClusterSize;
        }

        if (OldSize + BytesThisRound > NewSize) {
            BytesThisRound = NewSize - OldSize;
        }

        Status = FatWriteFile(FileToken,
                              &Seek,
                              ZeroBuffer,
                              BytesThisRound,
                              0,
                              NULL,
                              &BytesWritten);

        if (!KSUCCESS(Status)) {
            goto TruncateEnd;
        }

        ASSERT(BytesWritten != 0);

        OldSize += BytesWritten;
    }

    Status = STATUS_SUCCESS;

TruncateEnd:
    if (ZeroBuffer != NULL) {
        FatFreeIoBuffer(ZeroBuffer);
    }

    return Status;
}

KSTATUS
FatFileSeek (
    PVOID FileToken,
    PVOID Irp,
    ULONG IoFlags,
    SEEK_COMMAND SeekCommand,
    ULONGLONG Offset,
    PFAT_SEEK_INFORMATION FatSeekInformation
    )

/*++

Routine Description:

    This routine returns a set of seek data for the given file.

Arguments:

    FileToken - Supplies the opaque token returned when the file was opened.

    Irp - Supplies an optional pointer to an IRP to use for disk operations.

    IoFlags - Supplies flags regarding the I/O operation. See IO_FLAG_*
        definitions.

    SeekCommand - Supplies the type of seek to perform.

    Offset - Supplies the offset, in bytes, to seek to, from the reference
        location specified by the seek command.

    FatSeekInformation - Supplies a pointer that receives the information
        collected by the seek operation, including the new cluster and byte
        offsets.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if an unrecognized seek option is passed.

    STATUS_OUT_OF_BOUNDS if an offset was supplied that exceeds the maximum
    possible file size.

    STATUS_FILE_CORRUPT if the file system has a problem.

    Other errors on device I/O error.

--*/

{

    ULONG BlockShift;
    ULONG BlockSize;
    ULONGLONG ByteOffset;
    ULONGLONG ClusterAlignedDestination;
    ULONG ClusterBad;
    ULONGLONG ClusterEnd;
    ULONG ClusterSize;
    ULONGLONG ClusterStart;
    ULONG CurrentCluster;
    ULONGLONG CurrentOffset;
    ULONG CurrentWindowIndex;
    ULONGLONG DestinationOffset;
    ULONGLONG DiskByteOffset;
    PFAT_FILE File;
    ULONGLONG FileByteOffset;
    ULONG PreviousCluster;
    ULONG PreviousTableIndex;
    KSTATUS Status;
    ULONG TableIndex;
    PFAT_VOLUME Volume;
    PVOID Window;
    ULONG WindowIndex;
    ULONG WindowOffset;

    File = (PFAT_FILE)FileToken;
    Volume = File->Volume;
    BlockShift = Volume->BlockShift;
    BlockSize = Volume->Device.BlockSize;
    ClusterBad = Volume->ClusterBad;
    ClusterSize = Volume->ClusterSize;
    FileByteOffset = FatSeekInformation->FileByteOffset;
    Status = STATUS_SUCCESS;

    //
    // If it's a directory file, then the seek offset is in terms of directory
    // blocks, not file bytes.
    //

    if ((File->OpenFlags & OPEN_FLAG_DIRECTORY) != 0) {
        Offset *= sizeof(FAT_DIRECTORY_ENTRY);
    }

    //
    // Determine the absolute offset to seek to.
    //

    switch (SeekCommand) {
    case SeekCommandNop:
        Status = STATUS_SUCCESS;
        goto FatFileSeekEnd;

    case SeekCommandFromBeginning:
        if ((File->OpenFlags & OPEN_FLAG_DIRECTORY) != 0) {

            ASSERT(Offset >=
                   (DIRECTORY_CONTENTS_OFFSET * sizeof(FAT_DIRECTORY_ENTRY)));

            Offset -= DIRECTORY_CONTENTS_OFFSET * sizeof(FAT_DIRECTORY_ENTRY);
        }

        DestinationOffset = Offset;
        break;

    case SeekCommandFromCurrentOffset:
        DestinationOffset = FileByteOffset + Offset;
        break;

    case SeekCommandFromEnd:

        ASSERT(FALSE);

        Status = STATUS_NOT_IMPLEMENTED;
        goto FatFileSeekEnd;

    default:

        ASSERT(FALSE);

        Status = STATUS_INVALID_PARAMETER;
        goto FatFileSeekEnd;
    }

    if (DestinationOffset > MAX_ULONG) {
        Status = STATUS_OUT_OF_BOUNDS;
        goto FatFileSeekEnd;
    }

    if (File->IsRootDirectory != FALSE) {

        ASSERT(DestinationOffset <=
               File->Volume->RootDirectoryCount * sizeof(FAT_DIRECTORY_ENTRY));

        FatSeekInformation->FileByteOffset = DestinationOffset;
        FatSeekInformation->CurrentBlock =
                        (File->Volume->RootDirectoryByteOffset +
                         (ULONG)DestinationOffset) >> File->Volume->BlockShift;

        FatSeekInformation->CurrentCluster = 0;
        FatSeekInformation->ClusterByteOffset = 0;
        Status = STATUS_SUCCESS;
        goto FatFileSeekEnd;
    }

    //
    // If the current location is the offset, no action is needed unless this
    // is a seek to the beginning and the current cluster is not set.
    //

    if (DestinationOffset == FileByteOffset) {
        if ((DestinationOffset == 0) &&
            (FatSeekInformation->CurrentCluster == 0)) {

            //
            // If the file is completely empty then a seek to zero is at the
            // end of the file.
            //

            if ((File->SeekTable[0] == FAT_CLUSTER_FREE) ||
                (File->SeekTable[0] >= Volume->ClusterCount)) {

                Status = STATUS_END_OF_FILE;
                goto FatFileSeekEnd;
            }

            FatSeekInformation->CurrentCluster = File->SeekTable[0];
            ByteOffset = FAT_CLUSTER_TO_BYTE(File->Volume, File->SeekTable[0]);
            FatSeekInformation->CurrentBlock = ByteOffset >> BlockShift;

            ASSERT(IS_ALIGNED(ByteOffset, BlockSize) != FALSE);

            FatSeekInformation->ClusterByteOffset = 0;
        }

        Status = STATUS_SUCCESS;
        goto FatFileSeekEnd;
    }

    //
    // If the desired location is within the current cluster, then just move
    // to that location. Don't do this if there is not a valid cluster in the
    // seek information.
    //

    ClusterStart = FileByteOffset - FatSeekInformation->ClusterByteOffset;
    ClusterEnd = ClusterStart + ClusterSize;
    CurrentCluster = FatSeekInformation->CurrentCluster;
    if (((CurrentCluster >= FAT_CLUSTER_BEGIN) &&
         (CurrentCluster < Volume->ClusterCount)) &&
        ((DestinationOffset >= ClusterStart) &&
         (DestinationOffset < ClusterEnd))) {

        DiskByteOffset = FAT_CLUSTER_TO_BYTE(Volume, CurrentCluster);
        FatSeekInformation->ClusterByteOffset = DestinationOffset -
                                                ClusterStart;

        DiskByteOffset += FatSeekInformation->ClusterByteOffset;
        FatSeekInformation->CurrentBlock = DiskByteOffset >> BlockShift;
        FatSeekInformation->FileByteOffset = DestinationOffset;
        Status = STATUS_SUCCESS;
        goto FatFileSeekEnd;
    }

    //
    // Get the nearest seek table index, and march down until a seek table
    // entry is filled in. The first one is guaranteed to be filled in.
    //

    ASSERT(File->SeekTable[0] != 0);

    TableIndex = FAT_SEEK_TABLE_INDEX(DestinationOffset);
    while (File->SeekTable[TableIndex] == 0) {
        TableIndex -= 1;
    }

    CurrentOffset = FAT_SEEK_TABLE_OFFSET(TableIndex);
    CurrentCluster = File->SeekTable[TableIndex];

    ASSERT((CurrentCluster >= FAT_CLUSTER_BEGIN) &&
           (CurrentCluster < Volume->ClusterCount));

    //
    // As an optimization, if the current offset is below the destination and
    // closer than this offset, use it.
    //

    if ((ALIGN_RANGE_DOWN(FileByteOffset, ClusterSize) <= DestinationOffset) &&
        (FileByteOffset > CurrentOffset)) {

        CurrentOffset = ALIGN_RANGE_DOWN(FileByteOffset, ClusterSize);
        if (FatSeekInformation->ClusterByteOffset == ClusterSize) {

            ASSERT(CurrentOffset >= ClusterSize);

            CurrentOffset -= ClusterSize;
        }

        CurrentCluster = FatSeekInformation->CurrentCluster;
    }

    //
    // Cruise the singly linked list of clusters.
    //

    ClusterAlignedDestination = ALIGN_RANGE_DOWN(DestinationOffset,
                                                 ClusterSize);

    PreviousCluster = CurrentCluster;
    PreviousTableIndex = TableIndex;
    CurrentWindowIndex = MAX_ULONG;
    while (CurrentOffset < ClusterAlignedDestination) {

        //
        // Read the window if necessary. Chances are the previous window is
        // still the correct one.
        //

        WindowIndex = FAT_WINDOW_INDEX(Volume, CurrentCluster);
        if (WindowIndex != CurrentWindowIndex) {
            Status = FatpFatCacheGetFatWindow(Volume,
                                              FALSE,
                                              CurrentCluster,
                                              &Window,
                                              &WindowOffset);

            if (!KSUCCESS(Status)) {
                goto FatFileSeekEnd;
            }

            CurrentWindowIndex = WindowIndex;

        } else if (Volume->Format != Fat12Format) {
            WindowOffset = CurrentCluster -
                           FAT_WINDOW_INDEX_TO_CLUSTER(Volume, WindowIndex);
        }

        //
        // Get the next cluster.
        //

        if (Volume->Format == Fat12Format) {
            CurrentCluster = FAT12_READ_CLUSTER(Window, CurrentCluster);

        } else if (Volume->Format == Fat16Format) {
            CurrentCluster = ((PUSHORT)Window)[WindowOffset];

        } else {
            CurrentCluster = ((PULONG)Window)[WindowOffset];
        }

        CurrentOffset += ClusterSize;

        //
        // Stop if the end of the file was hit.
        //

        if ((CurrentCluster < FAT_CLUSTER_BEGIN) ||
            (CurrentCluster >= ClusterBad)) {

            if (CurrentOffset == ClusterAlignedDestination) {
                Status = STATUS_SUCCESS;

            } else {
                Status = STATUS_END_OF_FILE;
            }

            DiskByteOffset = FAT_CLUSTER_TO_BYTE(Volume, PreviousCluster);

            //
            // Tip it just over the line so just before the first I/O operation
            // it needs to fetch the next cluster.
            //

            FatSeekInformation->CurrentBlock = DiskByteOffset >> BlockShift;
            FatSeekInformation->ClusterByteOffset = ClusterSize;
            FatSeekInformation->CurrentCluster = PreviousCluster;
            FatSeekInformation->FileByteOffset = CurrentOffset;
            goto FatFileSeekEnd;
        }

        //
        // Populate the seek table.
        //

        TableIndex = FAT_SEEK_TABLE_INDEX(CurrentOffset);
        if (TableIndex != PreviousTableIndex) {
            if ((CurrentOffset & FAT_SEEK_OFFSET_MASK) == 0) {

                ASSERT((File->SeekTable[TableIndex] == 0) ||
                       (File->SeekTable[TableIndex] == CurrentCluster));

                ASSERT(CurrentCluster != 0);

                File->SeekTable[TableIndex] = CurrentCluster;
            }

            PreviousTableIndex = TableIndex;
        }

        PreviousCluster = CurrentCluster;
    }

    //
    // Calculate the cluster and byte offsets from the cluster and return
    // success.
    //

    DiskByteOffset = FAT_CLUSTER_TO_BYTE(Volume, CurrentCluster);
    FatSeekInformation->ClusterByteOffset = DestinationOffset -
                                            ClusterAlignedDestination;

    DiskByteOffset += FatSeekInformation->ClusterByteOffset;
    FatSeekInformation->CurrentBlock = DiskByteOffset >> BlockShift;
    FatSeekInformation->CurrentCluster = CurrentCluster;
    FatSeekInformation->FileByteOffset = DestinationOffset;

FatFileSeekEnd:
    return Status;
}

KSTATUS
FatWriteFileProperties (
    PVOID Volume,
    PFILE_PROPERTIES NewProperties,
    ULONG IoFlags
    )

/*++

Routine Description:

    This routine updates the metadata (located in the directory entry) for the
    given file.

Arguments:

    Volume - Supplies a pointer to the FAT volume.

    NewProperties - Supplies a pointer to the new file metadata.

    IoFlags - Supplies flags regarding the I/O operation. See IO_FLAG_*
        definitions.

Return Value:

    Status code.

--*/

{

    UCHAR Checksum;
    ULONG Cluster;
    PFAT_FILE Directory;
    ULONG DirectoryCluster;
    FAT_DIRECTORY_CONTEXT DirectoryContext;
    BOOL DirectoryContextInitialized;
    FAT_DIRECTORY_ENTRY DirectoryEntry;
    FAT_ENCODED_PROPERTIES EncodedProperties;
    ULONG EntriesRead;
    ULONG EntriesWritten;
    ULONGLONG EntryOffset;
    PFAT_VOLUME FatVolume;
    ULONGLONG FileSize;
    UCHAR NewChecksum;
    ULONG ReadCluster;
    KSTATUS Status;

    Directory = NULL;
    DirectoryContextInitialized = FALSE;
    Cluster = (ULONG)(NewProperties->FileId);
    FatVolume = (PFAT_VOLUME)Volume;

    //
    // If this is the root directory, its properties cannot be updated, so
    // just ignore it.
    //

    if (Cluster == FatVolume->RootDirectoryCluster) {
        Status = STATUS_SUCCESS;
        goto FatWriteFilePropertiesEnd;
    }

    ASSERT((Cluster == (ULONG)(NewProperties->FileId)) &&
           (Cluster >= FAT_CLUSTER_BEGIN) &&
           (Cluster < FatVolume->ClusterCount));

    //
    // Look up the directory.
    //

    Status = FatpGetFileMapping(FatVolume,
                                Cluster,
                                &DirectoryCluster,
                                &EntryOffset);

    if (!KSUCCESS(Status)) {
        goto FatWriteFilePropertiesEnd;
    }

    //
    // Open the directory and read the entry.
    //

    Status = FatOpenFileId(Volume,
                           DirectoryCluster,
                           IO_ACCESS_READ,
                           OPEN_FLAG_DIRECTORY,
                           (PVOID)&Directory);

    if (!KSUCCESS(Status)) {
        goto FatWriteFilePropertiesEnd;
    }

    FatpInitializeDirectoryContext(&DirectoryContext, Directory);
    DirectoryContext.IoFlags = IoFlags;
    DirectoryContextInitialized = TRUE;
    Status = FatpDirectorySeek(&DirectoryContext, EntryOffset);
    if (!KSUCCESS(Status)) {
        goto FatWriteFilePropertiesEnd;
    }

    Status = FatpReadDirectory(&DirectoryContext,
                               &DirectoryEntry,
                               1,
                               &EntriesRead);

    if (!KSUCCESS(Status)) {
        goto FatWriteFilePropertiesEnd;
    }

    if (EntriesRead != 1) {
        Status = STATUS_END_OF_FILE;
        goto FatWriteFilePropertiesEnd;
    }

    ASSERT(DirectoryEntry.DosName[0] != FAT_DIRECTORY_ENTRY_ERASED);

    //
    // Grab the original directory entry for the update.
    //

    Checksum = FatpChecksumDirectoryEntry(&DirectoryEntry);
    NewChecksum = Checksum;

    //
    // Update the directory entry with the given file properties.
    //

    ReadCluster = (DirectoryEntry.ClusterHigh << 16) |
                  DirectoryEntry.ClusterLow;

    ASSERT(ReadCluster == Cluster);
    ASSERT((NewProperties->Type == IoObjectRegularFile) ||
           (NewProperties->Type == IoObjectSymbolicLink) ||
           ((DirectoryEntry.FileAttributes & FAT_SUBDIRECTORY) != 0));

    if ((DirectoryEntry.FileAttributes & FAT_SUBDIRECTORY) == 0) {
        FileSize = NewProperties->Size;
        if (FileSize > MAX_ULONG) {
            DirectoryEntry.FileSizeInBytes = MAX_ULONG;

        } else {
            DirectoryEntry.FileSizeInBytes = (ULONG)FileSize;
        }
    }

    FatpConvertSystemTimeToFatTime(&(NewProperties->ModifiedTime),
                                   &(DirectoryEntry.LastModifiedDate),
                                   &(DirectoryEntry.LastModifiedTime),
                                   NULL);

    FatpConvertSystemTimeToFatTime(&(NewProperties->AccessTime),
                                   &(DirectoryEntry.LastAccessDate),
                                   NULL,
                                   NULL);

    if ((NewProperties->Permissions &
         (FILE_PERMISSION_USER_WRITE | FILE_PERMISSION_GROUP_WRITE |
          FILE_PERMISSION_OTHER_WRITE)) == 0) {

        DirectoryEntry.FileAttributes |= FAT_READ_ONLY;

    } else {
        DirectoryEntry.FileAttributes &= ~FAT_READ_ONLY;
    }

    //
    // If already in this form, update the encoded properties in the short
    // name.
    //

    if (FatDisableEncodedProperties == FALSE) {
        FatpReadEncodedProperties(&DirectoryEntry, &EncodedProperties);
        if (EncodedProperties.Cluster == Cluster) {
            if ((FatPrintTruncatedUserIds != FALSE) &&
                (((NewProperties->UserId & ~MAX_USHORT) != 0) ||
                 ((NewProperties->GroupId & ~MAX_USHORT) != 0))) {

                RtlDebugPrint("FAT: Truncated UID/GID: FILE_PROPERTIES 0x%x "
                              "(ID 0x%I64x UID 0x%x GID 0x%x)\n",
                              NewProperties,
                              NewProperties->FileId,
                              NewProperties->UserId,
                              NewProperties->GroupId);
            }

            EncodedProperties.Owner = NewProperties->UserId & MAX_USHORT;
            EncodedProperties.Group = NewProperties->GroupId & MAX_USHORT;
            EncodedProperties.Permissions =
                                         NewProperties->Permissions &
                                         FAT_ENCODED_PROPERTY_PERMISSION_MASK;

            if (NewProperties->Type == IoObjectSymbolicLink) {
                EncodedProperties.Permissions |= FAT_ENCODED_PROPERTY_SYMLINK;
            }

            //
            // Use the least significant bit of the creation time 10ms as the
            // ones bit for modification time.
            //

            DirectoryEntry.CreationTime10ms &= ~0x1;
            DirectoryEntry.CreationTime10ms |=
                                     NewProperties->ModifiedTime.Seconds & 0x1;

            FatpWriteEncodedProperties(&DirectoryEntry, &EncodedProperties);
            NewChecksum = FatpChecksumDirectoryEntry(&DirectoryEntry);
        }

    } else {

        ASSERT(NewProperties->Type != IoObjectSymbolicLink);
    }

    //
    // Write the updated directory entry back in.
    //

    Status = FatpDirectorySeek(&DirectoryContext, EntryOffset);
    if (!KSUCCESS(Status)) {
        goto FatWriteFilePropertiesEnd;
    }

    Status = FatpWriteDirectory(&DirectoryContext,
                                &DirectoryEntry,
                                1,
                                &EntriesWritten);

    if (!KSUCCESS(Status)) {
        goto FatWriteFilePropertiesEnd;
    }

    if (EntriesWritten != 1) {
        Status = STATUS_END_OF_FILE;
        goto FatWriteFilePropertiesEnd;
    }

    //
    // If the short file name changed due to encoded property changes, update
    // the checksums in the long file name entries.
    //

    if (Checksum != NewChecksum) {
        Status = FatpPerformLongEntryMaintenance(&DirectoryContext,
                                                 EntryOffset,
                                                 Checksum,
                                                 NewChecksum);

        if (!KSUCCESS(Status)) {
            goto FatWriteFilePropertiesEnd;
        }
    }

    Status = FatpFlushDirectory(&DirectoryContext);
    if (!KSUCCESS(Status)) {
        goto FatWriteFilePropertiesEnd;
    }

    Status = STATUS_SUCCESS;

FatWriteFilePropertiesEnd:
    if (DirectoryContextInitialized != FALSE) {

        ASSERT(!KSUCCESS(Status) ||
               ((DirectoryContext.FatFlags & FAT_DIRECTORY_FLAG_DIRTY) == 0));

        FatpDestroyDirectoryContext(&DirectoryContext);
    }

    if (Directory != NULL) {
        FatCloseFile(Directory);
    }

    return Status;
}

KSTATUS
FatDeleteFileBlocks (
    PVOID Volume,
    PVOID FileToken,
    FILE_ID FileId,
    ULONGLONG FileSize,
    BOOL Truncate
    )

/*++

Routine Description:

    This routine deletes the data contents of a file beyond the specified file
    size, freeing its corresponding clusters. It does not touch the file's
    directory entry.

Arguments:

    Volume - Supplies a pointer to the FAT volume.

    FileToken - Supplies an optional pointer to an open file token for the
        file. If this is set, then the seek table for the file will be
        maintained. If no file is supplied or there are other open files, the
        seek table for those files will be wrong and may cause volume
        corruption.

    FileId - Supplies the ID of the file whose contents should be deleted or
        truncated.

    FileSize - Supplies the file size beyond which all data is to be discarded.

    Truncate - Supplies a boolean that if set to TRUE will leave the first
        cluster (the file ID) allocated. This allows a file to be truncated to
        zero size but still maintain its file ID. If FALSE, all file blocks,
        including the first one, will be deleted.

Return Value:

    Status code.

--*/

{

    ULONG ClusterCount;
    BOOL DirtyFat;
    PFAT_VOLUME FatVolume;
    PFAT_FILE File;
    KSTATUS FlushStatus;
    ULONG NextCluster;
    ULONG StartingCluster;
    KSTATUS Status;
    ULONG TableIndex;
    BOOL VolumeLockHeld;

    ASSERT((Truncate != FALSE) || (FileSize == 0));

    DirtyFat = FALSE;
    FatVolume = (PFAT_VOLUME)Volume;
    File = (PFAT_FILE)FileToken;
    ClusterCount = FatVolume->ClusterCount;
    StartingCluster = (ULONG)FileId;
    VolumeLockHeld = FALSE;

    ASSERT((StartingCluster == FileId) &&
           (StartingCluster >= FAT_CLUSTER_BEGIN) &&
           (StartingCluster < FatVolume->ClusterCount));

    if (Truncate != FALSE) {

        //
        // If this is not a truncate to zero, then find the last cluster that
        // will remain in the file and make that the starting cluster.
        //

        while (FileSize > FatVolume->ClusterSize) {
            Status = FatpGetNextCluster(FatVolume,
                                        0,
                                        StartingCluster,
                                        &StartingCluster);

            if (!KSUCCESS(Status)) {
                goto DeleteFileBlocksEnd;
            }

            //
            // Fail if the end of file is hit. The caller should have specified
            // a new file size smaller than the original.
            //

            if ((StartingCluster < FAT_CLUSTER_BEGIN) ||
                (StartingCluster >= ClusterCount)) {

                Status = STATUS_INVALID_PARAMETER;
                goto DeleteFileBlocksEnd;
            }

            FileSize -= FatVolume->ClusterSize;
        }

        //
        // The starting cluster should now be the last cluster. Mark it as the
        // end and retrieve what was previously there.
        //

        FatAcquireLock(FatVolume->Lock);
        VolumeLockHeld = TRUE;
        Status = FatpFatCacheWriteClusterEntry(FatVolume,
                                               StartingCluster,
                                               FatVolume->ClusterEnd,
                                               &NextCluster);

        if (!KSUCCESS(Status)) {
            goto DeleteFileBlocksEnd;
        }

        DirtyFat = TRUE;
        FatReleaseLock(FatVolume->Lock);
        VolumeLockHeld = FALSE;

        //
        // This cluster should not be free. It is currently allocated!
        //

        if (NextCluster == FAT_CLUSTER_FREE) {
            RtlDebugPrint("FAT: DeleteFileBlocks: Free cluster after 0x%x\n",
                          StartingCluster);
        }

        //
        // If there is no next cluster, there's nothing that needs to be
        // deleted.
        //

        if ((NextCluster < FAT_CLUSTER_BEGIN) ||
            (NextCluster >= ClusterCount)) {

            Status = STATUS_SUCCESS;
            goto DeleteFileBlocksEnd;
        }

        StartingCluster = NextCluster;
    }

    //
    // Clean out the seek table if a file was provided.
    //

    if (File != NULL) {
        TableIndex = FAT_SEEK_TABLE_INDEX(FileSize);
        if ((TableIndex == 0) && (Truncate != FALSE)) {
            TableIndex = 1;
        }

        RtlZeroMemory(&(File->SeekTable[TableIndex]),
                      (FAT_SEEK_TABLE_SIZE - TableIndex) * FAT32_CLUSTER_WIDTH);
    }

    //
    // Free up the clusters. This flushes the FAT cache.
    //

    Status = FatpFreeClusterChain(FatVolume,
                                  NULL,
                                  StartingCluster);

    if (!KSUCCESS(Status)) {
        goto DeleteFileBlocksEnd;
    }

    DirtyFat = FALSE;
    Status = STATUS_SUCCESS;

DeleteFileBlocksEnd:
    if (DirtyFat != FALSE) {
        if (VolumeLockHeld == FALSE) {
            FatAcquireLock(FatVolume->Lock);
            VolumeLockHeld = TRUE;
        }

        FlushStatus = FatpFatCacheFlush(Volume, 0);
        if ((!KSUCCESS(FlushStatus)) && (KSUCCESS(Status))) {
            Status = FlushStatus;
        }
    }

    if (VolumeLockHeld != FALSE) {
        FatReleaseLock(FatVolume->Lock);
    }

    return Status;
}

KSTATUS
FatGetFileBlockInformation (
    PVOID Volume,
    FILE_ID FileId,
    PFILE_BLOCK_INFORMATION *BlockInformation
    )

/*++

Routine Description:

    This routine gets the block information for the given file.

Arguments:

    Volume - Supplies a pointer to the FAT volume.

    FileId - Supplies the ID of the file whose block information is being
        requested.

    BlockInformation - Supplies a pointer that receives a pointer to the block
        information for the file.

Return Value:

    Status code.

--*/

{

    PFILE_BLOCK_ENTRY BlockEntry;
    ULONG CurrentCluster;
    ULONGLONG DiskByteOffset;
    PFAT_VOLUME FatVolume;
    PFILE_BLOCK_INFORMATION Information;
    ULONG NextCluster;
    ULONG RunCount;
    ULONG RunStart;
    KSTATUS Status;

    FatVolume = (PFAT_VOLUME)Volume;
    NextCluster = (ULONG)FileId;

    //
    // Create the head of the list.
    //

    Information = FatAllocateNonPagedMemory(FatVolume->Device.DeviceToken,
                                            sizeof(FILE_BLOCK_INFORMATION));

    if (Information == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto GetFileBlockInformationEnd;
    }

    INITIALIZE_LIST_HEAD(&(Information->BlockList));

    //
    // Loop getting the next cluster, creating a new run every time a
    // non-contiguous cluster is discovered.
    //

    RunStart = NextCluster;
    RunCount = 1;
    while (TRUE) {
        CurrentCluster = NextCluster;
        Status = FatpGetNextCluster(FatVolume, 0, CurrentCluster, &NextCluster);
        if (!KSUCCESS(Status)) {
            goto GetFileBlockInformationEnd;
        }

        //
        // If the next cluster is free, reserved, or bad, the file is corrupt.
        //

        if ((NextCluster < FAT_CLUSTER_BEGIN) ||
            (NextCluster == FatVolume->ClusterBad)) {

            Status = STATUS_FILE_CORRUPT;
            goto GetFileBlockInformationEnd;
        }

        //
        // If this is the end of the file, exit and add the last run after the
        // loop.
        //

        if (NextCluster > FatVolume->ClusterBad) {
            break;
        }

        //
        // If this is part of the run, then up the count and continue.
        //

        if (NextCluster == (CurrentCluster + 1)) {
            RunCount += 1;
            continue;
        }

        //
        // The run is over. Add it to the list.
        //

        BlockEntry = FatAllocateNonPagedMemory(FatVolume->Device.DeviceToken,
                                               sizeof(FILE_BLOCK_ENTRY));

        if (BlockEntry == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto GetFileBlockInformationEnd;
        }

        DiskByteOffset = FAT_CLUSTER_TO_BYTE(FatVolume, RunStart);
        BlockEntry->Address = DiskByteOffset >> FatVolume->BlockShift;
        BlockEntry->Count = (RunCount * FatVolume->ClusterSize) >>
                            FatVolume->BlockShift;

        INSERT_BEFORE(&(BlockEntry->ListEntry), &(Information->BlockList));
        RunStart = NextCluster;
        RunCount = 1;
    }

    //
    // Add the last run to the list.
    //

    BlockEntry = FatAllocateNonPagedMemory(FatVolume->Device.DeviceToken,
                                           sizeof(FILE_BLOCK_ENTRY));

    if (BlockEntry == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto GetFileBlockInformationEnd;
    }

    DiskByteOffset = FAT_CLUSTER_TO_BYTE(FatVolume, RunStart);
    BlockEntry->Address = DiskByteOffset >> FatVolume->BlockShift;
    BlockEntry->Count = (RunCount * FatVolume->ClusterSize) >>
                        FatVolume->BlockShift;

    INSERT_BEFORE(&(BlockEntry->ListEntry), &(Information->BlockList));

    //
    // Now that the disk blocks are collected, query the backing device so that
    // it has a chance to adjust the offsets to absolute disk offsets.
    //

    Status = FatGetDeviceBlockInformation(FatVolume->Device.DeviceToken,
                                          Information);

    if (!KSUCCESS(Status)) {
        goto GetFileBlockInformationEnd;
    }

    *BlockInformation = Information;
    Status = STATUS_SUCCESS;

GetFileBlockInformationEnd:
    if (!KSUCCESS(Status)) {
        if (Information != NULL) {
            while (LIST_EMPTY(&(Information->BlockList)) == FALSE) {
                BlockEntry = LIST_VALUE(Information->BlockList.Next,
                                        FILE_BLOCK_ENTRY,
                                        ListEntry);

                LIST_REMOVE(&(BlockEntry->ListEntry));
                FatFreeNonPagedMemory(FatVolume->Device.DeviceToken,
                                      BlockEntry);
            }

            FatFreeNonPagedMemory(FatVolume->Device.DeviceToken, Information);
        }
    }

    return Status;
}

KSTATUS
FatAllocateFileClusters (
    PVOID Volume,
    FILE_ID FileId,
    ULONGLONG FileSize
    )

/*++

Routine Description:

    This routine expands the file capacity of the given file ID by allocating
    clusters for it. It does not zero out those clusters, so the usefulness of
    this function is limited to scenarios where the security of uninitialized
    disk contents is not a concern.

Arguments:

    Volume - Supplies a pointer to the FAT volume.

    FileId - Supplies the ID of the file whose size should be expanded.

    FileSize - Supplies the file size to allocate for the file.

Return Value:

    Status code.

--*/

{

    ULONG Cluster;
    ULONG ClusterCount;
    ULONGLONG CurrentSize;
    BOOL Dirty;
    PFAT_VOLUME FatVolume;
    ULONG NextCluster;
    KSTATUS Status;

    FatVolume = Volume;
    ClusterCount = FatVolume->ClusterCount;
    CurrentSize = 0;
    Dirty = FALSE;
    Status = STATUS_SUCCESS;

    ASSERT((FileId > FAT_CLUSTER_BEGIN) && (FileId < ClusterCount));

    Cluster = FileId;
    while (CurrentSize < FileSize) {
        Status = FatpGetNextCluster(Volume, 0, Cluster, &NextCluster);
        if (!KSUCCESS(Status)) {
            return Status;
        }

        if (NextCluster >= ClusterCount) {
            Status = FatpAllocateCluster(Volume, Cluster, &NextCluster, FALSE);
            if (!KSUCCESS(Status)) {
                return Status;
            }

            Dirty = TRUE;
        }

        Cluster = NextCluster;
        CurrentSize += FatVolume->ClusterSize;
    }

    if (Dirty != FALSE) {
        FatAcquireLock(FatVolume->Lock);
        Status = FatpFatCacheFlush(FatVolume, 0);
        FatReleaseLock(FatVolume->Lock);
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
FatpPerformFileIo (
    PVOID FileToken,
    BOOL Write,
    PFAT_SEEK_INFORMATION FatSeekInformation,
    PFAT_IO_BUFFER IoBuffer,
    UINTN SizeInBytes,
    ULONG IoFlags,
    PVOID Irp,
    PUINTN BytesCompleted
    )

/*++

Routine Description:

    This routine performs I/Of for the specified number of bytes from an open
    FAT file.

Arguments:

    FileToken - Supplies the opaque token returned when the file was opened.

    Write - Supplies a boolean indicating if this is a read or write.

    FatSeekInformation - Supplies a pointer to current seek data for the given
        file, indicating where to begin the I/O.

    IoBuffer - Supplies a pointer to a FAT I/O buffer containing the data to
        write to the file or the where the data read from the file will be
        stored.

    SizeInBytes - Supplies the number of bytes to read or write.

    IoFlags - Supplies flags regarding the I/O operation. See IO_FLAG_*
        definitions.

    Irp - Supplies an optional pointer to an IRP to use for disk transfers.

    BytesCompleted - Supplies the number of bytes that were actually read from
        or written to the file.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if no buffer was passed.

    STATUS_END_OF_FILE if the file end was reached. In this case the remaining
        data in the buffer as well as the bytes read field will be valid.

    STATUS_FILE_CORRUPT if the file system was internally inconsistent.

    Other error codes on device I/O error.

--*/

{

    ULONG BlockByteOffset;
    UINTN BlockCount;
    ULONG BlockShift;
    ULONG BlockSize;
    ULONGLONG ByteOffset;
    UINTN BytesThisRound;
    ULONG ClusterBad;
    ULONG ClusterCount;
    ULONG ClusterShift;
    ULONG ClusterSize;
    ULONG CurrentCluster;
    PFAT_FILE File;
    ULONGLONG FileByteOffset;
    KSTATUS FlushStatus;
    UINTN MaxContiguousBytes;
    ULONG NewCluster;
    BOOL NewTerritory;
    ULONG NextCluster;
    PFAT_IO_BUFFER ScratchIoBuffer;
    BOOL ScratchLockHeld;
    KSTATUS Status;
    ULONG TableIndex;
    UINTN TotalBytesProcessed;
    PFAT_VOLUME Volume;

    File = (PFAT_FILE)FileToken;
    Volume = File->Volume;
    BlockShift = Volume->BlockShift;
    BlockSize = Volume->Device.BlockSize;
    ClusterShift = Volume->ClusterShift;
    ClusterSize = Volume->ClusterSize;
    ClusterBad = Volume->ClusterBad;
    IoFlags |= IO_FLAG_FS_DATA;
    MaxContiguousBytes = 0;
    NewTerritory = FALSE;
    ScratchIoBuffer = NULL;
    ScratchLockHeld = FALSE;
    TotalBytesProcessed = 0;

    //
    // Do nothing if this is a bogus I/O request.
    //

    if (SizeInBytes == 0) {
        Status = STATUS_SUCCESS;
        goto PerformFileIoEnd;
    }

    //
    // If the file offset is suspiciously at 0 and the current cluster is not
    // set, then recalculate all the pointers.
    //

    if ((FatSeekInformation->FileByteOffset == 0) &&
        (FatSeekInformation->CurrentCluster == 0)) {

        if (File->IsRootDirectory != FALSE) {
            FatSeekInformation->CurrentBlock =
                                 Volume->RootDirectoryByteOffset >> BlockShift;

            MaxContiguousBytes = Volume->RootDirectoryCount *
                                 sizeof(FAT_DIRECTORY_ENTRY);

        } else {

            //
            // The file should already have at least one cluster that was
            // allocated when the file was opened. Files without clusters
            // aren't supported because the cluster number is used as the file
            // ID.
            //

            if (File->SeekTable[0] == FAT_CLUSTER_FREE) {

                ASSERT(FALSE);

                Status = STATUS_VOLUME_CORRUPT;
                goto PerformFileIoEnd;
            }

            if ((File->SeekTable[0] < FAT_CLUSTER_BEGIN) ||
                (File->SeekTable[0] >= ClusterBad)) {

                Status = STATUS_FILE_CORRUPT;
                goto PerformFileIoEnd;
            }

            //
            // Recalculate the block and byte offset for the first cluster in
            // the file.
            //

            FatSeekInformation->CurrentCluster = File->SeekTable[0];
            ByteOffset = FAT_CLUSTER_TO_BYTE(Volume, File->SeekTable[0]);
            FatSeekInformation->CurrentBlock = ByteOffset >> BlockShift;

            ASSERT(IS_ALIGNED(ByteOffset, BlockSize) != FALSE);
        }

        FatSeekInformation->ClusterByteOffset = 0;

    //
    // If needed, advance to a new cluster. It is assumed the caller
    // provides enough synchronization to avoid two threads concurrently
    // executing "get next cluster" then "allocate cluster" concurrently.
    //

    } else if (FatSeekInformation->ClusterByteOffset >= ClusterSize) {

        ASSERT(FatSeekInformation->ClusterByteOffset == ClusterSize);

        Status = FatpGetNextCluster(Volume,
                                    IoFlags,
                                    FatSeekInformation->CurrentCluster,
                                    &NextCluster);

        if (!KSUCCESS(Status)) {
            goto PerformFileIoEnd;
        }

        //
        // If it pointed to a free, reserved, or bad cluster, the file
        // is corrupt.
        //

        if ((NextCluster < FAT_CLUSTER_BEGIN) || (NextCluster == ClusterBad)) {
            Status = STATUS_FILE_CORRUPT;
            goto PerformFileIoEnd;
        }

        //
        // If this is the end and a read, stop. Otherwise allocate a cluster.
        //

        if (NextCluster > ClusterBad) {
            if (Write == FALSE) {
                FatSeekInformation->CurrentBlock = 0;
                Status = STATUS_END_OF_FILE;
                goto PerformFileIoEnd;
            }

            ASSERT((IoFlags & IO_FLAG_NO_ALLOCATE) == 0);
            ASSERT((File->OpenFlags & OPEN_FLAG_PAGE_FILE) == 0);

            Status = FatpAllocateCluster(Volume,
                                         FatSeekInformation->CurrentCluster,
                                         &NewCluster,
                                         FALSE);

            if (!KSUCCESS(Status)) {
                goto PerformFileIoEnd;
            }

            NewTerritory = TRUE;
            FatSeekInformation->CurrentCluster = NewCluster;
            ByteOffset = FAT_CLUSTER_TO_BYTE(Volume, NewCluster);
            FatSeekInformation->CurrentBlock = ByteOffset >> BlockShift;

            ASSERT(IS_ALIGNED(ByteOffset, BlockSize) != FALSE);

        //
        // This is not the end, simply update the current cluster.
        //

        } else {
            ByteOffset = FAT_CLUSTER_TO_BYTE(Volume, NextCluster);

            ASSERT(IS_ALIGNED(ByteOffset, BlockSize) != FALSE);

            FatSeekInformation->CurrentCluster = NextCluster;
            FatSeekInformation->CurrentBlock = ByteOffset >> BlockShift;
        }

        FatSeekInformation->ClusterByteOffset = 0;

        //
        // Populate the seek table.
        //

        TableIndex = FAT_SEEK_TABLE_INDEX(FatSeekInformation->FileByteOffset);
        if ((FatSeekInformation->FileByteOffset & FAT_SEEK_OFFSET_MASK) == 0) {

            ASSERT((File->SeekTable[TableIndex] == 0) ||
                   (File->SeekTable[TableIndex] ==
                    FatSeekInformation->CurrentCluster));

            ASSERT(FatSeekInformation->CurrentCluster != 0);

            File->SeekTable[TableIndex] = FatSeekInformation->CurrentCluster;
        }
    }

    ASSERT(FatSeekInformation->CurrentBlock != 0);

    NextCluster = FAT_CLUSTER_FREE;
    Status = STATUS_SUCCESS;
    while (SizeInBytes != 0) {

        //
        // Search forward looking for contiguous clusters to determine the
        // maximum amount of data that can be processed on this pass.
        //

        if (MaxContiguousBytes == 0) {
            if (File->IsRootDirectory != FALSE) {
                Status = STATUS_END_OF_FILE;
                goto PerformFileIoEnd;
            }

            MaxContiguousBytes = ClusterSize -
                                 FatSeekInformation->ClusterByteOffset;

            CurrentCluster = FatSeekInformation->CurrentCluster;
            FileByteOffset = FatSeekInformation->FileByteOffset;
            while (MaxContiguousBytes < SizeInBytes) {
                Status = FatpGetNextCluster(Volume,
                                            IoFlags,
                                            CurrentCluster,
                                            &NextCluster);

                if (!KSUCCESS(Status)) {
                    break;
                }

                if ((NextCluster < FAT_CLUSTER_BEGIN) ||
                    (NextCluster == ClusterBad)) {

                    Status = STATUS_FILE_CORRUPT;
                    goto PerformFileIoEnd;
                }

                if (NextCluster > ClusterBad) {
                    if (Write == FALSE) {
                        break;
                    }

                    ASSERT((IoFlags & IO_FLAG_NO_ALLOCATE) == 0);
                    ASSERT((File->OpenFlags & OPEN_FLAG_PAGE_FILE) == 0);

                    Status = FatpAllocateCluster(Volume,
                                                 CurrentCluster,
                                                 &NewCluster,
                                                 FALSE);

                    if (!KSUCCESS(Status)) {
                        goto PerformFileIoEnd;
                    }

                    NewTerritory = TRUE;
                    NextCluster = NewCluster;
                }

                if (NextCluster != (CurrentCluster + 1)) {
                    break;
                }

                MaxContiguousBytes += ClusterSize;
                CurrentCluster = NextCluster;
                FileByteOffset += ClusterSize;

                //
                // Populate the seek table.
                //

                TableIndex = FAT_SEEK_TABLE_INDEX(FileByteOffset);
                if ((FileByteOffset & FAT_SEEK_OFFSET_MASK) == 0) {

                    ASSERT((File->SeekTable[TableIndex] == 0) ||
                           (File->SeekTable[TableIndex] == CurrentCluster));

                    ASSERT(CurrentCluster != 0);

                    File->SeekTable[TableIndex] = CurrentCluster;
                }
            }
        }

        if (MaxContiguousBytes > SizeInBytes) {
            MaxContiguousBytes = SizeInBytes;
        }

        //
        // Get the block byte offset and number of bytes to process for the
        // next round of I/O. If the file offset is block aligned, try to
        // process full blocks.
        //

        FileByteOffset = FatSeekInformation->FileByteOffset;
        if (IS_ALIGNED(FileByteOffset, BlockSize) != FALSE) {
            BlockByteOffset = 0;
            BytesThisRound = ALIGN_RANGE_DOWN(MaxContiguousBytes, BlockSize);
            if (BytesThisRound == 0) {
                BytesThisRound = MaxContiguousBytes;
            }

        //
        // Otherwise process a partial block.
        //

        } else {
            BlockByteOffset = REMAINDER(FileByteOffset, BlockSize);
            BytesThisRound = BlockSize - BlockByteOffset;
            if (BytesThisRound > MaxContiguousBytes) {
                BytesThisRound = MaxContiguousBytes;
            }
        }

        //
        // If processing entire blocks, use the buffer directly.
        //

        if (BytesThisRound >= BlockSize) {

            ASSERT(IS_ALIGNED(BytesThisRound, BlockSize) != FALSE);

            BlockCount = BytesThisRound >> BlockShift;
            if (Write != FALSE) {
                Status = FatWriteDevice(Volume->Device.DeviceToken,
                                        FatSeekInformation->CurrentBlock,
                                        BlockCount,
                                        IoFlags,
                                        Irp,
                                        IoBuffer);

            } else {
                Status = FatReadDevice(Volume->Device.DeviceToken,
                                       FatSeekInformation->CurrentBlock,
                                       BlockCount,
                                       IoFlags,
                                       Irp,
                                       IoBuffer);
            }

            if (!KSUCCESS(Status)) {
                goto PerformFileIoEnd;
            }

        //
        // A partial block is being processed, so the original block must be
        // read, modified, and written back (for writes) or read and copied
        // (for reads). It is assumed the caller provides the synchronization
        // necessary to ensure these partial writes don't stomp each other out.
        //

        } else {

            ASSERT(ScratchIoBuffer == NULL);
            ASSERT(((File->OpenFlags & OPEN_FLAG_PAGE_FILE) == 0) ||
                   (FatGetPageSize() < BlockSize));

            BlockCount = 1;
            ScratchIoBuffer = File->ScratchIoBuffer;
            if (File->ScratchIoBufferLock != NULL) {
                FatAcquireLock(File->ScratchIoBuffer);
                ScratchLockHeld = TRUE;
            }

            if (ScratchIoBuffer == NULL) {
                ScratchIoBuffer = FatAllocateIoBuffer(
                                                    Volume->Device.DeviceToken,
                                                    BlockSize);

                if (ScratchIoBuffer == NULL) {
                    Status = STATUS_INSUFFICIENT_RESOURCES;
                    goto PerformFileIoEnd;
                }
            }

            if (NewTerritory != FALSE) {

                ASSERT(Write != FALSE);

                Status = FatZeroIoBuffer(ScratchIoBuffer, 0, BlockSize);
                if (!KSUCCESS(Status)) {
                    goto PerformFileIoEnd;
                }

            } else {
                Status = FatReadDevice(Volume->Device.DeviceToken,
                                       FatSeekInformation->CurrentBlock,
                                       BlockCount,
                                       IoFlags,
                                       Irp,
                                       ScratchIoBuffer);

                if (!KSUCCESS(Status)) {
                    goto PerformFileIoEnd;
                }
            }

            //
            // For writes, copy the data from the supplied FAT I/O buffer to
            // the scratch buffer and then write the scratch buffer to the
            // device.
            //

            ASSERT((BlockByteOffset + BytesThisRound) <= BlockSize);

            if (Write != FALSE) {
                Status = FatCopyIoBuffer(ScratchIoBuffer,
                                         BlockByteOffset,
                                         IoBuffer,
                                         0,
                                         BytesThisRound);

                if (!KSUCCESS(Status)) {
                    goto PerformFileIoEnd;
                }

                Status = FatWriteDevice(Volume->Device.DeviceToken,
                                        FatSeekInformation->CurrentBlock,
                                        BlockCount,
                                        IoFlags,
                                        Irp,
                                        ScratchIoBuffer);

                if (!KSUCCESS(Status)) {
                    goto PerformFileIoEnd;
                }

            //
            // For reads, copy the data from the scratch buffer into the
            // supplied FAT I/O buffer.
            //

            } else {
                Status = FatCopyIoBuffer(IoBuffer,
                                         0,
                                         ScratchIoBuffer,
                                         BlockByteOffset,
                                         BytesThisRound);

                if (!KSUCCESS(Status)) {
                    goto PerformFileIoEnd;
                }
            }

            if (ScratchIoBuffer != File->ScratchIoBuffer) {

                ASSERT(File->ScratchIoBuffer == NULL);

                FatFreeIoBuffer(ScratchIoBuffer);

            } else {

                ASSERT(File->ScratchIoBufferLock != NULL);

                FatReleaseLock(File->ScratchIoBufferLock);
                ScratchLockHeld = FALSE;
            }

            ScratchIoBuffer = NULL;

            //
            // If the I/O did not reach the end of the block, do not advance
            // the current block in the seek information.
            //

            if ((BlockByteOffset + BytesThisRound) < BlockSize) {
                BlockCount = 0;
            }
        }

        //
        // Update the I/O buffer's offset so the next I/O starts where this
        // round left off.
        //

        FatIoBufferUpdateOffset(IoBuffer, BytesThisRound, FALSE);

        //
        // Update the counters.
        //

        ASSERT(BytesThisRound != 0);

        SizeInBytes -= BytesThisRound;
        MaxContiguousBytes -= BytesThisRound;
        FatSeekInformation->FileByteOffset += BytesThisRound;
        TotalBytesProcessed += BytesThisRound;

        //
        // Advance the cluster and cluster byte offset carefully. If the max
        // contiguous bytes have gone to zero and there is more to write then
        // the next cluster is not contiguous. It, however, has already been
        // retrieved, so just advance to that cluster.
        //

        if ((MaxContiguousBytes == 0) && (SizeInBytes != 0)) {
            if (File->IsRootDirectory != FALSE) {
                Status = STATUS_END_OF_FILE;
                goto PerformFileIoEnd;
            }

            ASSERT(NextCluster != FAT_CLUSTER_FREE);

            //
            // Stop if this is the end of the file.
            //

            if (NextCluster >= ClusterBad) {
                FatSeekInformation->CurrentBlock = 0;
                Status = STATUS_END_OF_FILE;
                goto PerformFileIoEnd;
            }

            //
            // Calculate the starting block for the next cluster.
            //

            ByteOffset = FAT_CLUSTER_TO_BYTE(Volume, NextCluster);
            FatSeekInformation->CurrentBlock = ByteOffset >> BlockShift;
            FatSeekInformation->CurrentCluster = NextCluster;
            FatSeekInformation->ClusterByteOffset = 0;
            MaxContiguousBytes = 0;

        //
        // Otherwise, there are two cases. Either there are no bytes left to
        // process or there are more contiguous bytes to process. In the latter
        // case, this can freely roll over to the next cluster. In the former
        // case, the next cluster is not known so this must set the seek
        // information appropriately so that the next I/O operation looks up
        // the next cluster.
        //

        } else {

            ASSERT(((SizeInBytes == 0) && (MaxContiguousBytes == 0)) ||
                   ((SizeInBytes != 0) && (MaxContiguousBytes != 0)));

            FatSeekInformation->CurrentBlock += BlockCount;
            if (File->IsRootDirectory == FALSE) {
                FatSeekInformation->ClusterByteOffset += BytesThisRound;
                ClusterCount = FatSeekInformation->ClusterByteOffset >>
                               ClusterShift;

                FatSeekInformation->CurrentCluster += ClusterCount;
                FatSeekInformation->ClusterByteOffset =
                             (FatSeekInformation->ClusterByteOffset -
                              (ClusterCount << ClusterShift));

                //
                // Roll it back if this I/O is done and it went up to a cluster
                // boundary.
                //

                if ((SizeInBytes == 0) &&
                    (IS_ALIGNED(FatSeekInformation->FileByteOffset,
                                ClusterSize))) {

                    FatSeekInformation->CurrentCluster -= 1;

                    ASSERT(FatSeekInformation->ClusterByteOffset == 0);

                    FatSeekInformation->ClusterByteOffset = ClusterSize;
                }
            }
        }

        ASSERT(FatSeekInformation->ClusterByteOffset <= ClusterSize);
    }

    Status = STATUS_SUCCESS;

PerformFileIoEnd:
    if (ScratchLockHeld != FALSE) {
        FatReleaseLock(File->ScratchIoBufferLock);
    }

    if ((ScratchIoBuffer != NULL) &&
        (ScratchIoBuffer != File->ScratchIoBuffer)) {

        FatFreeIoBuffer(ScratchIoBuffer);
    }

    if (NewTerritory != FALSE) {
        FatAcquireLock(Volume->Lock);
        FlushStatus = FatpFatCacheFlush(Volume, IoFlags);
        FatReleaseLock(Volume->Lock);
        if ((!KSUCCESS(FlushStatus)) && (KSUCCESS(Status))) {
            Status = FlushStatus;
        }
    }

    if (TotalBytesProcessed != 0) {
        FatIoBufferUpdateOffset(IoBuffer, TotalBytesProcessed, TRUE);
    }

    *BytesCompleted = TotalBytesProcessed;
    return Status;
}

