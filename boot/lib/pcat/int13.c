/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    int13.c

Abstract:

    This module implements basic BIOS disk services using the INT 13h services.

Author:

    Evan Green 18-Jul-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/lib/partlib.h>
#include <minoca/kernel/x86.h>
#include "firmware.h"
#include "bootlib.h"
#include "realmode.h"
#include "bios.h"

//
// ---------------------------------------------------------------- Definitions
//

#define REAL_MODE_DATA_BUFFER_SIZE 0x1000

//
// Define the drive numbers to search for partitions.
//

#define PCAT_DRIVE_SEARCH_START 0x80
#define PCAT_DRIVE_SEARCH_END 0x90

//
// Define the number of retrys allowed for INT 13h calls.
//

#define PCAT_BLOCK_IO_RETRY_COUNT 5

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores internal state associated with an open PC/AT disk.

Members:

    DriveNumber - Stores the drive number of the open disk.

    TotalSectors - Stores the total number of sectors on the disk.

    PartitionOffset - Stores the offset in blocks to the start of the partition
        this open handle represents.

    SectorSize - Stores the size of a sector.

--*/

typedef struct _PCAT_DISK {
    UCHAR DriveNumber;
    ULONGLONG TotalSectors;
    ULONGLONG PartitionOffset;
    ULONG SectorSize;
} PCAT_DISK, *PPCAT_DISK;

/*++

Structure Description:

    This structure stores the combination of a partition context and a PC/AT
    disk handle. This is used when the partition library is trying to read
    disk sectors to enumerate partitions.

Members:

    PartitionContext - Supplies the partition context.

    Disk - Supplies the disk handle.

--*/

typedef struct _PCAT_PARTITION_ENUMERATION {
    PARTITION_CONTEXT PartitionContext;
    PCAT_DISK Disk;
} PCAT_PARTITION_ENUMERATION, *PPCAT_PARTITION_ENUMERATION;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
FwpPcatOpenPartitionOnDrive (
    UCHAR Drive,
    UCHAR PartitionId[FIRMWARE_PARTITION_ID_SIZE],
    PHANDLE Handle
    );

KSTATUS
FwpPcatGetDiskParameters (
    UCHAR DriveNumber,
    PULONGLONG SectorCount,
    PULONG SectorSize
    );

KSTATUS
FwpPcatBlockOperation (
    PPCAT_DISK Disk,
    BOOL Write,
    PVOID Buffer,
    ULONG AbsoluteSector,
    ULONG SectorCount
    );

PVOID
FwpPcatPartitionAllocate (
    UINTN Size
    );

VOID
FwpPcatPartitionFree (
    PVOID Memory
    );

KSTATUS
FwpPcatPartitionReadSectors (
    PPARTITION_CONTEXT Context,
    ULONGLONG BlockAddress,
    PVOID Buffer
    );

//
// -------------------------------------------------------------------- Globals
//

UCHAR BoBootDriveNumber;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
FwpPcatOpenBootDisk (
    ULONG BootDriveNumber,
    ULONGLONG PartitionOffset,
    PHANDLE Handle
    )

/*++

Routine Description:

    This routine attempts to open the boot disk device.

Arguments:

    BootDriveNumber - Supplies the drive number of the boot device.

    PartitionOffset - Supplies the offset in sectors of the active partition
        on this device, as discovered by earlier loader code.

    Handle - Supplies a pointer where a handle to the opened disk will be
        returned upon success.

Return Value:

    Status code.

--*/

