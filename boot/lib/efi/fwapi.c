/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    fwapi.c

Abstract:

    This module implements the firmware interface for x86 systems.

Author:

    Evan Green 11-Feb-2014

Environment:

    Boot

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/uefi/uefi.h>
#include "firmware.h"
#include "bootlibp.h"
#include <minoca/lib/basevid.h>
#include "efisup.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// Store pointers to the main EFI structures.
//

EFI_HANDLE BoEfiImageHandle;
EFI_SYSTEM_TABLE *BoEfiSystemTable;
EFI_BOOT_SERVICES *BoEfiBootServices;
EFI_RUNTIME_SERVICES *BoEfiRuntimeServices;

BASE_VIDEO_CONTEXT BoVideoContext;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
FwInitialize (
    ULONG Phase,
    PBOOT_INITIALIZATION_BLOCK Parameters
    )

/*++

Routine Description:

    This routine initializes the firmware library.

Arguments:

    Phase - Supplies the initialization phase. Phase 0 occurs very early,
        before the debugger. Phase 1 occurs after the debugger is online.

    Parameters - Supplies a pointer to the boot application initialization
        information.

Return Value:

    Status code.

--*/

{

    SYSTEM_RESOURCE_FRAME_BUFFER FrameBuffer;
    ULONG StackSize;
    KSTATUS Status;
    PVOID TopOfStack;

    //
    // Very early initialization, perform just what's needed to perform
    // debugger initilization. One of the things needed during this period is
    // to save the firmware context, as the debugger installs trap handlers,
    // etc.
    //

    if (Phase == 0) {
        BoEfiImageHandle = *((EFI_HANDLE *)(UINTN)(Parameters->EfiImageHandle));
        BoEfiSystemTable = (PVOID)(UINTN)(Parameters->EfiSystemTable);
        BoEfiBootServices = BoEfiSystemTable->BootServices;
        BoEfiRuntimeServices = BoEfiSystemTable->RuntimeServices;
        BopEfiArchInitialize(&TopOfStack, &StackSize);
        if (Parameters->StackSize == 0) {
            Parameters->StackTop = (UINTN)TopOfStack;
            Parameters->StackSize = StackSize;
        }

        //
        // Look for and enumerate a debug device using the Serial I/O protocol.
        //

        BopEfiGetDebugDevice();

    //
    // Phase 1 initialization comes after the debugger comes online. This is
    // where most of the initialization occurs.
    //

    } else {

        ASSERT(Phase == 1);
        ASSERT(Parameters->ReservedRegionCount == 0);

        Status = BopEfiInitializeMemory();
        if (!KSUCCESS(Status)) {
            goto InitializeEnd;
        }

        Status = BopInitializeMemory(Parameters);
        if (!KSUCCESS(Status)) {
            goto InitializeEnd;
        }

        //
        // Initialize video services.
        //

        BopEfiInitializeVideo();
        RtlZeroMemory(&FrameBuffer, sizeof(SYSTEM_RESOURCE_FRAME_BUFFER));
        Status = BopEfiGetVideoInformation(
                                         &(FrameBuffer.Width),
                                         &(FrameBuffer.Height),
                                         &(FrameBuffer.PixelsPerScanLine),
                                         &(FrameBuffer.BitsPerPixel),
                                         &(FrameBuffer.RedMask),
                                         &(FrameBuffer.GreenMask),
                                         &(FrameBuffer.BlueMask),
                                         &(FrameBuffer.Header.PhysicalAddress),
                                         &(FrameBuffer.Header.Size));

        if (KSUCCESS(Status)) {
            FrameBuffer.Mode = BaseVideoModeFrameBuffer;

            ASSERT((UINTN)FrameBuffer.Header.PhysicalAddress ==
                   FrameBuffer.Header.PhysicalAddress);

            FrameBuffer.Header.VirtualAddress =
                              (PVOID)(UINTN)FrameBuffer.Header.PhysicalAddress;

            Status = VidInitialize(&BoVideoContext, &FrameBuffer);
            if (!KSUCCESS(Status)) {
                goto InitializeEnd;
            }

            if ((Parameters->Flags &
                 BOOT_INITIALIZATION_FLAG_SCREEN_CLEAR) == 0) {

                VidClearScreen(&BoVideoContext, 0, 0, -1, -1);
            }
        }
    }

    Status = STATUS_SUCCESS;

InitializeEnd:
    return Status;
}

VOID
FwDestroy (
    VOID
    )

