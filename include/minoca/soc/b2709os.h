/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    b2709os.h

Abstract:

    This header contains OS definitions for the BCM 2709 SoC.

Author:

    Chris Stevens 19-Mar-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the signature of the BCM 2709 ACPI table.
//

#define BCM2709_SIGNATURE 0x324D4342 // '2MCB'

//
// Define the flags for the CPU BCM2709 table entries.
//

#define BCM2709_CPU_FLAG_ENABLED 0x00000001

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _BCM2709_ENTRY_TYPE {
    Bcm2709EntryTypeCpu = 0x0,
} BCM2709_ENTRY_TYPE, *PBCM2709_ENTRY_TYPE;

/*++

Structure Description:

    This structure describes the BCM 2709 ACPI table.

Members:

    Header - Stores the standard ACPI table header. The signature here is
        '2MCB'.

    ApbClockFrequency - Stores the frequency of the Advanced Peripheral Bus's
        clock.

    InterruptControllerPhysicalAddress - Stores the physical address of the
        interrupt controller's register base.

    InterruptControllerGsiBase - Stores the Global System Interrupt number of
        the first line of the interrupt controller.

    ArmTimerPhysicalAddress - Stores the physical address of the ARM timer's
        registers.

    ArmTimerGsi - Stores the Global System Interrupt of the ARM timer.

    DebugUartPhysicalAddress - Stores the physical address of the UART used for
        serial debugging.

    DebugUartClockFrequency - Stores the frequency of the clock use for the
        UART.

    SystemTimerPhysicalAddress - Stores the physical address of the system
        timer's registers.

    SystemTimerFrequency - Stores the frequency of the system timer's
        free-running counter.

    SystemTimerGsiBase - Stores the Global System Interrupt base of the 4
        contiguous system timer interrupts.

    PwmClockFrequency - Stores the frequency of the Pulse Width Modulation
        clock.

    MailboxPhysicalAddress - Stores the physical address of the BCM2709 Mailbox
        register base.

    CpuLocalPhysicalAddress - Stores the physical address of the processor
        local registers.

--*/

typedef struct _BCM2709_TABLE {
    DESCRIPTION_HEADER Header;
    ULONGLONG ApbClockFrequency;
    ULONGLONG InterruptControllerPhysicalAddress;
    ULONGLONG InterruptControllerGsiBase;
    ULONGLONG ArmTimerPhysicalAddress;
    ULONG ArmTimerGsi;
    ULONGLONG DebugUartPhysicalAddress;
    ULONG DebugUartClockFrequency;
    ULONGLONG SystemTimerPhysicalAddress;
    ULONGLONG SystemTimerFrequency;
    ULONG SystemTimerGsiBase;
    ULONG PwmClockFrequency;
    ULONGLONG MailboxPhysicalAddress;
    ULONGLONG CpuLocalPhysicalAddress;
    // ProcessorStructures[n].
} PACKED BCM2709_TABLE, *PBCM2709_TABLE;

/*++

Structure Description:

    This structure describes an entry in the BCM2709 table whose content is not
    yet fully known.

Members:

    Type - Stores the type of entry, used to differentiate the various types
        of entries.

    Length - Stores the size of the entry, in bytes.

--*/

typedef struct _BCM2709_GENERIC_ENTRY {
    UCHAR Type;
    UCHAR Length;
} PACKED BCM2709_GENERIC_ENTRY, *PBCM2709_GENERIC_ENTRY;

/*++

Structure Description:

    This structure describes a BCM2709 CPU interface unit in the BCM2709 table.

Members:

    Type - Stores a value to indicate a BCM2709 CPU interface structure (0x0).

    Length - Stores the size of this structure, 24.

    Reserved - Stores a reserved value which must be zero.

    ProcessorId - Stores the physical ID of the processor.

    Flags - Stores flags governing this BCM2709 CPU interface. See
        BCM2709_CPU_FLAG_*.

    ParkingProtocolVersion - Stores the version of the ARM processor parking
        protocol implemented.

    ParkedAddress - Stores the physical address of the processor's parking
        protocol mailbox.

--*/

typedef struct _BCM2709_CPU_ENTRY {
    UCHAR Type;
    UCHAR Length;
    USHORT Reserved;
    ULONG ProcessorId;
    ULONG Flags;
    ULONG ParkingProtocolVersion;
    ULONGLONG ParkedAddress;
} PACKED BCM2709_CPU_ENTRY, *PBCM2709_CPU_ENTRY;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

