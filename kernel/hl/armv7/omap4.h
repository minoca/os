/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    omap4.h

Abstract:

    This header defines support for OMAP4 hardware layer plugins.

Author:

    Evan Green 3-Nov-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

// Define the OMAP4 allocation tag.
//

#define OMAP4_ALLOCATION_TAG 0x34504D4F // '4PMO'

//
// Define the signature of the OMAP4 ACPI table.
//

#define OMAP4_SIGNATURE 0x34504D4F // '4PMO'

//
// Define the number of timers in an OMAP4.
//

#define OMAP4_TIMER_COUNT 11

//
// Define the bit width for the timers.
//

#define OMAP4_TIMER_BIT_WIDTH 32

//
// Define the fixed frequency for the first timer.
//

#define OMAP4_TIMER_FIXED_FREQUENCY 32768

//
// Define the size of one timer's register space.
//

#define OMAP4_TIMER_CONTROLLER_SIZE 0x1000

//
// This SMC command writes to the L2 cache debug register.
//

#define OMAP4_SMC_COMMAND_WRITE_L2_CACHE_DEBUG_REGISTER 0x100

//
// This SMC command cleans and invalidates a physical address range in the L2
// cache.
//

#define OMAP4_SMC_COMMAND_CLEAN_INVALIDATE_L2_CACHE_BY_PHYSICAL 0x101

//
// This SMC command writes to the L2 cache control register.
//

#define OMAP4_SMC_COMMAND_WRITE_L2_CACHE_CONTROL_REGISTER 0x102

//
// This SMC command writes to the auxiliary control register.
//

#define OMAP4_SMC_COMMAND_WRITE_AUXILIARY_CACHE_CONTROL 0x109

//
// This SMC command writes to the Tag and Data RAM latency control register.
//

#define OMAP4_SMC_COMMAND_WRITE_RAM_LATENCY_CONTROL_REGISTER 0x112

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure describes the OMAP4 ACPI table.

Members:

    Header - Stores the standard ACPI table header. The signature here is
        'OMP4'.

    TimerBlockPhysicalAddress - Stores the physical address of the timer block.

    DebugUartPhysicalAddress - Stores the physical address of the UART used for
        serial debug communications.

    WakeupClockPhysicalAddress - Stores the physical address of the wakeup
        clock management register interface (WKUP_CM).

    L4ClockPhysicalAddress - Stores the physical address of the L4 Peripheral
        Interconect clock management register interface (L4PER_CM2).

    AudioClockPhysicalAddress - Stores the physical address of the Audio
        Back-End clock management interface (ABE_CM1).

    Pl310RegistersBasePhysicalAddress - Stores the base physical address of
        the PL-310 cache controller's registers.

--*/

#pragma pack(push, 1)

typedef struct _OMAP4_TABLE {
    DESCRIPTION_HEADER Header;
    ULONGLONG TimerPhysicalAddress[OMAP4_TIMER_COUNT];
    ULONG TimerGsi[OMAP4_TIMER_COUNT];
    ULONGLONG DebugUartPhysicalAddress;
    ULONGLONG WakeupClockPhysicalAddress;
    ULONGLONG L4ClockPhysicalAddress;
    ULONGLONG AudioClockPhysicalAddress;
    ULONGLONG Pl310RegistersBasePhysicalAddress;
} PACKED OMAP4_TABLE, *POMAP4_TABLE;

#pragma pack(pop)

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the OMAP4 ACPI table.
//

extern POMAP4_TABLE HlOmap4Table;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
HlpOmap4InitializePowerAndClocks (
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