/*++

Routine Description:

    This routine destroys the firmware layer upon failure. This routine
    releases any resources registered with the firmware on behalf of this boot
    application. It is not called in success cases.

Arguments:

    None.

Return Value:

    None.

--*/

{

    BopEfiDestroyMemory();
    BopEfiRestoreFirmwareContext();
    return;
}

VOID
FwClearScreen (
    ULONG MinimumX,
    ULONG MinimumY,
    ULONG MaximumX,
    ULONG MaximumY
    )

/*++

Routine Description:

    This routine clears a region of the screen, filling it with the default
    fill character.

Arguments:

    MinimumX - Supplies the minimum X coordinate of the rectangle to clear,
        inclusive.

    MinimumY - Supplies the minimum Y coordinate of the rectangle to clear,
        inclusive.

    MaximumX - Supplies the maximum X coordinate of the rectangle to clear,
        exclusive.

    MaximumY - Supplies the maximum Y coordinate of the rectangle to clear,
        exclusive.

Return Value:

    None.

--*/

{

    VidClearScreen(&BoVideoContext, MinimumX, MinimumY, MaximumX, MaximumY);
    return;
}

KSTATUS
FwAllocatePages (
    PULONGLONG Address,
    ULONGLONG Size,
    ULONG Alignment,
    MEMORY_TYPE MemoryType
    )

/*++

Routine Description:

    This routine allocates physical pages for use.

Arguments:

    Address - Supplies a pointer to where the allocation will be returned.

    Size - Supplies the size of the required space, in bytes.

    Alignment - Supplies the alignment requirement for the allocation, in bytes.
        Valid values are powers of 2. Set to 1 or 0 to specify no alignment
        requirement.

    MemoryType - Supplies the type of memory to mark the allocation as.

Return Value:

    STATUS_SUCCESS if the allocation was successful.

    STATUS_INVALID_PARAMETER if a page count of 0 was passed or the address
        parameter was not filled out.

    STATUS_NO_MEMORY if the allocation request could not be filled.

--*/

{

    ULONGLONG Allocation;
    MEMORY_DESCRIPTOR Descriptor;
    ULONGLONG PaddedSize;
    ULONG PageSize;
    KSTATUS Status;

    //
    // If the alignment is less than or equal to what EFI always hands back,
    // just forward the call on.
    //

    if (Alignment <= EFI_PAGE_SIZE) {
        Status = BopEfiLoaderAllocatePages(Address, Size, MemoryType);
        return Status;
    }

    //
    // Add the alignment to the size so that there's an aligned size somewhere
    // in there.
    //

    ASSERT(POWER_OF_2(Alignment));

    PaddedSize = Size + Alignment;
    Status = BopEfiLoaderAllocatePages(&Allocation, PaddedSize, MemoryType);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    *Address = ALIGN_RANGE_UP(Allocation, Alignment);

    //
    // Now try to free any space before and after if it's available.
    //

    PageSize = MmPageSize();
    if (*Address - Allocation >= PageSize) {
        MmMdInitDescriptor(&Descriptor,
                           Allocation,
                           *Address,
                           MemoryTypeLoaderTemporary);

        MmMdAddDescriptorToList(&BoMemoryMap, &Descriptor);
    }

    if (Allocation + PaddedSize - (*Address + Size) >= PageSize) {
        MmMdInitDescriptor(&Descriptor,
                           *Address + Size,
                           Allocation + PaddedSize,
                           MemoryTypeLoaderTemporary);

        MmMdAddDescriptorToList(&BoMemoryMap, &Descriptor);
    }

    return Status;
}

VOID
FwPrintString (
    ULONG XCoordinate,
    ULONG YCoordinate,
    PSTR String
    )

/*++

Routine Description:

    This routine prints a null-terminated string to the screen at the
    specified location.

Arguments:

    XCoordinate - Supplies the X coordinate of the location on the screen
        to write to.

    YCoordinate - Supplies the Y cooordinate of the location on the screen
        to write to.

    String - Supplies the string to print.

Return Value:

    None.

--*/

{

    VidPrintString(&BoVideoContext, XCoordinate, YCoordinate, String);
    return;
}

VOID
FwPrintHexInteger (
    ULONG XCoordinate,
    ULONG YCoordinate,
    ULONG Number
    )

/*++

Routine Description:

    This routine prints an integer to the screen in the specified location.

Arguments:

    XCoordinate - Supplies the X coordinate of the location on the screen
        to write to.

    YCoordinate - Supplies the Y cooordinate of the location on the screen
        to write to.

    Number - Supplies the signed integer to print.

Return Value:

    None.

--*/

