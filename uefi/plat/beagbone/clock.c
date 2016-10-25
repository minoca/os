/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    clock.c

Abstract:

    This module initializes power and clocks for TI AM335x SoCs.

Author:

    Evan Green 19-Dec-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include <minoca/soc/am335x.h>
#include "bbonefw.h"

//
// --------------------------------------------------------------------- Macros
//

#define AM335_CM_DPLL_READ(_Register)                                       \
    EfiReadRegister32((VOID *)(AM335_SOC_CM_DPLL_REGISTERS + (_Register)))

#define AM335_CM_DPLL_WRITE(_Register, _Value)                              \
    EfiWriteRegister32((VOID *)(AM335_SOC_CM_DPLL_REGISTERS + (_Register)), \
                       (_Value))

#define AM335_CM_PER_READ(_Register)                                        \
    EfiReadRegister32((VOID *)(AM335_CM_PER_REGISTERS + (_Register)))

#define AM335_CM_PER_WRITE(_Register, _Value)                               \
    EfiWriteRegister32((VOID *)(AM335_CM_PER_REGISTERS + (_Register)),      \
                       (_Value))

#define AM335_CM_WAKEUP_READ(_Register)                                     \
    EfiReadRegister32((VOID *)(AM335_CM_WAKEUP_REGISTERS + (_Register)))

#define AM335_CM_WAKEUP_WRITE(_Register, _Value)                            \
    EfiWriteRegister32((VOID *)(AM335_CM_WAKEUP_REGISTERS + (_Register)),   \
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
// ------------------------------------------------------------------ Functions
//

VOID
EfipAm335InitializePowerAndClocks (
    VOID
    )

/*++

Routine Description:

    This routine initializes power and clocks for the UEFI firmware on the TI
    AM335x SoC.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UINT32 Value;

    //
    // Select the 32kHz source for timer 2. Timer 0 is fixed at 32kHz.
    //

    Value = AM335_CM_DPLL_CLOCK_SELECT_TIMER_32KHZ;
    AM335_CM_DPLL_WRITE(AM335_CM_DPLL_CLOCK_SELECT_TIMER2, Value);

    //
    // Select the system clock source for all the other timers.
    //

    Value = AM335_CM_DPLL_CLOCK_SELECT_TIMER_SYSTEM_CLOCK;
    AM335_CM_DPLL_WRITE(AM335_CM_DPLL_CLOCK_SELECT_TIMER3, Value);
    AM335_CM_DPLL_WRITE(AM335_CM_DPLL_CLOCK_SELECT_TIMER4, Value);
    AM335_CM_DPLL_WRITE(AM335_CM_DPLL_CLOCK_SELECT_TIMER5, Value);
    AM335_CM_DPLL_WRITE(AM335_CM_DPLL_CLOCK_SELECT_TIMER6, Value);
    AM335_CM_DPLL_WRITE(AM335_CM_DPLL_CLOCK_SELECT_TIMER7, Value);

    //
    // Enable timers 0 and 2.
    //

    Value = AM335_CM_WAKEUP_TIMER0_CLOCK_ENABLE;
    AM335_CM_WAKEUP_WRITE(AM335_CM_WAKEUP_TIMER0_CLOCK_CONTROL, Value);
    Value = AM335_CM_PER_TIMER2_CLOCK_ENABLE;
    AM335_CM_PER_WRITE(AM335_CM_PER_TIMER2_CLOCK_CONTROL, Value);

    //
    // Configure and enable the LCD clock.
    //

    AM335_CM_DPLL_WRITE(AM335_CM_DPLL_CLOCK_SELECT_LCD,
                        AM335_CM_DPLL_CLOCK_SELECT_LCD_PER_PLL_CLKOUT_M2);

    Value = AM335_CM_PER_READ(AM335_CM_PER_LCD_CLOCK_CONTROL);
    Value |= AM335_CM_PER_LCD_CLOCK_ENABLE;
    AM335_CM_PER_WRITE(AM335_CM_PER_LCD_CLOCK_CONTROL, Value);
    do {
        Value = AM335_CM_PER_READ(AM335_CM_PER_LCD_CLOCK_CONTROL);

    } while ((Value & AM335_CM_PER_LCD_CLOCK_MODE_MASK) !=
             AM335_CM_PER_LCD_CLOCK_ENABLE);

    //
    // Enable the mailbox clock for Cortex-M3 assisted sleep transitions.
    //

    Value = AM335_CM_PER_READ(AM335_CM_PER_MAILBOX_CLOCK_CONTROL);
    Value |= AM335_CM_PER_MAILBOX_CLOCK_ENABLE;
    AM335_CM_PER_WRITE(AM335_CM_PER_MAILBOX_CLOCK_CONTROL, Value);
    do {
        Value = AM335_CM_PER_READ(AM335_CM_PER_MAILBOX_CLOCK_CONTROL);

    } while ((Value & AM335_CM_PER_MAILBOX_CLOCK_MODE_MASK) !=
             AM335_CM_PER_MAILBOX_CLOCK_ENABLE);

    //
    // Enable the EDMA TPCC and TPTC clocks.
    //

    Value = AM335_CM_PER_READ(AM335_CM_PER_TPCC_CLOCK_CONTROL);
    Value |= AM335_CM_PER_TPCC_CLOCK_ENABLE;
    AM335_CM_PER_WRITE(AM335_CM_PER_TPCC_CLOCK_CONTROL, Value);
    Value = AM335_CM_PER_READ(AM335_CM_PER_TPTC0_CLOCK_CONTROL);
    Value |= AM335_CM_PER_TPTC0_CLOCK_ENABLE;
    AM335_CM_PER_WRITE(AM335_CM_PER_TPTC0_CLOCK_CONTROL, Value);
    Value = AM335_CM_PER_READ(AM335_CM_PER_TPTC1_CLOCK_CONTROL);
    Value |= AM335_CM_PER_TPTC1_CLOCK_ENABLE;
    AM335_CM_PER_WRITE(AM335_CM_PER_TPTC1_CLOCK_CONTROL, Value);
    Value = AM335_CM_PER_READ(AM335_CM_PER_TPTC2_CLOCK_CONTROL);
    Value |= AM335_CM_PER_TPTC2_CLOCK_ENABLE;
    AM335_CM_PER_WRITE(AM335_CM_PER_TPTC2_CLOCK_CONTROL, Value);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

