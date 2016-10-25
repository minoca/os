/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    cptimer.c

Abstract:

    This module implements timer support for the Integrator/CP board.

Author:

    Evan Green 22-Aug-2012

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
#include "integcp.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Control register bits.
//

#define CP_TIMER_ENABLED           0x00000080
#define CP_TIMER_MODE_FREE_RUNNING 0x00000000
#define CP_TIMER_MODE_PERIODIC     0x00000040
#define CP_TIMER_INTERRUPT_ENABLE  0x00000020
#define CP_TIMER_DIVIDE_BY_1       0x00000000
#define CP_TIMER_DIVIDE_BY_16      0x00000004
#define CP_TIMER_DIVIDE_BY_256     0x00000008
#define CP_TIMER_32_BIT            0x00000002
#define CP_TIMER_16_BIT            0x00000000
#define CP_TIMER_MODE_ONE_SHOT     0x00000001

//
// --------------------------------------------------------------------- Macros
//

//
// This macro reads from a Integrator/CP timer. _Base should be a pointer, and
// _Register should be a CP_TIMER_REGISTER value.
//

#define READ_TIMER_REGISTER(_Base, _Register) \
    HlReadRegister32((PULONG)(_Base) + (_Register))

//
// This macro writes to a Integrator/CP timer. _Base should be a pointer,
// _Register should be CP_TIMER_REGISTER value, and _Value should be a ULONG.
//

#define WRITE_TIMER_REGISTER(_Base, _Register, _Value) \
    HlWriteRegister32((PULONG)(_Base) + (_Register), (_Value))

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
HlpCpTimerInitialize (
    PVOID Context
    );

ULONGLONG
HlpCpTimerRead (
    PVOID Context
    );

KSTATUS
HlpCpTimerArm (
    PVOID Context,
    TIMER_MODE Mode,
    ULONGLONG TickCount
    );

VOID
HlpCpTimerDisarm (
    PVOID Context
    );

VOID
HlpCpTimerAcknowledgeInterrupt (
    PVOID Context
    );

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define the registers for one timer, in ULONGs.
//

typedef enum _CP_TIMER_REGISTER {
    CpTimerLoadValue           = 0,
    CpTimerCurrentValue        = 1,
    CpTimerControl             = 2,
    CpTimerInterruptClear      = 3,
    CpTimerInterruptRawStatus  = 4,
    CpTimerInterruptStatus     = 5,
    CpTimerBackgroundLoadValue = 6,
    CpTimerRegisterSize        = 0x40
} CP_TIMER_REGISTER, *PCP_TIMER_REGISTER;

/*++

Structure Description:

    This structure stores the internal state associated with a Integrator/CP
    timer.

Members:

    BaseAddress - Stores the virtual address of the beginning of this timer
        block.

    Index - Stores the zero-based index of this timer within the timer block.

--*/

typedef struct _CP_TIMER_DATA {
    PVOID BaseAddress;
    ULONG Index;
} CP_TIMER_DATA, *PCP_TIMER_DATA;

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the timer block's virtual address.
//

PVOID HlCpTimer = NULL;

//
// ------------------------------------------------------------------ Functions
//

VOID
HlpCpTimerModuleEntry (
    VOID
    )

/*++

Routine Description:

    This routine is the entry point for the Integrator/CP timer hardware module.
    Its role is to detect and report the prescense of an Integrator/CP timer.

Arguments:

    None.

Return Value:

    None.

--*/