{

    VidPrintHexInteger(&BoVideoContext, XCoordinate, YCoordinate, Number);
    return;
}

VOID
FwPrintInteger (
    ULONG XCoordinate,
    ULONG YCoordinate,
    LONG Number
    )

/*++

Routine Description:

    This routine prints an integer to the screen in the specified location.

Arguments:

    XCoordinate - Supplies the X coordinate of the location on the screen
        to write to.

    YCoordinate - Supplies the Y cooordinate of the location on the screen
        to write to.

    Number - Supplies the signed integer to print.

Return Value:

    None.

--*/

{

    VidPrintInteger(&BoVideoContext, XCoordinate, YCoordinate, Number);
    return;
}

KSTATUS
FwOpenBootDisk (
    ULONG BootDriveNumber,
    ULONGLONG PartitionOffset,
    PBOOT_ENTRY BootEntry,
    PHANDLE Handle
    )

/*++

Routine Description:

    This routine attempts to open the boot disk device.

Arguments:

    BootDriveNumber - Supplies the drive number of the boot device, for PC/AT
        systems.

    PartitionOffset - Supplies the offset in sectors to apply to the beginning
        of the disk to get to the boot partition. This only applies to PC/AT
        systems where the partition offset is coded into the Volume Boot
        Record.

    BootEntry - Supplies an optional pointer to the boot entry, used by EFI
        systems to look up a disk by GUID.

    Handle - Supplies a pointer where a handle to the opened disk will be
        returned upon success.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    ASSERT(PartitionOffset == 0);

    //
    // If the caller is requesting a volume for a particular boot entry, open
    // the partition based on its GUID. Otherwise, open the volume associated
    // with the EFI boot application handle.
    //

    if (BootEntry != NULL) {
        Status = FwOpenPartition(BootEntry->PartitionId, Handle);

    } else {
        Status = BopEfiOpenBootDisk(Handle);
    }

    return Status;
}

KSTATUS
FwOpenPartition (
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

    return BopEfiOpenPartition(PartitionId, Handle);
}

VOID
FwCloseDisk (
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

    BopEfiCloseDisk(DiskHandle);
    return;
}

KSTATUS
FwReadDiskSectors (
    HANDLE DiskHandle,
    ULONGLONG Sector,
    ULONG SectorCount,
    PVOID Buffer
    )

/*++

Routine Description:

    This routine uses firmware calls read sectors off of a disk.

Arguments:

    DiskHandle - Supplies a handle to the disk to read from.

    Sector - Supplies the zero-based sector number to read from.

    SectorCount - Supplies the number of sectors to read. The supplied buffer
        must be at least this large.

    Buffer - Supplies the buffer where the data read from the disk will be
        returned upon success.

Return Value:

    STATUS_SUCCESS if the operation completed successfully.

    STATUS_FIRMWARE_ERROR if the firmware returned an error.

    Other error codes.

--*/

{

    return BopEfiLoaderBlockIoRead(DiskHandle, Sector, SectorCount, Buffer);
}

KSTATUS
FwWriteDiskSectors (
    HANDLE DiskHandle,
    ULONGLONG Sector,
    ULONG SectorCount,
    PVOID Buffer
    )

/*++

Routine Description:

    This routine uses firmware calls to write sectors to a disk.

Arguments:

    DiskHandle - Supplies a handle to the disk to write to.

    Sector - Supplies the zero-based sector number to write to.

    SectorCount - Supplies the number of sectors to write. The supplied buffer
        must be at least this large.

    Buffer - Supplies the buffer containing the data to write to the disk.

Return Value:

    STATUS_SUCCESS if the operation completed successfully.

    STATUS_FIRMWARE_ERROR if the firmware returned an error.

    Other error codes.

--*/

{

    return BopEfiLoaderBlockIoWrite(DiskHandle, Sector, SectorCount, Buffer);
}

ULONG
FwGetDiskSectorSize (
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

    return BopEfiGetDiskBlockSize(DiskHandle);
}

ULONGLONG
FwGetDiskSectorCount (
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

    return BopEfiGetDiskBlockCount(DiskHandle);
}

KSTATUS
FwGetRamDisks (
    PBOOT_RAM_DISK *RamDisks,
    PULONG RamDiskCount
    )

