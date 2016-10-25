/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    omap3pwr.c

Abstract:

    This module implements power and clock domain services for the TI OMAP3.

Author:

    Evan Green 3-Sep-2012

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
#include "omap3.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro reads from an OMAP3 PRCM Register. _Base should be a pointer, and
// _Register should be a PRCM_REGISTER value.
//

#define READ_PRCM_REGISTER(_Base, _Register) \
    HlReadRegister32((PULONG)(_Base) + (_Register))

//
// This macro writes to an OMAP3 PRCM Register. _Base should be a pointer,
// _Register should be PRCM_REGISTER value, and _Value should be a ULONG.
//

#define WRITE_PRCM_REGISTER(_Base, _Register, _Value) \
    HlWriteRegister32((PULONG)(_Base) + (_Register), (_Value))

//
// ---------------------------------------------------------------- Definitions
//

//
// Register offsets for the Power and Clock Management unit. All offsets are in
// ULONGs.
//

typedef enum _PRCM_REGISTER {
    ClockPeripheralFunctionalClockEnable = 0x400, // CM_FCLKEN_PER
    ClockPeripheralInterfaceClockEnable  = 0x404, // CM_ICLKEN_PER
    ClockPeripheralIdleStatus            = 0x408, // CM_IDLEST_PER
    ClockPeripheralAutoIdleEnable        = 0x40C, // CM_AUTOIDLE_PER
    ClockPeripheralClockSelector         = 0x410, // CM_CLKSEL_PER
    ClockPeripheralSleepDependencyEnable = 0x411, // CM_SLEEPDEP_PER
    ClockPeripheralControl               = 0x412, // CM_CLKSTCTRL_PER
    ClockPeripheralStatus                = 0x413, // CM_CLKSTAT_PER
} PRCM_REGISTER, *PPRCM_REGISTER;

//
// Bit values for the ClockIntervalInterfaceClockEnable and
// ClockPeripheralFunctionalClockEnable registers.
//

#define GPTIMER2_CLOCK_ENABLE 0x00000008

//
// Bit values for the ClockPeripheralClockSelector register. If a bit is set,
// the system clock drives the unit. If the bit is clear, the 32kHz is the
// source.
//

#define SELECT_SYSTEM_CLOCK_GPTIMER2 0x00000001

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

PVOID HlOmap3Prcm = NULL;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
HlpOmap3InitializePowerAndClocks (
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

    //
    // Map the timer hardware if it is not already mapped.
    //

    if (HlOmap3Prcm == NULL) {
        HlOmap3Prcm = HlMapPhysicalAddress(HlOmap3Table->PrcmPhysicalAddress,
                                           OMAP3_PRCM_SIZE,
                                           TRUE);

        if (HlOmap3Prcm == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto InitializePowerAndClocksEnd;
        }
    }

    //
    // Enable the interface clock for GP Timer 2.
    //

    Value = READ_PRCM_REGISTER(HlOmap3Prcm,
                               ClockPeripheralInterfaceClockEnable);

    Value |= GPTIMER2_CLOCK_ENABLE;
    WRITE_PRCM_REGISTER(HlOmap3Prcm,
                        ClockPeripheralInterfaceClockEnable,
                        Value);

    //
    // Set GP Timer 2 to be run off the 32kHz and enable its functional
    // clock.
    //

    Value = READ_PRCM_REGISTER(HlOmap3Prcm, ClockPeripheralClockSelector);
    Value &= ~SELECT_SYSTEM_CLOCK_GPTIMER2;
    WRITE_PRCM_REGISTER(HlOmap3Prcm, ClockPeripheralClockSelector, Value);
    Value = READ_PRCM_REGISTER(HlOmap3Prcm,
                               ClockPeripheralFunctionalClockEnable);

    Value |= GPTIMER2_CLOCK_ENABLE;
    WRITE_PRCM_REGISTER(HlOmap3Prcm,
                        ClockPeripheralFunctionalClockEnable,
                        Value);

    Status = STATUS_SUCCESS;

InitializePowerAndClocksEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

