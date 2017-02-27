/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    bios.h

Abstract:

    This header contains definitions for PC/AT BIOS services.

Author:

    Evan Green 18-Jul-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the physical address where the EBDA (Extended BIOS Data Area) address
// is stored.
//

#define EBDA_POINTER_ADDRESS 0x40E

//
// Define the address and length of the space to search for the RSDP.
//

#define RSDP_SEARCH_ADDRESS (PVOID)0xE0000
#define RSDP_SEARCH_LENGTH 0x20000

//
// Define INT 10 functions.
//

#define INT10_SET_CURSOR_POSITION 0x02

//
// Define INT 13 functions.
//

#define INT13_READ_SECTORS                  0x02
#define INT13_WRITE_SECTORS                 0x03
#define INT13_GET_DRIVE_PARAMETERS          0x08
#define INT13_EXTENDED_READ                 0x42
#define INT13_EXTENDED_WRITE                0x43
#define INT13_EXTENDED_GET_DRIVE_PARAMETERS 0x48

//
// Define BIOS text mode information.
//

#define BIOS_TEXT_VIDEO_BASE 0xB8000
#define BIOS_TEXT_VIDEO_COLUMNS 80
#define BIOS_TEXT_VIDEO_ROWS 25
#define BIOS_TEXT_VIDEO_CELL_WIDTH 2

//
// --------------------------------------------------------------------- Macros
//

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _TEXT_COLOR {
    ColorBlack,
    ColorBlue,
    ColorGreen,
    ColorCyan,
    ColorRed,
    ColorMagenta,
    ColorBrown,
    ColorLightGray,
    ColorDarkGray,
    ColorBrightBlue,
    ColorBrightGreen,
    ColorBrightCyan,
    ColorBrightRed,
    ColorBrightMagenta,
    ColorYellow,
    ColorWhite
} TEXT_COLOR, *PTEXT_COLOR;

/*++

Structure Description:

    This structure defines a disk access packet used in the INT 13 calls.

Members:

    PacketSize - Stores the packet size of the packet, either 16 (this
        structure) or 24 if there is an additional quad word on the end
        containing the 64-bit transfer buffer.

    Reserved - Stores a reserved value. Set to zero.

    BlockCount - Stores the number of sectors to transfer.

    TransferBuffer - Stores a pointer to the data buffer, as a linear address.

    BlockAddress - Stores the absolute sector number to transfer. The first
        sector is zero.

--*/

typedef struct _INT13_DISK_ACCESS_PACKET {
    UCHAR PacketSize;
    UCHAR Reserved;
    USHORT BlockCount;
    ULONG TransferBuffer;
    ULONGLONG BlockAddress;
} PACKED INT13_DISK_ACCESS_PACKET, *PINT13_DISK_ACCESS_PACKET;

/*++

Structure Description:

    This structure defines the structure of the drive parameters returned from
    int 0x13 function AH=0x48 (extended read drive parameters).

Members:

    PacketSize - Stores the packet size of the packet, 0x1E bytes.

    InformationFlags - Stores various flags about the disk.

    Cylinders - Stores the number of cylinders on the disk (one beyond the last
        valid index).

    Heads - Stores the number of heads on the disk (one beyond the last valid
        index).

    SectorsPerTrack - Stores the number of sectors per track on the disk (the
        last valid index, since sector numbers start with one).

    TotalSectorCount - Stores the absolute number of sectors (one beyond the
        last valid index).

    SectorSize - Stores the number of bytes per sector.

    EnhancedDiskInformation - Stores an optional pointer to the enhanced drive
        information.

--*/

typedef struct _INT13_EXTENDED_DRIVE_PARAMETERS {
    USHORT PacketSize;
    USHORT InformationFlags;
    ULONG Cylinders;
    ULONG Heads;
    ULONG SectorsPerTrack;
    ULONGLONG TotalSectorCount;
    USHORT SectorSize;
    ULONG EnhancedDiskInformation;
} PACKED INT13_EXTENDED_DRIVE_PARAMETERS, *PINT13_EXTENDED_DRIVE_PARAMETERS;

//
// -------------------------------------------------------------------- Globals
//

//
// Store the frame buffer attributes.
//

extern ULONG FwFrameBufferMode;
extern PHYSICAL_ADDRESS FwFrameBufferPhysical;
extern ULONG FwFrameBufferWidth;
extern ULONG FwFrameBufferHeight;
extern ULONG FwFrameBufferBitsPerPixel;
extern ULONG FwFrameBufferRedMask;
extern ULONG FwFrameBufferGreenMask;
extern ULONG FwFrameBufferBlueMask;

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
FwPcatGetMemoryMap (
    PMEMORY_DESCRIPTOR_LIST MdlOut
    );

