/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    omap4pwr.c

Abstract:

    This module implements power and clock domain services for the TI OMAP4.

Author:

    Evan Green 4-Nov-2012

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
#include "omap4.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro reads from an OMAP4 PRCM Register. _Base should be a pointer, and
// _Register should be a register offset in ULONGs.
//

#define READ_PRCM_REGISTER(_Base, _Register) \
    HlReadRegister32((PULONG)(_Base) + (_Register))

//
// This macro writes to an OMAP4 PRCM Register. _Base should be a pointer,
// _Register should be register offset in ULONGs, and _Value should be a ULONG.
//

#define WRITE_PRCM_REGISTER(_Base, _Register, _Value) \
    HlWriteRegister32((PULONG)(_Base) + (_Register), (_Value))

//
// ---------------------------------------------------------------- Definitions
//

//
// This bit is set to select the always on 32kHz clock source to drive the
// timer counter.
//

#define GPTIMER_SELECT_32KHZ_CLOCK 0x01000000
#define GPTIMER_SELECT_SYSTEM_CLOCK 0x00000000

//
// These bits define the operating mode of the functional clock.
//

#define GPTIMER_CLOCK_MODE_MASK 0x03
#define GPTIMER_ENABLE_CLOCK 0x02

//
// Define the clock control bits for the Audio backend control.
//

#define AUDIO_CLOCK_CONTROL_MODE_MASK 0x3
#define AUDIO_CLOCK_CONTROL_NO_SLEEP 0x0

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Register offsets for the Wakeup Clock Management interface (WKUP_CM). All
// offsets are in ULONGs.
//

typedef enum _WKUP_CM_REGISTER {
    WakeupClockControl         = 0x00, // CM_WKUP_CLKSTCTRL
    WakeupClockGpTimer1Control = 0x10, // CM_WKUP_GPTIMER1_CLKCTRL
} WKUP_CM_REGISTER, *PWKUP_CM_REGISTER;

//
// Register offsets for the L4 Interconnect Clock Managment interface
// (L4PER_CM). All offsets are in ULONGs.
//

typedef enum _L4PER_CM_REGISTER {
    L4ClockControl          = 0x00, // CM_L4PER_CLKSTCTRL
    L4ClockGpTimer10Control = 0x0A, // CM_L4PER_GPTIMER10_CLKCTRL
    L4ClockGpTimer11Control = 0x0C, // CM_L4PER_GPTIMER11_CLKCTRL
    L4ClockGpTimer2Control  = 0x0E, // CM_L4PER_GPTIMER2_CLKCTRL
    L4ClockGpTimer3Control  = 0x10, // CM_L4PER_GPTIMER3_CLKCTRL
    L4ClockGpTimer4Control  = 0x12, // CM_L4PER_GPTIMER4_CLKCTRL
    L4ClockGpTimer9Control  = 0x14, // CM_L4PER_GPTIMER9_CLKCTRL
} L4PER_CM_REGISTER, *PL4PER_CM_REGISTER;

//
// Register offsets for the Audio Back-End Clock Management interface (ABE_CM1).
// All offsets are in ULONGs.
//

typedef enum _ABE_CM1_REGISTER {
    AudioClockControl         = 0x00, // CM1_ABE_CLKSTCTRL
    AudioClockGpTimer5Control = 0x1A, // CM1_ABE_GPTIMER5_CLKCTRL
    AudioClockGpTimer6Control = 0x1C, // CM1_ABE_GPTIMER6_CLKCTRL
    AudioClockGpTimer7Control = 0x1E, // CM1_ABE_GPTIMER7_CLKCTRL
    AudioClockGpTimer8Control = 0x20, // CM1_ABE_GPTIMER8_CLKCTRL
} ABE_CM1_REGISTER, *PABE_CM1_REGISTER;

//
// -------------------------------------------------------------------- Globals
//

//
// Store pointers to pieces of the PRCM.
//

