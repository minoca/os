/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    partelto.c

Abstract:

    This module implements support for parsing El Torito partitions.

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

EFI_STATUS
EfiPartitionDetectElTorito (
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

    UINTN BootEntry;
    EFI_ELTORITO_CATALOG *Catalog;
    CDROM_DEVICE_PATH CdPath;
    UINTN Check;
    UINT16 *CheckBuffer;
    INTN CompareResult;
    EFI_STATUS Found;
    UINTN Index;
    UINT32 Lba;
    UINTN MaxIndex;
    EFI_BLOCK_IO_MEDIA *Media;
    UINT32 SectorCount;
    EFI_STATUS Status;
    UINT32 SubBlockSize;
    EFI_CDROM_VOLUME_DESCRIPTOR *VolumeDescriptor;
    UINT32 VolumeDescriptorLba;
    UINT32 VolumeSpaceSize;

    Found = EFI_NOT_FOUND;
    Media = BlockIo->Media;
    VolumeSpaceSize = 0;

    //
    // CD-ROMs have a fixed block size.
    //

    if (Media->BlockSize != EFI_CD_BLOCK_SIZE) {
        return EFI_NOT_FOUND;
    }

    VolumeDescriptor = EfiCoreAllocateBootPool((UINTN)(Media->BlockSize));
    if (VolumeDescriptor == NULL) {
        return EFI_NOT_FOUND;
    }

    Catalog = (EFI_ELTORITO_CATALOG *)VolumeDescriptor;

    //
    // The ISO-9660 volume descriptor starts at 32k on the media (and the
    // block size is fixed to 2048 remember).
    //

    VolumeDescriptorLba = EFI_CD_VOLUME_RECORD_LBA - 1;

    //
    // Loop over all the volumes.
    //

    while (TRUE) {
        VolumeDescriptorLba += 1;
        if (VolumeDescriptorLba > Media->LastBlock) {
            break;
        }

        Status = DiskIo->ReadDisk(DiskIo,
                                  Media->MediaId,
                                  VolumeDescriptorLba * Media->BlockSize,
                                  Media->BlockSize,
                                  VolumeDescriptor);

        if (EFI_ERROR(Status)) {
            RtlDebugPrint("ElTorito: Failed to read volume descriptor.\n");
            Found = Status;
            break;
        }

        //
        // Check for a valid volume descriptor signature.
        //

        if (VolumeDescriptor->BootRecordVolume.Type == EFI_CD_VOLUME_TYPE_END) {
            break;
        }

        CompareResult = EfiCoreCompareMemory(
                                   VolumeDescriptor->BootRecordVolume.SystemId,
                                   EFI_CD_VOLUME_ELTORITO_ID,
                                   sizeof(EFI_CD_VOLUME_ELTORITO_ID) - 1);

        if (CompareResult != 0) {
            continue;
        }

        //
        // Read in the boot catalog.
        //

        Lba = EFI_UNPACK_UINT32(VolumeDescriptor->BootRecordVolume.Catalog);
        if (Lba > Media->LastBlock) {
            continue;
        }

        Status = DiskIo->ReadDisk(DiskIo,
                                  Media->MediaId,
                                  Lba * Media->BlockSize,
                                  Media->BlockSize,
                                  Catalog);

        if (EFI_ERROR(Status)) {
            RtlDebugPrint("ElTorito: Error reading catalog at lba 0x%I64x.\n",
                          Lba);

            continue;
        }

        //
        // Make sure it looks like a catalog.
        //

        if ((Catalog->Catalog.Indicator != EFI_ELTORITO_ID_CATALOG) ||
            (Catalog->Catalog.Id55AA != 0xAA55)) {

            RtlDebugPrint("ElTorito: Bad catalog.\n");
            continue;
        }

        Check = 0;
        CheckBuffer = (UINT16 *)Catalog;
        for (Index = 0;
             Index < sizeof(EFI_ELTORITO_CATALOG) / sizeof(UINT16);
             Index += 1) {

            Check += CheckBuffer[Index];
        }

        if ((Check & 0xFFFF) != 0) {
            RtlDebugPrint("ElTorito: Catalog checksum failure.\n");
        }

        MaxIndex = Media->BlockSize / sizeof(EFI_ELTORITO_CATALOG);
        BootEntry = 1;
        for (Index = 1; Index < MaxIndex; Index += 1) {
            Catalog += 1;
            if ((Catalog->Boot.Indicator != EFI_ELTORITO_ID_SECTION_BOOTABLE) ||
                (Catalog->Boot.Lba == 0)) {

                continue;
            }

            SubBlockSize = 512;
            SectorCount = Catalog->Boot.SectorCount;
            switch (Catalog->Boot.MediaType) {
            case EFI_ELTORITO_NO_EMULATION:
                SubBlockSize = Media->BlockSize;
                break;

            case EFI_ELTORITO_HARD_DISK:
                break;

            case EFI_ELTORITO_12_DISKETTE:
                SectorCount = 0x50 * 0x02 * 0x0F;
                break;

            case EFI_ELTORITO_14_DISKETTE:
                SectorCount = 0x50 * 0x02 * 0x12;
                break;

            case EFI_ELTORITO_28_DISKETTE:
                SectorCount = 0x50 * 0x02 * 0x24;
                break;

            default:
                RtlDebugPrint("ElTorito: Unsupported boot media type 0x%x.\n",
                              Catalog->Boot.MediaType);

                break;
            }

            VolumeSpaceSize += SectorCount * SubBlockSize;

            //
            // Create a child device handle.
            //

            CdPath.Header.Type = MEDIA_DEVICE_PATH;
            CdPath.Header.SubType = MEDIA_CDROM_DP;
            EfiCoreSetDevicePathNodeLength(&(CdPath.Header), sizeof(CdPath));
            if (Index == 1) {
                BootEntry = 0;
            }

            CdPath.BootEntry = (UINT32)BootEntry;
            BootEntry += 1;
            CdPath.PartitionStart = Catalog->Boot.Lba;

            //
            // If the sector count is less than two, set the partition as the
            // whole CD.
            //

            if (SectorCount < 2) {
                if (VolumeSpaceSize > Media->LastBlock + 1) {
                    CdPath.PartitionSize = (UINT32)(Media->LastBlock -
                                                    Catalog->Boot.Lba + 1);

                } else {
                    CdPath.PartitionSize = (UINT32)(VolumeSpaceSize -
                                                    Catalog->Boot.Lba);
                }

            } else {
                CdPath.PartitionSize = ALIGN_VALUE(SectorCount * SubBlockSize,
                                                   Media->BlockSize);
            }

            Status = EfiPartitionInstallChildHandle(
                                  This,
                                  Handle,
                                  DiskIo,
                                  BlockIo,
                                  DevicePath,
                                  (EFI_DEVICE_PATH_PROTOCOL *)&CdPath,
                                  Catalog->Boot.Lba,
                                  Catalog->Boot.Lba + CdPath.PartitionSize - 1,
                                  SubBlockSize,
                                  FALSE);

            if (!EFI_ERROR(Status)) {
                Found = EFI_SUCCESS;
            }
        }
    }

    EfiFreePool(VolumeDescriptor);
    return Found;
}

//
// --------------------------------------------------------- Internal Functions
//