{

    PPCAT_DISK Disk;
    KSTATUS Status;

    //
    // Save the boot drive number for guessing at disk numbers later.
    //

    BoBootDriveNumber = BootDriveNumber;
    Disk = BoAllocateMemory(sizeof(PCAT_DISK));
    if (Disk == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto PcatOpenBootDiskEnd;
    }

    RtlZeroMemory(Disk, sizeof(PCAT_DISK));
    Disk->DriveNumber = (UCHAR)BootDriveNumber;
    Status = FwpPcatGetDiskParameters(Disk->DriveNumber,
                                      &(Disk->TotalSectors),
                                      &(Disk->SectorSize));

    if (!KSUCCESS(Status)) {
        goto PcatOpenBootDiskEnd;
    }

    Disk->PartitionOffset = PartitionOffset;

    ASSERT(Disk->TotalSectors > Disk->PartitionOffset);

    Disk->TotalSectors -= PartitionOffset;
    Status = STATUS_SUCCESS;

PcatOpenBootDiskEnd:
    if (!KSUCCESS(Status)) {
        *Handle = NULL;

    } else {
        *Handle = (HANDLE)Disk;
    }

    return Status;
}

KSTATUS
FwpPcatOpenPartition (
    UCHAR PartitionId[FIRMWARE_PARTITION_ID_SIZE],
    PHANDLE Handle
    )

/*++

Routine Description:

    This routine opens a handle to a disk and partition with the given IDs.

Arguments:

    PartitionId - Supplies the partition identifier to match against.

    Handle - Supplies a pointer where a handle to the opened disk will be
        returned upon success.

Return Value:

    Status code.

--*/

{

    ULONG Drive;
    KSTATUS Status;

    //
    // Try the boot drive first, it's probably there.
    //

    Status = FwpPcatOpenPartitionOnDrive(BoBootDriveNumber,
                                         PartitionId,
                                         Handle);

    if (KSUCCESS(Status)) {
        goto PcatOpenPartitionEnd;
    }

    //
    // Also search all the hard drives enumerated by the BIOS.
    //

    for (Drive = PCAT_DRIVE_SEARCH_START;
         Drive < PCAT_DRIVE_SEARCH_END;
         Drive += 1) {

        //
        // Skip the boot drive, it was already tried.
        //

        if (Drive == BoBootDriveNumber) {
            continue;
        }

        Status = FwpPcatOpenPartitionOnDrive(Drive,
                                             PartitionId,
                                             Handle);

        if (KSUCCESS(Status)) {
            goto PcatOpenPartitionEnd;
        }
    }

    Status = STATUS_NO_SUCH_DEVICE;

PcatOpenPartitionEnd:
    if (!KSUCCESS(Status)) {
        *Handle = NULL;
    }

    return Status;
}

VOID
FwpPcatCloseDisk (
    HANDLE DiskHandle
    )

/*++

Routine Description:

    This routine closes an open disk.

Arguments:

    DiskHandle - Supplies a pointer to the open disk handle.

Return Value:

    None.

--*/

{

    BoFreeMemory(DiskHandle);
    return;
}

KSTATUS
FwpPcatReadSectors (
    HANDLE DiskHandle,
    ULONGLONG Sector,
    ULONG SectorCount,
    PVOID Buffer
    )

/*++

Routine Description:

    This routine uses the BIOS to read sectors off of a disk.

Arguments:

    DiskHandle - Supplies a handle to the disk to read from.

    Sector - Supplies the zero-based sector number to read from.

    SectorCount - Supplies the number of sectors to read. The supplied buffer
        must be at least this large.

    Buffer - Supplies the buffer where the data read from the disk will be
        returned upon success.

Return Value:

    STATUS_SUCCESS if the operation completed successfully.

    STATUS_FIRMWARE_ERROR if the BIOS returned an error.

    Other error codes.

--*/

