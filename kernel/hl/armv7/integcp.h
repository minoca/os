/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    integcp.h

Abstract:

    This header contains definitions for the Integrator/CP hardware modules.

Author:

    Evan Green 31-Oct-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the integrator allocation tag.
//

#define INTEGRATOR_ALLOCATION_TAG 0x50436E49 // 'PCnI'

//
// Define the signature of the Integrator/CP ACPI table.
//

#define INTEGRATORCP_SIGNATURE 0x50434E49 // 'PCNI'

//
// Define the default UART base and clock frequency if enumeration is forced.
//

#define INTEGRATORCP_UART_BASE 0x16000000
#define INTEGRATORCP_UART_CLOCK_FREQUENCY 14745600

//
// Define the size of the interrupt controller register space, in bytes.
//

#define INTEGRATORCP_INTERRUPT_CONTROLLER_SIZE 0x1000

//
// Define the number of interrupt lines on the Integrator/CP.
//

#define INTEGRATORCP_INTERRUPT_LINE_COUNT 32

//
// Define the number of timers in the Integrator/CP timer block.
//

#define INTEGRATORCP_TIMER_COUNT 3

//
// Define the fixed frequency for the second two Integrator/CP timers.
//

#define INTEGRATORCP_TIMER_FIXED_FREQUENCY 1000000

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure describes the Integrator/CP ACPI table.

Members:

    Header - Stores the standard ACPI table header. The signature here is
        'INCP'.

    Pl110PhysicalAddress - Stores the physical address of the PL110 LCD
        controller.

    InterruptControllerPhysicalAddress - Stores the physical address of the
        interrupt controller.

    InterruptControllerGsiBase - Stores the Global System Interrupt number of
        the first line of the interrupt controller.

    TimerBlockPhysicalAddress - Stores the physical address of the timer block.

    TimerGsi - Stores the Global System Interrupt numbers of the timers.

    DebugUartPhysicalAddress - Stores the physical address of the UART used for
        serial debugging.

    DebugUartClockFrequency - Stores the frequency of the clock use for the
        UART.

--*/

#pragma pack(push, 1)

typedef struct _INTEGRATORCP_TABLE {
    DESCRIPTION_HEADER Header;
    ULONGLONG Pl110PhysicalAddress;
    ULONGLONG InterruptControllerPhysicalAddress;
    ULONG InterruptControllerGsiBase;
    ULONGLONG TimerBlockPhysicalAddress;
    ULONG TimerGsi[INTEGRATORCP_TIMER_COUNT];
    ULONGLONG DebugUartPhysicalAddress;
    ULONG DebugUartClockFrequency;
} PACKED INTEGRATORCP_TABLE, *PINTEGRATORCP_TABLE;

#pragma pack(pop)

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the Integrator/CP ACPI table, if found.
//

extern PINTEGRATORCP_TABLE HlCpIntegratorTable;

//
// -------------------------------------------------------- Function Prototypes
//
