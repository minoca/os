/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    omap3.h

Abstract:

    This header defines support for OMAP3 hardware layer plugins.

Author:

    Evan Green 31-Oct-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

// Define the integrator allocation tag.
//

#define OMAP3_ALLOCATION_TAG 0x33504D4F // '3PMO'

//
// Define the signature of the OMAP3 ACPI table.
//

#define OMAP3_SIGNATURE 0x33504D4F // '3PMO'

//
// Define the number of timers in an OMAP3.
//

#define OMAP3_TIMER_COUNT 12

//
// Define the bit width for the timers.
//

#define OMAP3_TIMER_BIT_WIDTH 32

//
// Define the fixed frequency for the first timers.
//

#define OMAP3_TIMER_FIXED_FREQUENCY 32768

//
// Define the size of one timer's register space.
//

#define OMAP3_TIMER_CONTROLLER_SIZE 0x1000

//
// Define the number of *unique* interrupt priorities in the OMAP controller.
//

#define OMAP3_INTERRUPT_PRIORITY_COUNT 63

//
// Define the size of the interrupt controller register space.
//

#define OMAP3_INTERRUPT_CONTROLLER_SIZE 0x1000

//
// Define the number of interrupt lines in an OMAP3 interrupt controller.
//

#define OMAP3_INTERRUPT_LINE_COUNT 96

//
// Define the size of the PRCM register space.
//

#define OMAP3_PRCM_SIZE 0x2000

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure describes the OMAP3 ACPI table.

Members:

    Header - Stores the standard ACPI table header. The signature here is
        'OMP3'.

    InterruptControllerPhysicalAddress - Stores the physical address of the
        interrupt controller.

    InterruptControllerGsiBase - Stores the Global System Interrupt number of
        the first line of the interrupt controller.

    TimerBlockPhysicalAddress - Stores the physical address of the timer block.

    PrcmPhysicalAddress - Stores the physical address of the power and clock
        module.

    DebugUartPhysicalAddress - Stores the physical address of the UART used for
        serial debug communications.

--*/

#pragma pack(push, 1)

typedef struct _OMAP3_TABLE {
    DESCRIPTION_HEADER Header;
    ULONGLONG InterruptControllerPhysicalAddress;
    ULONG InterruptControllerGsiBase;
    ULONGLONG TimerPhysicalAddress[OMAP3_TIMER_COUNT];
    ULONG TimerGsi[OMAP3_TIMER_COUNT];
    ULONGLONG PrcmPhysicalAddress;
    ULONGLONG DebugUartPhysicalAddress;
} PACKED OMAP3_TABLE, *POMAP3_TABLE;

#pragma pack(pop)

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the OMAP3 ACPI table.
//

extern POMAP3_TABLE HlOmap3Table;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
HlpOmap3InitializePowerAndClocks (
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