{

    PPCAT_DISK Disk;
    ULONG MaxPerRound;
    ULONG SectorsThisRound;
    KSTATUS Status;

    Disk = (PPCAT_DISK)DiskHandle;
    MaxPerRound = REAL_MODE_DATA_BUFFER_SIZE / Disk->SectorSize;
    Sector += Disk->PartitionOffset;

    //
    // Iterate over the buffer reading the maximum allowed number of sectors
    // at a time.
    //

    Status = STATUS_SUCCESS;
    while (SectorCount != 0) {
        SectorsThisRound = MaxPerRound;
        if (SectorsThisRound > SectorCount) {
            SectorsThisRound = SectorCount;
        }

        //
        // The BIOS cannot read above 2TB (use UEFI and GPT).
        //

        ASSERT(Sector == (ULONG)Sector);

        Status = FwpPcatBlockOperation(Disk,
                                       FALSE,
                                       Buffer,
                                       Sector,
                                       SectorsThisRound);

        if (!KSUCCESS(Status)) {
            break;
        }

        Sector += SectorsThisRound;
        SectorCount -= SectorsThisRound;
        Buffer += (SectorsThisRound * Disk->SectorSize);
    }

    return Status;
}

KSTATUS
FwpPcatWriteSectors (
    HANDLE DiskHandle,
    ULONGLONG Sector,
    ULONG SectorCount,
    PVOID Buffer
    )

/*++

Routine Description:

    This routine uses the BIOS to write sectors to a disk.

Arguments:

    DiskHandle - Supplies a handle to the disk to write to.

    Sector - Supplies the zero-based sector number to write to.

    SectorCount - Supplies the number of sectors to write. The supplied buffer
        must be at least this large.

    Buffer - Supplies the buffer containing the data to write to the disk.

Return Value:

    STATUS_SUCCESS if the operation completed successfully.

    STATUS_FIRMWARE_ERROR if the BIOS returned an error.

    Other error codes.

--*/

{

    PPCAT_DISK Disk;
    ULONG MaxPerRound;
    ULONG SectorsThisRound;
    KSTATUS Status;

    Disk = (PPCAT_DISK)DiskHandle;
    MaxPerRound = REAL_MODE_DATA_BUFFER_SIZE / Disk->SectorSize;
    Sector += Disk->PartitionOffset;

    //
    // Iterate over the buffer writing the maximum allowed number of sectors
    // at a time.
    //

    Status = STATUS_SUCCESS;
    while (SectorCount != 0) {
        SectorsThisRound = MaxPerRound;
        if (SectorsThisRound > SectorCount) {
            SectorsThisRound = SectorCount;
        }

        //
        // The BIOS cannot read above 2TB (use UEFI and GPT).
        //

        ASSERT(Sector == (ULONG)Sector);

        Status = FwpPcatBlockOperation(Disk,
                                       TRUE,
                                       Buffer,
                                       Sector,
                                       SectorsThisRound);

        if (!KSUCCESS(Status)) {
            break;
        }

        Sector += SectorsThisRound;
        SectorCount -= SectorsThisRound;
        Buffer += (SectorsThisRound * Disk->SectorSize);
    }

    return Status;
}

ULONG
FwpPcatGetSectorSize (
    HANDLE DiskHandle
    )

/*++

Routine Description:

    This routine determines the number of bytes in a sector on the given disk.

Arguments:

    DiskHandle - Supplies a handle to the disk to query.

Return Value:

    Returns the number of bytes in a sector on success.

    0 on error.

--*/

{

    PPCAT_DISK Disk;

    Disk = (PPCAT_DISK)DiskHandle;
    return Disk->SectorSize;
}

ULONGLONG
FwpPcatGetSectorCount (
    HANDLE DiskHandle
    )

/*++

Routine Description:

    This routine determines the number of sectors on the disk.

Arguments:

    DiskHandle - Supplies a handle to the disk to query.

Return Value:

    Returns the number of sectors in the disk on success.

    0 on error.

--*/

{

    PPCAT_DISK Disk;

    Disk = (PPCAT_DISK)DiskHandle;
    return Disk->TotalSectors;
}

VOID
FwpPcatGetDiskInformation (
    HANDLE DiskHandle,
    PULONG DriveNumber,
    PULONGLONG PartitionOffset
    )

