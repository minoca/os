/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    bcm2709.h

Abstract:

    This header contains definitions for the BCM 2709 SoC.

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
// ------------------------------------------------------ Data Type Definitions
//

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

    MailboxPhysicalAddress - Stores the physical address of the BCM2709 Mailbox
        register base.

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
    ULONGLONG MailboxPhysicalAddress;
} PACKED BCM2709_TABLE, *PBCM2709_TABLE;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

