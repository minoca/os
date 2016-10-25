/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    am335pwr.c

Abstract:

    This module implements clock and power support for AM335x SoCs.

Author:

    Evan Green 6-Jan-2015

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// Include kernel.h, but be cautious about which APIs are used. Most of the
// system depends on the hardware modules. Limit use to HL, RTL and AR routines.
//

#include <minoca/kernel/kernel.h>
#include <minoca/soc/am335x.h>
#include "am335.h"

//
// --------------------------------------------------------------------- Macros
//

#define AM335_CM_DPLL_READ(_Register) \
    HlReadRegister32(HlAm335Prcm + AM335_CM_DPLL_OFFSET + (_Register))

#define AM335_CM_DPLL_WRITE(_Register, _Value)                          \
    HlWriteRegister32(HlAm335Prcm + AM335_CM_DPLL_OFFSET + (_Register), \
                      (_Value))

#define AM335_CM_PER_READ(_Register) \
    HlReadRegister32(HlAm335Prcm + AM335_CM_PER_OFFSET + (_Register))

#define AM335_CM_PER_WRITE(_Register, _Value)                          \
    HlWriteRegister32(HlAm335Prcm + AM335_CM_PER_OFFSET + (_Register), \
                      (_Value))

#define AM335_CM_WAKEUP_READ(_Register) \
    HlReadRegister32(HlAm335Prcm + AM335_CM_WAKEUP_OFFSET + (_Register))

#define AM335_CM_WAKEUP_WRITE(_Register, _Value)                          \
    HlWriteRegister32(HlAm335Prcm + AM335_CM_WAKEUP_OFFSET + (_Register), \
                      (_Value))

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// Store virtual address register bases.
//

PVOID HlAm335Prcm;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
HlpAm335InitializePowerAndClocks (
    VOID
    )

/*++

Routine Description:

    This routine initializes the PRCM and turns on clocks and power domains
    needed by the system.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;
    ULONG Value;

    if (HlAm335Prcm == NULL) {
        HlAm335Prcm = HlMapPhysicalAddress(HlAm335Table->PrcmBase,
                                           AM335_PRCM_SIZE,
                                           TRUE);

        if (HlAm335Prcm == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto InitializePowerAndClocksEnd;
        }
    }

    //
    // Select the 32kHz source for timer 1. Timer 0 is fixed at 32kHz.
    //

    Value = AM335_CM_DPLL_CLOCK_SELECT_TIMER1_32KHZ;
    AM335_CM_DPLL_WRITE(AM335_CM_DPLL_CLOCK_SELECT_TIMER1, Value);

    //
    // Select the system clock source for all the other timers.
    //

    Value = AM335_CM_DPLL_CLOCK_SELECT_TIMER_SYSTEM_CLOCK;
    AM335_CM_DPLL_WRITE(AM335_CM_DPLL_CLOCK_SELECT_TIMER2, Value);
    AM335_CM_DPLL_WRITE(AM335_CM_DPLL_CLOCK_SELECT_TIMER3, Value);
    AM335_CM_DPLL_WRITE(AM335_CM_DPLL_CLOCK_SELECT_TIMER4, Value);
    AM335_CM_DPLL_WRITE(AM335_CM_DPLL_CLOCK_SELECT_TIMER5, Value);
    AM335_CM_DPLL_WRITE(AM335_CM_DPLL_CLOCK_SELECT_TIMER6, Value);
    AM335_CM_DPLL_WRITE(AM335_CM_DPLL_CLOCK_SELECT_TIMER7, Value);

    //
    // Enable all timers.
    //

    Value = AM335_CM_WAKEUP_TIMER0_CLOCK_ENABLE;
    AM335_CM_WAKEUP_WRITE(AM335_CM_WAKEUP_TIMER0_CLOCK_CONTROL, Value);
    AM335_CM_WAKEUP_WRITE(AM335_CM_WAKEUP_TIMER1_CLOCK_CONTROL, Value);
    Value = AM335_CM_PER_TIMER2_CLOCK_ENABLE;
    AM335_CM_PER_WRITE(AM335_CM_PER_TIMER2_CLOCK_CONTROL, Value);
    AM335_CM_PER_WRITE(AM335_CM_PER_TIMER3_CLOCK_CONTROL, Value);
    AM335_CM_PER_WRITE(AM335_CM_PER_TIMER4_CLOCK_CONTROL, Value);
    AM335_CM_PER_WRITE(AM335_CM_PER_TIMER5_CLOCK_CONTROL, Value);
    AM335_CM_PER_WRITE(AM335_CM_PER_TIMER6_CLOCK_CONTROL, Value);
    AM335_CM_PER_WRITE(AM335_CM_PER_TIMER7_CLOCK_CONTROL, Value);
    Status = STATUS_SUCCESS;

InitializePowerAndClocksEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