PVOID HlOmap4WakeupClockControl = NULL;
PVOID HlOmap4L4ClockControl = NULL;
PVOID HlOmap4AudioClockControl = NULL;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
HlpOmap4InitializePowerAndClocks (
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

    //
    // Map each of the PRCM sections if needed.
    //

    if (HlOmap4WakeupClockControl == NULL) {
        HlOmap4WakeupClockControl = HlMapPhysicalAddress(
                                      HlOmap4Table->WakeupClockPhysicalAddress,
                                      0x800,
                                      TRUE);

        if (HlOmap4WakeupClockControl == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto InitializePowerAndClocksEnd;
        }
    }

    if (HlOmap4L4ClockControl == NULL) {
        HlOmap4L4ClockControl = HlMapPhysicalAddress(
                                          HlOmap4Table->L4ClockPhysicalAddress,
                                          0xC00,
                                          TRUE);

        if (HlOmap4L4ClockControl == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto InitializePowerAndClocksEnd;
        }
    }

    if (HlOmap4AudioClockControl == NULL) {
        HlOmap4AudioClockControl = HlMapPhysicalAddress(
                                       HlOmap4Table->AudioClockPhysicalAddress,
                                       0xB00,
                                       TRUE);

        if (HlOmap4AudioClockControl == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto InitializePowerAndClocksEnd;
        }
    }

    //
    // Enable GP Timer 1, and set it to run at the system clock frequency.
    //

    Value = GPTIMER_SELECT_SYSTEM_CLOCK | GPTIMER_ENABLE_CLOCK;
    WRITE_PRCM_REGISTER(HlOmap4WakeupClockControl,
                        WakeupClockGpTimer1Control,
                        Value);

    //
    // Enable GP Timers 2-4 and 9-11 to run at the 32kHz clock speed.
    //

    Value = GPTIMER_SELECT_32KHZ_CLOCK | GPTIMER_ENABLE_CLOCK;
    WRITE_PRCM_REGISTER(HlOmap4L4ClockControl,
                        L4ClockGpTimer2Control,
                        Value);

    WRITE_PRCM_REGISTER(HlOmap4L4ClockControl,
                        L4ClockGpTimer3Control,
                        Value);

    WRITE_PRCM_REGISTER(HlOmap4L4ClockControl,
                        L4ClockGpTimer4Control,
                        Value);

    WRITE_PRCM_REGISTER(HlOmap4L4ClockControl,
                        L4ClockGpTimer9Control,
                        Value);

    WRITE_PRCM_REGISTER(HlOmap4L4ClockControl,
                        L4ClockGpTimer10Control,
                        Value);

    WRITE_PRCM_REGISTER(HlOmap4L4ClockControl,
                        L4ClockGpTimer11Control,
                        Value);

    //
    // Enable the Audio Back-End clock.
    //

    Value = READ_PRCM_REGISTER(HlOmap4AudioClockControl, AudioClockControl);
    Value &= ~AUDIO_CLOCK_CONTROL_MODE_MASK;
    Value |= AUDIO_CLOCK_CONTROL_NO_SLEEP;
    WRITE_PRCM_REGISTER(HlOmap4AudioClockControl, AudioClockControl, Value);

    //
    // Enable GP Timers 5-8 to run at the 32kHz always on clock rate.
    //

    Value = GPTIMER_SELECT_32KHZ_CLOCK | GPTIMER_ENABLE_CLOCK;
    WRITE_PRCM_REGISTER(HlOmap4AudioClockControl,
                        AudioClockGpTimer5Control,
                        Value);

    WRITE_PRCM_REGISTER(HlOmap4AudioClockControl,
                        AudioClockGpTimer6Control,
                        Value);

    WRITE_PRCM_REGISTER(HlOmap4AudioClockControl,
                        AudioClockGpTimer7Control,
                        Value);

    WRITE_PRCM_REGISTER(HlOmap4AudioClockControl,
                        AudioClockGpTimer8Control,
                        Value);

    Status = STATUS_SUCCESS;

InitializePowerAndClocksEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