/*++

Routine Description:

    This routine gets the firmware memory map from the BIOS using int 15 E820
    calls.

Arguments:

    MdlOut - Supplies a pointer where the memory map information will be
        stored. This buffer must be allocated by the caller.

Return Value:

    STATUS_SUCCESS if one or more descriptors could be retrieved from the
        firmware.

    STATUS_UNSUCCESSFUL if no descriptors could be obtained from the firmware.

--*/

KSTATUS
FwPcatAllocatePages (
    PULONGLONG Address,
    ULONGLONG Size,
    ULONG Alignment,
    MEMORY_TYPE MemoryType
    );

/*++

Routine Description:

    This routine allocates physical pages for use.

Arguments:

    Address - Supplies a pointer to where the allocation will be returned.

    Size - Supplies the size of the required space.

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

KSTATUS
FwpPcatOpenBootDisk (
    ULONG BootDriveNumber,
    ULONGLONG PartitionOffset,
    PHANDLE Handle
    );

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

KSTATUS
FwpPcatOpenPartition (
    UCHAR PartitionId[FIRMWARE_PARTITION_ID_SIZE],
    PHANDLE Handle
    );

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

VOID
FwpPcatCloseDisk (
    HANDLE DiskHandle
    );

/*++

Routine Description:

    This routine closes an open disk.

Arguments:

    DiskHandle - Supplies a pointer to the open disk handle.

Return Value:

    None.

--*/

KSTATUS
FwpPcatReadSectors (
    HANDLE DiskHandle,
    ULONGLONG Sector,
    ULONG SectorCount,
    PVOID Buffer
    );

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

KSTATUS
FwpPcatWriteSectors (
    HANDLE DiskHandle,
    ULONGLONG Sector,
    ULONG SectorCount,
    PVOID Buffer
    );

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

ULONG
FwpPcatGetSectorSize (
    HANDLE DiskHandle
    );

/*++

Routine Description:

    This routine determines the number of bytes in a sector on the given disk.

Arguments:

    DiskHandle - Supplies a handle to the disk to query.

Return Value:

    Returns the number of bytes in a sector on success.

    0 on error.

--*/

ULONGLONG
FwpPcatGetSectorCount (
    HANDLE DiskHandle
    );

/*++

Routine Description:

    This routine determines the number of sectors on the disk.

Arguments:

    DiskHandle - Supplies a handle to the disk to query.

Return Value:

    Returns the number of sectors in the disk on success.

    0 on error.

--*/

VOID
FwpPcatGetDiskInformation (
    HANDLE DiskHandle,
    PULONG DriveNumber,
    PULONGLONG PartitionOffset
    );

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

KSTATUS
FwpPcatInitializeVideo (
    );

/*++

Routine Description:

    This routine attempts to initialize the video subsystem on a PCAT machine.

Arguments:

    None.

Return Value:

    Status code.

--*/

PVOID
FwPcatFindRsdp (
    VOID
    );

/*++

Routine Description:

    This routine attempts to find the ACPI RSDP table pointer on a PC-AT
    compatible system. It looks in the first 1k of the EBDA (Extended BIOS Data
    Area), as well as between the ranges 0xE0000 and 0xFFFFF. This routine
    must be run in physical mode.

Arguments:

    None.

Return Value:

    Returns a pointer to the RSDP table on success.

    NULL on failure.

--*/

PVOID
FwPcatFindSmbiosTable (
    VOID
    );

/*++

Routine Description:

    This routine attempts to find the SMBIOS table entry point structure.

Arguments:

    None.

Return Value:

    Returns a pointer to the SMBIOS entry point structure on success.

    NULL on failure.

--*/

KSTATUS
FwPcatGetCurrentTime (
    PSYSTEM_TIME Time
    );

/*++

Routine Description:

    This routine attempts to get the current system time.

Arguments:

    Time - Supplies a pointer where the current time will be returned.

Return Value:

    Status code.

--*/

KSTATUS
FwPcatStall (
    ULONG Microseconds
    );

/*++

Routine Description:

    This routine performs a short busy stall using INT 0x1A function 0, which
    returns a counter that increments 18.6025 times per second. Callers are
    advised to perform a "warm-up" stall to align to tick boundaries for more
    accurate results.

Arguments:

    Microseconds - Supplies the number of microseconds to stall for.

Return Value:

    Status code.

--*/

KSTATUS
FwPcatResetSystem (
    SYSTEM_RESET_TYPE ResetType
    );

/*++

Routine Description:

    This routine resets the system.

Arguments:

    ResetType - Supplies the desired reset type. If the desired reset type is
        not supported, a cold reset will be attempted.

Return Value:

    Does not return on success, the system is reset.

    STATUS_INVALID_PARAMETER if an invalid reset type was supplied.

    STATUS_NOT_SUPPORTED if the system cannot be reset.

    STATUS_UNSUCCESSFUL if the system did not reset.

--*/