{

    TIMER_DESCRIPTION CpTimer;
    KSTATUS Status;
    PCP_TIMER_DATA TimerData;
    ULONG TimerIndex;

    //
    // Interrupt controllers are always initialized before timers, so the
    // integrator table should already be set up.
    //

    if ((HlCpIntegratorTable == NULL) ||
        (HlCpIntegratorTable->TimerBlockPhysicalAddress == 0)) {

        goto CpTimerModuleEntryEnd;
    }

    //
    // Register each of the independent timers in the timer block.
    //

    for (TimerIndex = 0;
         TimerIndex < INTEGRATORCP_TIMER_COUNT;
         TimerIndex += 1) {

        RtlZeroMemory(&CpTimer, sizeof(TIMER_DESCRIPTION));
        CpTimer.TableVersion = TIMER_DESCRIPTION_VERSION;
        CpTimer.FunctionTable.Initialize = HlpCpTimerInitialize;
        CpTimer.FunctionTable.ReadCounter = HlpCpTimerRead;
        CpTimer.FunctionTable.WriteCounter = NULL;
        CpTimer.FunctionTable.Arm = HlpCpTimerArm;
        CpTimer.FunctionTable.Disarm = HlpCpTimerDisarm;
        CpTimer.FunctionTable.AcknowledgeInterrupt =
                                                HlpCpTimerAcknowledgeInterrupt;

        TimerData = HlAllocateMemory(sizeof(CP_TIMER_DATA),
                                     INTEGRATOR_ALLOCATION_TAG,
                                     FALSE,
                                     NULL);

        if (TimerData == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto CpTimerModuleEntryEnd;
        }

        RtlZeroMemory(TimerData, sizeof(CP_TIMER_DATA));
        TimerData->Index = TimerIndex;
        CpTimer.Context = TimerData;
        CpTimer.Features = TIMER_FEATURE_READABLE |
                           TIMER_FEATURE_PERIODIC |
                           TIMER_FEATURE_ONE_SHOT;

        CpTimer.CounterBitWidth = 32;

        //
        // The first timer runs at the bus clock speed, but the second two
        // run at a fixed frequency.
        //

        if (TimerIndex == 0) {
            CpTimer.CounterFrequency = 0;

        } else {
            CpTimer.CounterFrequency = INTEGRATORCP_TIMER_FIXED_FREQUENCY;
        }

        CpTimer.Interrupt.Line.Type = InterruptLineControllerSpecified;
        CpTimer.Interrupt.Line.U.Local.Controller = 0;
        CpTimer.Interrupt.Line.U.Local.Line =
                                     HlCpIntegratorTable->TimerGsi[TimerIndex];

        CpTimer.Interrupt.TriggerMode = InterruptModeUnknown;
        CpTimer.Interrupt.ActiveLevel = InterruptActiveLevelUnknown;

        //
        // Register the timer with the system.
        //

        Status = HlRegisterHardware(HardwareModuleTimer, &CpTimer);
        if (!KSUCCESS(Status)) {
            goto CpTimerModuleEntryEnd;
        }
    }

CpTimerModuleEntryEnd:
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
HlpCpTimerInitialize (
    PVOID Context
    )

/*++

Routine Description:

    This routine initializes an Integrator/CP timer.

Arguments:

    Context - Supplies the pointer to the timer's context, provided by the
        hardware module upon initialization.

Return Value:

    Status code.

--*/

{

    ULONG ControlValue;
    KSTATUS Status;
    PCP_TIMER_DATA Timer;

    Timer = (PCP_TIMER_DATA)Context;

    //
    // Map the hardware if that has not been done.
    //

    if (Timer->BaseAddress == NULL) {
        if (HlCpTimer == NULL) {
            HlCpTimer = HlMapPhysicalAddress(
                                HlCpIntegratorTable->TimerBlockPhysicalAddress,
                                CpTimerRegisterSize * sizeof(ULONG),
                                TRUE);

            if (HlCpTimer == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto CpTimerInitializeEnd;
            }
        }

        Timer->BaseAddress = (PVOID)((PULONG)HlCpTimer +
                                     (Timer->Index * CpTimerRegisterSize));
    }

    //
    // Program the timer in free running mode with no interrupt generation.
    //

    ControlValue = CP_TIMER_ENABLED |
                   CP_TIMER_DIVIDE_BY_1 |
                   CP_TIMER_32_BIT |
                   CP_TIMER_MODE_FREE_RUNNING;

    WRITE_TIMER_REGISTER(Timer->BaseAddress, CpTimerControl, ControlValue);
    WRITE_TIMER_REGISTER(Timer->BaseAddress, CpTimerInterruptClear, 1);
    Status = STATUS_SUCCESS;

CpTimerInitializeEnd:
    return Status;
}

ULONGLONG
HlpCpTimerRead (
    PVOID Context
    )

/*++

Routine Description:

    This routine returns the hardware counter's raw value.

Arguments:

    Context - Supplies the pointer to the timer's context.

Return Value:

    Returns the timer's current count.

--*/

{

    PCP_TIMER_DATA Timer;
    ULONG Value;

    Timer = (PCP_TIMER_DATA)Context;
    Value = 0xFFFFFFFF -
            READ_TIMER_REGISTER(Timer->BaseAddress, CpTimerCurrentValue);

    return Value;
}

KSTATUS
HlpCpTimerArm (
    PVOID Context,
    TIMER_MODE Mode,
    ULONGLONG TickCount
    )

/*++

Routine Description:

    This routine arms the timer to fire an interrupt after the specified number
    of ticks.

Arguments:

    Context - Supplies the pointer to the timer's context, provided by the
        hardware module upon initialization.

    Mode - Supplies the mode to arm the timer in. The system will never request
        a mode not supported by the timer's feature bits. The mode dictates
        how the tick count argument is interpreted.

    TickCount - Supplies the number of timer ticks from now for the timer to
        fire in. In absolute mode, this supplies the time in timer ticks at
        which to fire an interrupt.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    ULONG ControlValue;
    PCP_TIMER_DATA Timer;

    Timer = (PCP_TIMER_DATA)Context;
    if (TickCount >= MAX_ULONG) {
        TickCount = MAX_ULONG - 1;
    }

    //
    // Set up the control value to program.
    //

    ControlValue = CP_TIMER_ENABLED |
                   CP_TIMER_DIVIDE_BY_1 |
                   CP_TIMER_32_BIT |
                   CP_TIMER_INTERRUPT_ENABLE;

    if (Mode == TimerModePeriodic) {
        ControlValue |= CP_TIMER_MODE_PERIODIC;

    } else {
        ControlValue |= CP_TIMER_MODE_ONE_SHOT;
    }

    //
    // Set the timer to its maximum value, set the configuration, clear the
    // interrupt, then set the value.
    //

    WRITE_TIMER_REGISTER(Timer->BaseAddress, CpTimerLoadValue, 0xFFFFFFFF);
    WRITE_TIMER_REGISTER(Timer->BaseAddress, CpTimerControl, ControlValue);
    WRITE_TIMER_REGISTER(Timer->BaseAddress, CpTimerInterruptClear, 1);
    WRITE_TIMER_REGISTER(Timer->BaseAddress, CpTimerLoadValue, TickCount);
    return STATUS_SUCCESS;
}

VOID
HlpCpTimerDisarm (
    PVOID Context
    )

/*++

Routine Description:

    This routine disarms the timer, stopping interrupts from firing.

Arguments:

    Context - Supplies the pointer to the timer's context, provided by the
        hardware module upon initialization.

Return Value:

    None.

--*/

{

    ULONG ControlValue;
    PCP_TIMER_DATA Timer;

    Timer = (PCP_TIMER_DATA)Context;

    //
    // Disable the timer by programming it in free running mode with no
    // interrupt generation.
    //

    ControlValue = CP_TIMER_ENABLED |
                   CP_TIMER_DIVIDE_BY_1 |
                   CP_TIMER_32_BIT |
                   CP_TIMER_MODE_FREE_RUNNING;

    WRITE_TIMER_REGISTER(Timer->BaseAddress, CpTimerControl, ControlValue);
    WRITE_TIMER_REGISTER(Timer->BaseAddress, CpTimerInterruptClear, 1);
    return;
}

VOID
HlpCpTimerAcknowledgeInterrupt (
    PVOID Context
    )

/*++

Routine Description:

    This routine performs any actions necessary upon reciept of a timer's
    interrupt. This may involve writing to an acknowledge register to re-enable
    the timer to fire again, or other hardware specific actions.

Arguments:

    Context - Supplies the pointer to the timer's context, provided by the
        hardware module upon initialization.

Return Value:

    None.

--*/

{

    PCP_TIMER_DATA Timer;

    Timer = (PCP_TIMER_DATA)Context;
    WRITE_TIMER_REGISTER(Timer->BaseAddress, CpTimerInterruptClear, 1);
    return;
}

