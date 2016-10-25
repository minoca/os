/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    disk.c

Abstract:

    This module enumerates the disks found on BIOS systems.

Author:

    Evan Green 20-Mar-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include <minoca/uefi/protocol/blockio.h>
#include "biosfw.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro returns a pointer to the disk I/O data given a pointer to the
// block I/O protocol instance.
//

#define EFI_PCAT_DISK_FROM_THIS(_BlockIo) \
        (EFI_PCAT_DISK *)((VOID *)(_BlockIo) -                  \
                        ((VOID *)(&(((EFI_PCAT_DISK *)0)->BlockIo))))

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_PCAT_DISK_MAGIC 0x73446350 // 'sDcP'

//
// Define the drive numbers to probe.
//

#define EFI_PCAT_HARD_DRIVE_START 0x80
#define EFI_PCAT_HARD_DRIVE_COUNT 0x10

#define EFI_PCAT_REMOVABLE_DRIVE_START 0x00
#define EFI_PCAT_REMOVABLE_DRIVE_COUNT 0x10

#define EFI_PCAT_MAX_SECTORS_PER_TRANSFER 0x08

#define EFI_BIOS_BLOCK_IO_DEVICE_PATH_GUID                  \
    {                                                       \
        0xCF31FAC5, 0xC24E, 0x11D2,                         \
        {0x85, 0xF3, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3C}    \
    }

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores the disk I/O protocol's private context.

Members:

    Magic - Stores the magic constand EFI_PCAT_DISK_MAGIC.

    Handle - Stores the handle to the block I/O device.

    DevicePath - Stores a pointer to the device path.

    DriveNumber - Stores the BIOS drive number of this disk.

    SectorSize - Stores the size of a block in this device.

    TotalSectors - Stores the count of sectors in this device.

    BlockIo - Stores the block I/O protocol.

    Media - Stores the block I/O media information.

--*/

typedef struct _EFI_PCAT_DISK {
    UINT32 Magic;
    EFI_HANDLE Handle;
    EFI_DEVICE_PATH_PROTOCOL *DevicePath;
    UINT8 DriveNumber;
    UINT32 SectorSize;
    UINT64 TotalSectors;
    EFI_BLOCK_IO_PROTOCOL BlockIo;
    EFI_BLOCK_IO_MEDIA Media;
} EFI_PCAT_DISK, *PEFI_PCAT_DISK;

/*++

Structure Description:

    This structure defines the BIOS block I/O device path.

Members:

    DevicePath - Stores the standard vendor-specific device path.

    DriveNumber - Stores the BIOS drive number.

--*/

typedef struct _EFI_BIOS_BLOCK_IO_DEVICE_PATH {
    VENDOR_DEVICE_PATH DevicePath;
    UINT8 DriveNumber;
} EFI_BIOS_BLOCK_IO_DEVICE_PATH, *PEFI_BIOS_BLOCK_IO_DEVICE_PATH;

/*++

Structure Description:

    This structure defines the BIOS block I/O device path.

Members:

    DevicePath - Stores the standard vendor-specific device path.

    DriveNumber - Stores the BIOS drive number.

--*/

typedef struct _EFI_PCAT_DISK_DEVICE_PATH {
    EFI_BIOS_BLOCK_IO_DEVICE_PATH Disk;
    EFI_DEVICE_PATH_PROTOCOL End;
} PACKED EFI_PCAT_DISK_DEVICE_PATH, *PEFI_PCAT_DISK_DEVICE_PATH;

//
// ----------------------------------------------- Internal Function Prototypes
//

EFIAPI
EFI_STATUS
EfipPcatDiskReset (
    EFI_BLOCK_IO_PROTOCOL *This,
    BOOLEAN ExtendedVerification
    );

EFIAPI
EFI_STATUS
EfipPcatDiskReadBlocks (
    EFI_BLOCK_IO_PROTOCOL *This,
    UINT32 MediaId,
    EFI_LBA Lba,
    UINTN BufferSize,
    VOID *Buffer
    );

EFIAPI
EFI_STATUS
EfipPcatDiskWriteBlocks (
    EFI_BLOCK_IO_PROTOCOL *This,
    UINT32 MediaId,
    EFI_LBA Lba,
    UINTN BufferSize,
    VOID *Buffer
    );

EFIAPI
EFI_STATUS
EfipPcatDiskFlushBlocks (
    EFI_BLOCK_IO_PROTOCOL *This
    );

EFI_STATUS
EfipPcatProbeDrive (
    UINTN DriveNumber
    );