/*++

Routine Description:

    This routine returns an array of the RAM disks known to the firmware.

Arguments:

    RamDisks - Supplies a pointer where an array of RAM disk structures will
        be allocated and returned. It is the caller's responsibility to free
        this memory.

    RamDiskCount - Supplies a pointer where the count of RAM disks in the
        array will be returned.

Return Value:

    Status code.

--*/

{

    return BopEfiGetRamDisks(RamDisks, RamDiskCount);
}

PVOID
FwFindRsdp (
    VOID
    )

/*++

Routine Description:

    This routine attempts to find the ACPI RSDP table pointer.

Arguments:

    None.

Return Value:

    Returns a pointer to the RSDP table on success.

    NULL on failure.

--*/

{

    PVOID Rsdp;
    KSTATUS Status;

    Rsdp = NULL;

    //
    // Try to find the RSDP using the newer GUID first. If that doesn't work,
    // search on the older ACPI 1.0 GUID as well.
    //

    Status = BopEfiGetSystemConfigurationTable(&BoEfiAcpiTableGuid, &Rsdp);
    if (KSUCCESS(Status)) {
        return Rsdp;
    }

    Status = BopEfiGetSystemConfigurationTable(&BoEfiAcpi1TableGuid, &Rsdp);
    if (KSUCCESS(Status)) {
        return Rsdp;
    }

    return NULL;
}

PVOID
FwFindSmbiosTable (
    VOID
    )

/*++

Routine Description:

    This routine attempts to find the SMBIOS table entry point structure.

Arguments:

    None.

Return Value:

    Returns a pointer to the SMBIOS entry point structure on success.

    NULL on failure.

--*/

{

    PVOID SmbiosTable;
    KSTATUS Status;

    SmbiosTable = NULL;
    Status = BopEfiGetSystemConfigurationTable(&BoEfiSmbiosTableGuid,
                                               &SmbiosTable);

    if (KSUCCESS(Status)) {
        return SmbiosTable;
    }

    return NULL;
}

KSTATUS
FwGetCurrentTime (
    PSYSTEM_TIME Time
    )

/*++

Routine Description:

    This routine attempts to get the current system time.

Arguments:

    Time - Supplies a pointer where the current time will be returned.

Return Value:

    Status code.

--*/

{

    return BopEfiGetCurrentTime(Time);
}

KSTATUS
FwStall (
    ULONG Microseconds
    )

/*++

Routine Description:

    This routine performs a short busy stall using firmware services.
    Depending on the implementation, firmware may stall for a longer period
    than requested. Depending on the implementation, callers may get more
    accurate results by performing a "warm-up" stall to align to tick
    boundaries.

Arguments:

    Microseconds - Supplies the number of microseconds to stall for.

Return Value:

    Status code.

--*/

{

    EFI_STATUS Status;

    Status = BopEfiStall(Microseconds);
    if (EFI_ERROR(Status)) {
        return BopEfiStatusToKStatus(Status);
    }

    return STATUS_SUCCESS;
}

KSTATUS
FwResetSystem (
    SYSTEM_RESET_TYPE ResetType,
    PVOID Data,
    UINTN Size
    )

/*++

Routine Description:

    This routine resets the system.

Arguments:

    ResetType - Supplies the desired reset type. If the desired reset type is
        not supported, a cold reset will be attempted.

    Data - Supplies a pointer to platform-specific reboot data.

    Size - Supplies the size of the platform-specific data in bytes.

Return Value:

    Does not return on success, the system is reset.

    STATUS_INVALID_PARAMETER if an invalid reset type was supplied.

    STATUS_NO_INTERFACE if there are no appropriate reboot capababilities
    registered with the system.

    Other status codes on other failures.

--*/

{

    EFI_RESET_TYPE EfiResetType;

    switch (ResetType) {
    case SystemResetShutdown:
        EfiResetType = EfiResetShutdown;
        break;

    case SystemResetCold:
        EfiResetType = EfiResetCold;
        break;

    case SystemResetWarm:
    default:
        EfiResetType = EfiResetWarm;
        break;
    }

    BopEfiResetSystem(EfiResetType, 0, Size, Data);
    return STATUS_UNSUCCESSFUL;
}

BOOL
FwIsEfi (
    VOID
    )

/*++

Routine Description:

    This routine returns whether or not the firmware support layer is UEFI
    based.

Arguments:

    None.

Return Value:

    TRUE if this is an EFI system.

    FALSE if this is not an EFI system.

--*/

{

    return TRUE;
}

//
// --------------------------------------------------------- Internal Functions
//

