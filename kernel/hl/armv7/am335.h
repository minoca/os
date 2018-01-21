/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    am335.h

Abstract:

    This header contains OS level definitions for the hardware modules
    supporting the TI AM335x SoCs.

Author:

    Evan Green 6-Jan-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

#define AM335_ALLOCATION_TAG 0x33336D41

//
// Define the signature of the AM335x ACPI table: AM33
//

#define AM335X_SIGNATURE 0x33334D41

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure describes the TI AM335x ACPI table.

Members:

    Header - Stores the standard ACPI table header. The signature here is
        'AM33'.

    TimerBase - Stores the array of physical addresses of all the timers.

    TimerGsi - Stores the array of Global System Interrupt numbers for each of
        the timers.

    InterruptLineCount - Stores the number of interrupt lines in the interrupt
        controller (one beyond the highest valid line number).

    InterruptControllerBase - Stores the physical address of the INTC interrupt
        controller unit.

    PrcmBase - Stores the physical address of the PRCM registers.

--*/

#pragma pack(push, 1)

typedef struct _AM335X_TABLE {
    DESCRIPTION_HEADER Header;
    ULONGLONG TimerBase[AM335X_TIMER_COUNT];
    ULONG TimerGsi[AM335X_TIMER_COUNT];
    ULONG InterruptLineCount;
    ULONGLONG InterruptControllerBase;
    ULONGLONG PrcmBase;
} PACKED AM335X_TABLE, *PAM335X_TABLE;

#pragma pack(pop)

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the AM335x ACPI table.
//

extern PAM335X_TABLE HlAm335Table;

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
HlpAm335InitializePowerAndClocks (
    VOID
    );

/*++

Routine Description:

    This routine initializes the PRCM and turns on clocks and power domains
    needed by the system.

Arguments:

    None.

Return Value:

    Status code.

--*/

