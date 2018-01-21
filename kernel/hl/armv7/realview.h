/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    realview.h

Abstract:

    This header contains definitions for the RealView hardware modules.

Author:

    Evan Green 14-Jul-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the signature of the RealView ACPI table.
//

#define REALVIEW_SIGNATURE 0x57564C52 // 'WVLR'

//
// Define the number of timers in the RealView timer block.
//

#define REALVIEW_TIMER_COUNT 2

//
// Define the default UART base and clock frequency if enumeration is forced.
//

#define REALVIEW_UART_BASE 0x10009000
#define REALVIEW_UART_CLOCK_FREQUENCY 14745600

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure describes the RealView ACPI table.

Members:

    Header - Stores the standard ACPI table header. The signature here is
        'WVLR'.

    Pl110PhysicalAddress - Stores the phyiscal address of the PL110 LCD
        controller.

    TimerPhysicalAddress - Stores the physical addressES of the timer blocks.

    TimerFrequency - Stores the frequencies of the timers, or zero for
        unknown.

    TimerGsi - Stores the Global System Interrupt numbers of the timers.

    DebugUartPhysicalAddress - Stores the physical address of the UART used for
        serial debugging.

    DebugUartClockFrequency - Stores the frequency of the clock use for the
        UART.

--*/

#pragma pack(push, 1)

typedef struct _REALVIEW_TABLE {
    DESCRIPTION_HEADER Header;
    ULONGLONG Pl110PhysicalAddress;
    ULONGLONG TimerPhysicalAddress[REALVIEW_TIMER_COUNT];
    ULONGLONG TimerFrequency[REALVIEW_TIMER_COUNT];
    ULONG TimerGsi[REALVIEW_TIMER_COUNT];
    ULONGLONG DebugUartPhysicalAddress;
    ULONG DebugUartClockFrequency;
} PACKED REALVIEW_TABLE, *PREALVIEW_TABLE;

#pragma pack(pop)

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
