/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

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
// Avoid including kernel.h as this module may be isolated out into a dynamic
// library and will be restricted to a very limited API (as presented through
// the kernel sevices table).
//

#include <minoca/types.h>
#include <minoca/status.h>
#include <minoca/acpitabs.h>
#include <minoca/hmod.h>
#include <minoca/dev/am335x.h>

//
// --------------------------------------------------------------------- Macros
//

#define AM335_CM_DPLL_READ(_Register)                                       \
    HlAm335KernelServices->ReadRegister32(                                  \
                          HlAm335Prcm + AM335_CM_DPLL_OFFSET + (_Register))

#define AM335_CM_DPLL_WRITE(_Register, _Value)                              \
    HlAm335KernelServices->WriteRegister32(                                 \
                          HlAm335Prcm + AM335_CM_DPLL_OFFSET + (_Register), \
                          (_Value))

#define AM335_CM_PER_READ(_Register)                                        \
    HlAm335KernelServices->ReadRegister32(                                  \
                           HlAm335Prcm + AM335_CM_PER_OFFSET + (_Register))

#define AM335_CM_PER_WRITE(_Register, _Value)                               \
    HlAm335KernelServices->WriteRegister32(                                 \
                          HlAm335Prcm + AM335_CM_PER_OFFSET + (_Register),  \
                          (_Value))

#define AM335_CM_WAKEUP_READ(_Register)                                     \
    HlAm335KernelServices->ReadRegister32(                                  \
                         HlAm335Prcm + AM335_CM_WAKEUP_OFFSET + (_Register))

#define AM335_CM_WAKEUP_WRITE(_Register, _Value)                            \
    HlAm335KernelServices->WriteRegister32(                                 \
                        HlAm335Prcm + AM335_CM_WAKEUP_OFFSET + (_Register), \
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
        HlAm335Prcm = HlAm335KernelServices->MapPhysicalAddress(
                                      HlAm335Table->PrcmBase,
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