EFI_STATUS
EfipPcatGetDiskParameters (
    UINT8 DriveNumber,
    UINT64 *SectorCount,
    UINT32 *SectorSize
    );

EFI_STATUS
EfipPcatBlockOperation (
    PEFI_PCAT_DISK Disk,
    BOOLEAN Write,
    VOID *Buffer,
    UINT32 AbsoluteSector,
    UINT32 SectorCount
    );

EFI_STATUS
EfipPcatResetDisk (
    UINT8 DriveNumber
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define the private data template.
//

EFI_PCAT_DISK EfiPcatDiskTemplate = {
    EFI_PCAT_DISK_MAGIC,
    NULL,
    NULL,
    0xFF,
    0,
    0,
    {
        EFI_BLOCK_IO_PROTOCOL_REVISION3,
        NULL,
        EfipPcatDiskReset,
        EfipPcatDiskReadBlocks,
        EfipPcatDiskWriteBlocks,
        EfipPcatDiskFlushBlocks
    }
};

//
// Define the device path template.
//

EFI_PCAT_DISK_DEVICE_PATH EfiPcatDevicePathTemplate = {
    {
        {
            {
                HARDWARE_DEVICE_PATH,
                HW_VENDOR_DP,
                sizeof(EFI_BIOS_BLOCK_IO_DEVICE_PATH)
            },

            EFI_BIOS_BLOCK_IO_DEVICE_PATH_GUID,
        },

        0xFF
    },

    {
        END_DEVICE_PATH_TYPE,
        END_ENTIRE_DEVICE_PATH_SUBTYPE,
        END_DEVICE_PATH_LENGTH
    }
};

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfipPcatEnumerateDisks (
    VOID
    )

/*++

Routine Description:

    This routine enumerates all the disks it can find on a BIOS machine.

Arguments:

    None.

Return Value:

    EFI Status code.

--*/

{

    UINTN DriveIndex;
    EFI_STATUS Status;

    for (DriveIndex = 0;
         DriveIndex < EFI_PCAT_HARD_DRIVE_COUNT;
         DriveIndex += 1) {

        Status = EfipPcatProbeDrive(DriveIndex + EFI_PCAT_HARD_DRIVE_START);
        if (EFI_ERROR(Status)) {
            break;
        }
    }

    for (DriveIndex = 0;
         DriveIndex < EFI_PCAT_REMOVABLE_DRIVE_COUNT;
         DriveIndex += 1) {

        Status = EfipPcatProbeDrive(
                                  DriveIndex + EFI_PCAT_REMOVABLE_DRIVE_START);

        if (EFI_ERROR(Status)) {
            break;
        }
    }

    return EFI_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

EFIAPI
EFI_STATUS
EfipPcatDiskReset (
    EFI_BLOCK_IO_PROTOCOL *This,
    BOOLEAN ExtendedVerification
    )

/*++

Routine Description:

    This routine resets the block device.

Arguments:

    This - Supplies a pointer to the protocol instance.

    ExtendedVerification - Supplies a boolean indicating whether or not the
        driver should perform diagnostics on reset.

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request.

--*/

{

    PEFI_PCAT_DISK Disk;

    Disk = EFI_PCAT_DISK_FROM_THIS(This);
    return EfipPcatResetDisk(Disk->DriveNumber);
}

EFIAPI
EFI_STATUS
EfipPcatDiskReadBlocks (
    EFI_BLOCK_IO_PROTOCOL *This,
    UINT32 MediaId,
    EFI_LBA Lba,
    UINTN BufferSize,
    VOID *Buffer
    )

/*++

Routine Description:

    This routine performs a block I/O read from the device.

Arguments:

    This - Supplies a pointer to the protocol instance.

    MediaId - Supplies the media identifier, which changes each time the media
        is replaced.

    Lba - Supplies the logical block address of the read.

    BufferSize - Supplies the size of the buffer in bytes.

    Buffer - Supplies the buffer where the read data will be returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request.

    EFI_NO_MEDIA if there is no media in the device.

    EFI_MEDIA_CHANGED if the media ID does not match the current device.

    EFI_BAD_BUFFER_SIZE if the buffer was not a multiple of the device block
    size.

    EFI_INVALID_PARAMETER if the read request contains LBAs that are not valid,
    or the buffer is not properly aligned.

--*/

{

    PEFI_PCAT_DISK Disk;
    UINTN SectorCount;
    UINTN SectorsThisRound;
    EFI_STATUS Status;

    Disk = EFI_PCAT_DISK_FROM_THIS(This);
    if (MediaId != Disk->Media.MediaId) {
        return EFI_MEDIA_CHANGED;
    }

    if (Disk->Media.MediaPresent == FALSE) {
        return EFI_NO_MEDIA;
    }

    Status = EFI_SUCCESS;
    SectorCount = BufferSize / Disk->SectorSize;
    while (SectorCount != 0) {
        SectorsThisRound = EFI_PCAT_MAX_SECTORS_PER_TRANSFER;
        if (SectorsThisRound > SectorCount) {
            SectorsThisRound = SectorCount;
        }

        Status = EfipPcatBlockOperation(Disk,
                                        FALSE,
                                        Buffer,
                                        (UINT32)Lba,
                                        SectorsThisRound);

        if (EFI_ERROR(Status)) {
            break;
        }

        Lba += SectorsThisRound;
        Buffer += SectorsThisRound * Disk->SectorSize;
        SectorCount -= SectorsThisRound;
    }

    return Status;
}

EFIAPI
EFI_STATUS
EfipPcatDiskWriteBlocks (
    EFI_BLOCK_IO_PROTOCOL *This,
    UINT32 MediaId,
    EFI_LBA Lba,
    UINTN BufferSize,
    VOID *Buffer
    )

/*++

Routine Description:

    This routine performs a block I/O write to the device.

Arguments:

    This - Supplies a pointer to the protocol instance.

    MediaId - Supplies the media identifier, which changes each time the media
        is replaced.

    Lba - Supplies the logical block address of the write.

    BufferSize - Supplies the size of the buffer in bytes.

    Buffer - Supplies the buffer containing the data to write.

Return Value:

    EFI_SUCCESS on success.

    EFI_WRITE_PROTECTED if the device cannot be written to.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request.

    EFI_NO_MEDIA if there is no media in the device.

    EFI_MEDIA_CHANGED if the media ID does not match the current device.

    EFI_BAD_BUFFER_SIZE if the buffer was not a multiple of the device block
    size.

    EFI_INVALID_PARAMETER if the read request contains LBAs that are not valid,
    or the buffer is not properly aligned.

--*/

{

    PEFI_PCAT_DISK Disk;
    UINTN SectorCount;
    UINTN SectorsThisRound;
    EFI_STATUS Status;

    Disk = EFI_PCAT_DISK_FROM_THIS(This);
    if (MediaId != Disk->Media.MediaId) {
        return EFI_MEDIA_CHANGED;
    }

    if (Disk->Media.MediaPresent == FALSE) {
        return EFI_NO_MEDIA;
    }

    Status = EFI_SUCCESS;
    SectorCount = BufferSize / Disk->SectorSize;
    while (SectorCount != 0) {
        SectorsThisRound = EFI_PCAT_MAX_SECTORS_PER_TRANSFER;
        if (SectorsThisRound > SectorCount) {
            SectorsThisRound = SectorCount;
        }

        Status = EfipPcatBlockOperation(Disk,
                                        TRUE,
                                        Buffer,
                                        (UINT32)Lba,
                                        SectorsThisRound);

        if (EFI_ERROR(Status)) {
            break;
        }

        Lba += SectorsThisRound;
        Buffer += SectorsThisRound * Disk->SectorSize;
        SectorCount -= SectorsThisRound;
    }

    return Status;
}

EFIAPI
EFI_STATUS
EfipPcatDiskFlushBlocks (
    EFI_BLOCK_IO_PROTOCOL *This
    )

/*++

Routine Description:

    This routine flushes the block device.

Arguments:

    This - Supplies a pointer to the protocol instance.

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request.

    EFI_NO_MEDIA if there is no media in the device.

--*/

{

    return EFI_SUCCESS;
}

EFI_STATUS
EfipPcatProbeDrive (
    UINTN DriveNumber
    )

/*++

Routine Description:

    This routine probes the given drive number and creates a device handle if
    there is a drive there.

Arguments:

    DriveNumber - Supplies the drive number to probe.

Return Value:

    EFI Status code.

--*/

{

    PEFI_PCAT_DISK_DEVICE_PATH DevicePath;
    PEFI_PCAT_DISK Disk;
    UINT64 SectorCount;
    UINT32 SectorSize;
    EFI_STATUS Status;

    Status = EfipPcatGetDiskParameters(DriveNumber, &SectorCount, &SectorSize);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    //
    // There's a disk there. Allocate a data structure for it.
    //

    Status = EfiAllocatePool(EfiBootServicesData,
                             sizeof(EFI_PCAT_DISK),
                             (VOID **)&Disk);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    EfiCopyMem(Disk, &EfiPcatDiskTemplate, sizeof(EFI_PCAT_DISK));
    Disk->Handle = NULL;
    Disk->DriveNumber = DriveNumber;
    Disk->SectorSize = SectorSize;
    Disk->TotalSectors = SectorCount;
    Disk->BlockIo.Media = &(Disk->Media);
    if (DriveNumber < EFI_PCAT_HARD_DRIVE_START) {
        Disk->Media.RemovableMedia = TRUE;
    }

    Disk->Media.MediaPresent = TRUE;
    Disk->Media.BlockSize = SectorSize;
    Disk->Media.LastBlock = SectorCount - 1;
    Status = EfiAllocatePool(EfiBootServicesData,
                             sizeof(EFI_PCAT_DISK_DEVICE_PATH),
                             (VOID **)&DevicePath);

    if (EFI_ERROR(Status)) {
        EfiFreePool(Disk);
        return Status;
    }

    EfiCopyMem(DevicePath,
               &EfiPcatDevicePathTemplate,
               sizeof(EFI_PCAT_DISK_DEVICE_PATH));

    DevicePath->Disk.DriveNumber = DriveNumber;
    Disk->DevicePath = (EFI_DEVICE_PATH_PROTOCOL *)DevicePath;
    Status = EfiInstallMultipleProtocolInterfaces(&(Disk->Handle),
                                                  &EfiDevicePathProtocolGuid,
                                                  Disk->DevicePath,
                                                  &EfiBlockIoProtocolGuid,
                                                  &(Disk->BlockIo),
                                                  NULL);

    return Status;
}

EFI_STATUS
EfipPcatGetDiskParameters (
    UINT8 DriveNumber,
    UINT64 *SectorCount,
    UINT32 *SectorSize
    )

/*++

Routine Description:

    This routine uses the BIOS to determine the geometry for the given disk.

Arguments:

    DriveNumber - Supplies the drive number of the disk to query. Valid values
        are:

        0x80 - Boot drive.

        0x81, ... - Additional hard drives

        0x0, ... - Floppy drives.

    SectorCount - Supplies a pointer where the total number of sectors will be
        returned (one beyond the last valid LBA).

    SectorSize - Supplies a pointer where the size of a sector will be returned
        on success.

Return Value:

    STATUS_SUCCESS if the operation completed successfully.

    STATUS_FIRMWARE_ERROR if the BIOS returned an error.

    Other error codes.

--*/

{

    UINTN BufferAddress;
    PINT13_EXTENDED_DRIVE_PARAMETERS Parameters;
    BIOS_CALL_CONTEXT RealModeContext;
    EFI_STATUS Status;

    //
    // Create a standard BIOS call context.
    //

    Status = EfipCreateBiosCallContext(&RealModeContext, 0x13);
    if (EFI_ERROR(Status)) {
        goto GetDiskGeometryEnd;
    }

    //
    // Int 13 function 8 is "Get disk parameters". Ah takes the function number
    // (8), and dl takes the drive number.
    //

    RealModeContext.Eax = INT13_EXTENDED_GET_DRIVE_PARAMETERS << 8;
    RealModeContext.Edx = DriveNumber;
    RealModeContext.Ds = 0;
    BufferAddress = (UINTN)(RealModeContext.DataPage);
    RealModeContext.Esi = (UINT16)BufferAddress;
    Parameters = (PINT13_EXTENDED_DRIVE_PARAMETERS)(UINTN)BufferAddress;
    EfiSetMem(Parameters, sizeof(INT13_EXTENDED_DRIVE_PARAMETERS), 0);
    Parameters->PacketSize = sizeof(INT13_EXTENDED_DRIVE_PARAMETERS);

    //
    // Execute the firmware call.
    //

    EfipExecuteBiosCall(&RealModeContext);

    //
    // Check for an error. The status code is in Ah.
    //

    if (((RealModeContext.Eax & 0xFF00) != 0) ||
        ((RealModeContext.Eflags & IA32_EFLAG_CF) != 0)) {

        Status = EFI_NOT_FOUND;
        goto GetDiskGeometryEnd;
    }

    *SectorCount = Parameters->TotalSectorCount;
    *SectorSize = Parameters->SectorSize;
    Status = EFI_SUCCESS;

GetDiskGeometryEnd:
    EfipDestroyBiosCallContext(&RealModeContext);
    return Status;
}

EFI_STATUS
EfipPcatBlockOperation (
    PEFI_PCAT_DISK Disk,
    BOOLEAN Write,
    VOID *Buffer,
    UINT32 AbsoluteSector,
    UINT32 SectorCount
    )

/*++

Routine Description:

    This routine uses the BIOS to read from or write to the disk.

Arguments:

    Disk - Supplies a pointer to the disk to operate on.

    Write - Supplies a boolean indicating if this is a read (FALSE) or write
        (TRUE) operation.

    Buffer - Supplies the buffer either containing the data to write or where
        the read data will be returned.

    AbsoluteSector - Supplies the zero-based sector number to read from.

    SectorCount - Supplies the number of sectors to read. The supplied buffer
        must be at least this large.

Return Value:

    STATUS_SUCCESS if the operation completed successfully.

    STATUS_FIRMWARE_ERROR if the BIOS returned an error.

    Other error codes.

--*/

{

    UINTN RealModeBuffer;
    BIOS_CALL_CONTEXT RealModeContext;
    PINT13_DISK_ACCESS_PACKET Request;
    EFI_STATUS Status;

    //
    // Create a standard BIOS call context.
    //

    Status = EfipCreateBiosCallContext(&RealModeContext, 0x13);
    if (EFI_ERROR(Status)) {
        goto BlockOperationEnd;
    }

    //
    // Create the disk access packet on the stack.
    //

    Request = (PINT13_DISK_ACCESS_PACKET)(RealModeContext.Esp -
                                          sizeof(INT13_DISK_ACCESS_PACKET));

    Request->PacketSize = sizeof(INT13_DISK_ACCESS_PACKET);
    Request->Reserved = 0;
    Request->BlockCount = SectorCount;
    RealModeBuffer = (UINTN)(RealModeContext.DataPage);
    Request->TransferBuffer = (UINTN)RealModeBuffer;
    Request->BlockAddress = AbsoluteSector;
    RealModeContext.Edx = Disk->DriveNumber;
    RealModeContext.Esp = (UINTN)Request;
    RealModeContext.Esi = (UINTN)Request;
    if (Write != FALSE) {
        RealModeContext.Eax = INT13_EXTENDED_WRITE << 8;
        EfiCopyMem((VOID *)RealModeBuffer,
                   Buffer,
                   SectorCount * Disk->SectorSize);

    } else {
        RealModeContext.Eax = INT13_EXTENDED_READ << 8;
    }

    //
    // Execute the firmware call.
    //

    EfipExecuteBiosCall(&RealModeContext);

    //
    // Check for an error (carry flag set). The status code is in Ah.
    //

    if (((RealModeContext.Eax & 0xFF00) != 0) ||
        ((RealModeContext.Eflags & IA32_EFLAG_CF) != 0)) {

        Status = EFI_DEVICE_ERROR;
        goto BlockOperationEnd;
    }

    //
    // Copy the data over from the real mode data page to the caller's buffer.
    //

    if (Write == FALSE) {
        EfiCopyMem(Buffer,
                   (VOID *)RealModeBuffer,
                   SectorCount * Disk->SectorSize);
    }

    Status = EFI_SUCCESS;

BlockOperationEnd:
    EfipDestroyBiosCallContext(&RealModeContext);
    return Status;
}

EFI_STATUS
EfipPcatResetDisk (
    UINT8 DriveNumber
    )

/*++

Routine Description:

    This routine uses the BIOS to reset the disk.

Arguments:

    DriveNumber - Supplies the drive number of the disk to reset. Valid values
        are:

        0x80 - Boot drive.

        0x81, ... - Additional hard drives

        0x0, ... - Floppy drives.

Return Value:

    STATUS_SUCCESS if the operation completed successfully.

    STATUS_FIRMWARE_ERROR if the BIOS returned an error.

    Other error codes.

--*/

{

    BIOS_CALL_CONTEXT RealModeContext;
    EFI_STATUS Status;

    //
    // Create a standard BIOS call context.
    //

    Status = EfipCreateBiosCallContext(&RealModeContext, 0x13);
    if (EFI_ERROR(Status)) {
        goto PcatResetDiskEnd;
    }

    //
    // Int 13 function zero is reset.
    //

    RealModeContext.Eax = INT13_EXTENDED_GET_DRIVE_PARAMETERS << 8;
    RealModeContext.Edx = DriveNumber;

    //
    // Execute the firmware call.
    //

    EfipExecuteBiosCall(&RealModeContext);

    //
    // Check for an error. The status code is in Ah.
    //

    if (((RealModeContext.Eax & 0xFF00) != 0) ||
        ((RealModeContext.Eflags & IA32_EFLAG_CF) != 0)) {

        Status = EFI_DEVICE_ERROR;
        goto PcatResetDiskEnd;
    }

    Status = EFI_SUCCESS;

PcatResetDiskEnd:
    EfipDestroyBiosCallContext(&RealModeContext);
    return Status;
}

