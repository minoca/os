/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    biosfw.h

Abstract:

    This header contains definitions for the UEFI firmware on top of a legacy
    PC/AT BIOS.

Author:

    Evan Green 27-Feb-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// The ADDRESS_TO_SEGMENT macro converts a 32-bit address into a segment. It is
// assumed that the address being passed is 16-byte aligned.
//

#define ADDRESS_TO_SEGMENT(_Address) ((_Address) >> 4)

//
// The SEGMENTED_TO_LINEAR macro converts a segment:offset pair into a linear
// address.
//

#define SEGMENTED_TO_LINEAR(_Selector, _Offset)  \
    (((_Selector) << 4) + (_Offset))

//
// ---------------------------------------------------------------- Definitions
//

#define DEFAULT_FLAGS 0x00000202

#define IA32_EFLAG_CF 0x00000001

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
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines a BIOS call context, including all code, data, and
    stack memory, and registers.

Members:

    CodePage - Stores the code page information of the real mode operation.

    DataPage - Stores the data page of the real mode operation.

    StackPage - Stores the stack page of the real mode operation.

    Registers - Stores the register state of the real mode context. Upon exit,
        these fields contain the final register values.

--*/

typedef struct _BIOS_CALL_CONTEXT {
    VOID *CodePage;
    VOID *DataPage;
    VOID *StackPage;
    UINT32 Eax;
    UINT32 Ebx;
    UINT32 Ecx;
    UINT32 Edx;
    UINT32 Esi;
    UINT32 Edi;
    UINT32 Esp;
    UINT32 Ebp;
    UINT32 Eip;
    UINT32 Eflags;
    UINT32 Cs;
    UINT32 Ds;
    UINT32 Es;
    UINT32 Fs;
    UINT32 Gs;
    UINT32 Ss;
} BIOS_CALL_CONTEXT, *PBIOS_CALL_CONTEXT;

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
    UINT8 PacketSize;
    UINT8 Reserved;
    UINT16 BlockCount;
    UINT32 TransferBuffer;
    UINT64 BlockAddress;
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
    UINT16 PacketSize;
    UINT16 InformationFlags;
    UINT32 Cylinders;
    UINT32 Heads;
    UINT32 SectorsPerTrack;
    UINT64 TotalSectorCount;
    UINT16 SectorSize;
    UINT32 EnhancedDiskInformation;
} PACKED INT13_EXTENDED_DRIVE_PARAMETERS, *PINT13_EXTENDED_DRIVE_PARAMETERS;

//
// -------------------------------------------------------------------- Globals
//

//
// Save a pointer to the RSDP.
//

extern VOID *EfiRsdpPointer;

//
// -------------------------------------------------------- Function Prototypes
//

EFI_STATUS
EfipCreateBiosCallContext (
    PBIOS_CALL_CONTEXT Context,
    UINT8 InterruptNumber
    );

/*++

Routine Description:

    This routine initializes a standard real mode context for making a BIOS
    call via software interrupt (ie an int 0x10 call). It does not actually
    execute the context, it only initializes the data structures.

Arguments:

    Context - Supplies a pointer to the context structure that will be
        initialized. It is assumed this memory is already allocated.

    InterruptNumber - Supplies the interrupt number that will be called.

Return Value:

    Status code.

--*/

VOID
EfipDestroyBiosCallContext (
    PBIOS_CALL_CONTEXT Context
    );

/*++

Routine Description:

    This routine destroys a created BIOS call context.

Arguments:

    Context - Supplies a pointer to the context structure that will be
        freed.

Return Value:

    None.

--*/

VOID
EfipExecuteBiosCall (
    PBIOS_CALL_CONTEXT Context
    );

/*++

Routine Description:

    This routine executes 16-bit real mode code by switching the processor back
    to real mode.

Arguments:

    Context - Supplies a pointer to the context structure that will be
        executed. On return, this will contain the executed context.

Return Value:

    None.

--*/

VOID *
EfipPcatFindRsdp (
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

EFI_STATUS
EfipPcatInstallRsdp (
    VOID
    );

/*++

Routine Description:

    This routine installs the RSDP pointer as a configuration table in EFI.

Arguments:

    None.

Return Value:

    EFI status.

--*/

EFI_STATUS
EfipPcatInstallSmbios (
    VOID
    );

/*++

Routine Description:

    This routine installs the SMBIOS entry point structure as a configuration
    table in EFI.

Arguments:

    None.

Return Value:

    EFI status.

--*/

EFI_STATUS
EfipPcatEnumerateDisks (
    VOID
    );

/*++

Routine Description:

    This routine enumerates all the disks it can find on a BIOS machine.

Arguments:

    None.

Return Value:

    EFI Status code.

--*/

EFI_STATUS
EfipPcatEnumerateVideo (
    VOID
    );

/*++

Routine Description:

    This routine enumerates the video display on a BIOS machine.

Arguments:

    None.

Return Value:

    EFI Status code.

--*/

//
// Runtime functions
//

EFIAPI
VOID
EfipPcatResetSystem (
    EFI_RESET_TYPE ResetType,
    EFI_STATUS ResetStatus,
    UINTN DataSize,
    VOID *ResetData
    );

/*++

Routine Description:

    This routine resets the entire platform.

Arguments:

    ResetType - Supplies the type of reset to perform.

    ResetStatus - Supplies the status code for this reset.

    DataSize - Supplies the size of the reset data.

    ResetData - Supplies an optional pointer for reset types of cold, warm, or
        shutdown to a null-terminated string, optionally followed by additional
        binary data.

Return Value:

    None. This routine does not return.

--*/

VOID
EfipPcatInitializeReset (
    VOID
    );

/*++

Routine Description:

    This routine initializes support for reset system. This routine must run
    with boot services.

Arguments:

    None.

Return Value:

    None.

--*/