/*++

Routine Description:

    This routine returns information about an open disk handle.

Arguments:

    DiskHandle - Supplies a pointer to the open disk handle.

    DriveNumber - Supplies a pointer where the drive number of this disk
        handle will be returned.

    PartitionOffset - Supplies a pointer where the offset in sectors from the
        beginning of the disk to the partition this handle represents will be
        returned.

Return Value:

    None.

--*/

{

    PPCAT_DISK Disk;

    Disk = (PPCAT_DISK)DiskHandle;
    *DriveNumber = Disk->DriveNumber;
    *PartitionOffset = Disk->PartitionOffset;
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
FwpPcatOpenPartitionOnDrive (
    UCHAR Drive,
    UCHAR PartitionId[FIRMWARE_PARTITION_ID_SIZE],
    PHANDLE Handle
    )

/*++

Routine Description:

    This routine attempts to open a handle to the given partition on the given
    drive.

Arguments:

    Drive - Supplies the drive number.

    PartitionId - Supplies the partition identifier to match against.

    Handle - Supplies a pointer where a handle to the opened disk will be
        returned upon success.

Return Value:

    Status code.

--*/

{

    PCAT_PARTITION_ENUMERATION Context;
    PPCAT_DISK DiskHandle;
    BOOL Match;
    PPARTITION_INFORMATION Partition;
    BOOL PartitionContextInitialized;
    ULONG PartitionIndex;
    KSTATUS Status;

    DiskHandle = NULL;
    Partition = NULL;
    PartitionContextInitialized = FALSE;

    //
    // Initialize a local disk handle for the partition library.
    //

    RtlZeroMemory(&Context, sizeof(PCAT_PARTITION_ENUMERATION));
    Context.Disk.DriveNumber = Drive;
    Status = FwpPcatGetDiskParameters(Context.Disk.DriveNumber,
                                      &(Context.Disk.TotalSectors),
                                      &(Context.Disk.SectorSize));

    if (!KSUCCESS(Status)) {
        goto PcatOpenPartitionOnDriveEnd;
    }

    //
    // Ask the partition library to enumerate all the partitions on the drive.
    //

    Context.PartitionContext.AllocateFunction = FwpPcatPartitionAllocate;
    Context.PartitionContext.FreeFunction = FwpPcatPartitionFree;
    Context.PartitionContext.ReadFunction = FwpPcatPartitionReadSectors;
    Context.PartitionContext.BlockSize = Context.Disk.SectorSize;
    Context.PartitionContext.BlockCount = Context.Disk.TotalSectors;
    Status = PartInitialize(&(Context.PartitionContext));
    if (!KSUCCESS(Status)) {
        goto PcatOpenPartitionOnDriveEnd;
    }

    PartitionContextInitialized = TRUE;
    Status = PartEnumeratePartitions(&(Context.PartitionContext));
    if (!KSUCCESS(Status)) {
        goto PcatOpenPartitionOnDriveEnd;
    }

    //
    // Search through all the enumerated partitions looking for a match.
    //

    for (PartitionIndex = 0;
         PartitionIndex < Context.PartitionContext.PartitionCount;
         PartitionIndex += 1) {

        Partition = &(Context.PartitionContext.Partitions[PartitionIndex]);
        Match = RtlCompareMemory(PartitionId,
                                 Partition->Identifier,
                                 sizeof(Partition->Identifier));

        if (Match != FALSE) {
            break;
        }
    }

    //
    // Fail if no match was found.
    //

    if (PartitionIndex == Context.PartitionContext.PartitionCount) {
        Status = STATUS_NO_SUCH_DEVICE;
        goto PcatOpenPartitionOnDriveEnd;
    }

    //
    // Create a disk handle based on this partition.
    //

    DiskHandle = BoAllocateMemory(sizeof(PCAT_DISK));
    if (DiskHandle == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto PcatOpenPartitionOnDriveEnd;
    }

    RtlZeroMemory(DiskHandle, sizeof(PCAT_DISK));
    DiskHandle->DriveNumber = Drive;
    DiskHandle->TotalSectors = Partition->EndOffset - Partition->StartOffset;
    DiskHandle->PartitionOffset = Partition->StartOffset;
    DiskHandle->SectorSize = Context.Disk.SectorSize;
    Status = STATUS_SUCCESS;

PcatOpenPartitionOnDriveEnd:
    if (PartitionContextInitialized != FALSE) {
        PartDestroy(&(Context.PartitionContext));
    }

    if (!KSUCCESS(Status)) {
        if (DiskHandle != NULL) {
            BoFreeMemory(DiskHandle);
            DiskHandle = NULL;
        }
    }

    *Handle = DiskHandle;
    return Status;
}

KSTATUS
FwpPcatGetDiskParameters (
    UCHAR DriveNumber,
    PULONGLONG SectorCount,
    PULONG SectorSize
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
    REAL_MODE_CONTEXT RealModeContext;
    KSTATUS Status;

    //
    // Create a standard BIOS call context.
    //

    Status = FwpRealModeCreateBiosCallContext(&RealModeContext, 0x13);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // Int 13 function 8 is "Get disk parameters". Ah takes the function number
    // (8), and dl takes the drive number.
    //

    RealModeContext.Eax = INT13_EXTENDED_GET_DRIVE_PARAMETERS << 8;
    RealModeContext.Edx = DriveNumber;
    RealModeContext.Ds = 0;
    BufferAddress = RealModeContext.DataPage.RealModeAddress;

    ASSERT(BufferAddress == (USHORT)BufferAddress);

    RealModeContext.Esi = (USHORT)BufferAddress;
    Parameters = (PINT13_EXTENDED_DRIVE_PARAMETERS)(UINTN)BufferAddress;
    RtlZeroMemory(Parameters, sizeof(INT13_EXTENDED_DRIVE_PARAMETERS));
    Parameters->PacketSize = sizeof(INT13_EXTENDED_DRIVE_PARAMETERS);

    //
    // Execute the firmware call.
    //

    FwpRealModeExecute(&RealModeContext);

    //
    // Check for an error. The status code is in Ah.
    //

    if (((RealModeContext.Eax & 0xFF00) != 0) ||
        ((RealModeContext.Eflags & IA32_EFLAG_CF) != 0)) {

        Status = STATUS_FIRMWARE_ERROR;
        goto GetDiskGeometryEnd;
    }

    *SectorCount = Parameters->TotalSectorCount;
    *SectorSize = Parameters->SectorSize;
    Status = STATUS_SUCCESS;

GetDiskGeometryEnd:
    FwpRealModeDestroyBiosCallContext(&RealModeContext);
    return Status;
}

KSTATUS
FwpPcatBlockOperation (
    PPCAT_DISK Disk,
    BOOL Write,
    PVOID Buffer,
    ULONG AbsoluteSector,
    ULONG SectorCount
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
    REAL_MODE_CONTEXT RealModeContext;
    PINT13_DISK_ACCESS_PACKET Request;
    KSTATUS Status;
    ULONG Try;

    //
    // The real mode context only allocates a page for data/bounce buffers, so
    // this routine cannot do more I/O than that in one shot.
    //

    ASSERT(SectorCount * Disk->SectorSize <= REAL_MODE_DATA_BUFFER_SIZE);

    //
    // Create a standard BIOS call context.
    //

    Status = FwpRealModeCreateBiosCallContext(&RealModeContext, 0x13);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // Attempt to do the I/O, allowing it to retry a few times.
    //

    for (Try = 0; Try < PCAT_BLOCK_IO_RETRY_COUNT; Try += 1) {

        //
        // Create the disk access packet on the stack.
        //

        Request = (PINT13_DISK_ACCESS_PACKET)(RealModeContext.Esp -
                                              sizeof(INT13_DISK_ACCESS_PACKET));

        Request->PacketSize = sizeof(INT13_DISK_ACCESS_PACKET);
        Request->Reserved = 0;
        Request->BlockCount = SectorCount;
        RealModeBuffer = RealModeContext.DataPage.RealModeAddress;

        ASSERT(RealModeBuffer == (USHORT)RealModeBuffer);

        Request->TransferBuffer = (ULONG)RealModeBuffer;
        Request->BlockAddress = AbsoluteSector;
        RealModeContext.Edx = Disk->DriveNumber;
        RealModeContext.Esp = (UINTN)Request;
        RealModeContext.Esi = (UINTN)Request;
        if (Write != FALSE) {
            RealModeContext.Eax = INT13_EXTENDED_WRITE << BITS_PER_BYTE;
            RtlCopyMemory((PVOID)RealModeBuffer,
                          Buffer,
                          SectorCount * Disk->SectorSize);

        } else {
            RealModeContext.Eax = INT13_EXTENDED_READ << BITS_PER_BYTE;
        }

        //
        // Execute the firmware call.
        //

        FwpRealModeExecute(&RealModeContext);

        //
        // Check for an error (carry flag set). The status code is in Ah. If
        // there was no error, then move on.
        //

        if (((RealModeContext.Eax & 0xFF00) == 0) &&
            ((RealModeContext.Eflags & IA32_EFLAG_CF) == 0)) {

            Status = STATUS_SUCCESS;
            break;
        }

        //
        // If there was an error, reinitalize the context and try again.
        //

        FwpRealModeReinitializeBiosCallContext(&RealModeContext);
        Status = STATUS_FIRMWARE_ERROR;
    }

    if (!KSUCCESS(Status)) {
        goto BlockOperationEnd;
    }

    //
    // Copy the data over from the real mode data page to the caller's buffer.
    //

    if (Write == FALSE) {
        RtlCopyMemory(Buffer,
                      (PVOID)RealModeBuffer,
                      SectorCount * Disk->SectorSize);
    }

    Status = STATUS_SUCCESS;

BlockOperationEnd:
    FwpRealModeDestroyBiosCallContext(&RealModeContext);
    return Status;
}

PVOID
FwpPcatPartitionAllocate (
    UINTN Size
    )

/*++

Routine Description:

    This routine is called when the partition library needs to allocate memory.

Arguments:

    Size - Supplies the size of the allocation request, in bytes.

Return Value:

    Returns a pointer to the allocation if successful, or NULL if the
    allocation failed.

--*/

{

    return BoAllocateMemory(Size);
}

VOID
FwpPcatPartitionFree (
    PVOID Memory
    )

/*++

Routine Description:

    This routine is called when the partition library needs to free allocated
    memory.

Arguments:

    Memory - Supplies the allocation returned by the allocation routine.

Return Value:

    None.

--*/

{

    BoFreeMemory(Memory);
    return;
}

KSTATUS
FwpPcatPartitionReadSectors (
    PPARTITION_CONTEXT Context,
    ULONGLONG BlockAddress,
    PVOID Buffer
    )

/*++

Routine Description:

    This routine is called when the partition library needs to read a sector
    from the disk.

Arguments:

    Context - Supplies the partition context identifying the disk.

    BlockAddress - Supplies the block address to read.

    Buffer - Supplies a pointer where the data will be returned on success.
        This buffer is expected to be one block in size (as specified in the
        partition context).

Return Value:

    Status code.

--*/

{

    PPCAT_PARTITION_ENUMERATION Enumeration;
    KSTATUS Status;

    Enumeration = PARENT_STRUCTURE(Context,
                                   PCAT_PARTITION_ENUMERATION,
                                   PartitionContext);

    Status = FwpPcatReadSectors(&(Enumeration->Disk), BlockAddress, 1, Buffer);
    return Status;
}

