/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    sp804tmr.c

Abstract:

    This module implements support for the ARM SP804 dual timer.

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
#include "realview.h"

//
// ---------------------------------------------------------------- Definitions
//

#define SP804_ALLOCATION_TAG 0x34385053 // '48PS'

//
// Control register bits.
//

#define SP804_CONTROL_ENABLED           0x80
#define SP804_CONTROL_MODE_FREE_RUNNING 0x00
#define SP804_CONTROL_MODE_PERIODIC     0x40
#define SP804_CONTROL_INTERRUPT_ENABLE  0x20
#define SP804_CONTROL_DIVIDE_BY_1       0x00
#define SP804_CONTROL_DIVIDE_BY_16      0x04
#define SP804_CONTROL_DIVIDE_BY_256     0x08
#define SP804_CONTROL_32_BIT            0x02
#define SP804_CONTROL_16_BIT            0x00
#define SP804_CONTROL_MODE_ONE_SHOT     0x01

//
// --------------------------------------------------------------------- Macros
//

//
// This macro reads from an SP804 timer. _Base should be a pointer, and
// _Register should be a SP804_TIMER_REGISTER value.
//

#define READ_TIMER_REGISTER(_Base, _Register) \
    HlReadRegister32((PULONG)(_Base) + (_Register))

//
// This macro writes to a SP804 timer. _Base should be a pointer,
// _Register should be SP804_TIMER_REGISTER value, and _Value should be a ULONG.
//

#define WRITE_TIMER_REGISTER(_Base, _Register, _Value) \
    HlWriteRegister32((PULONG)(_Base) + (_Register), (_Value))

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
HlpSp804TimerInitialize (
    PVOID Context
    );

ULONGLONG
HlpSp804TimerRead (
    PVOID Context
    );

KSTATUS
HlpSp804TimerArm (
    PVOID Context,
    TIMER_MODE Mode,
    ULONGLONG TickCount
    );

VOID
HlpSp804TimerDisarm (
    PVOID Context
    );

VOID
HlpSp804TimerAcknowledgeInterrupt (
    PVOID Context
    );

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define the registers for one timer, in ULONGs.
//

typedef enum _SP804_TIMER_REGISTER {
    Sp804LoadValue           = 0,
    Sp804CurrentValue        = 1,
    Sp804Control             = 2,
    Sp804InterruptClear      = 3,
    Sp804InterruptRawStatus  = 4,
    Sp804InterruptStatus     = 5,
    Sp804BackgroundLoadValue = 6,
    Sp804RegisterSize        = 0x20
} SP804_TIMER_REGISTER, *PSP804_TIMER_REGISTER;

/*++

Structure Description:

    This structure stores the internal state associated with a SP804 timer.

Members:

    PhysicalAddress - Stores the physical address of the timer base.

    BaseAddress - Stores the virtual address of the beginning of this timer
        block.

    Index - Stores the zero-based index of this timer within the timer block.

--*/

typedef struct _SP804_TIMER_DATA {
    PHYSICAL_ADDRESS PhysicalAddress;
    PVOID BaseAddress;
    ULONG Index;
} SP804_TIMER_DATA, *PSP804_TIMER_DATA;

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the RealView table.
//

PREALVIEW_TABLE HlRealViewTable;

//
// ------------------------------------------------------------------ Functions
//

VOID
HlpSp804TimerModuleEntry (
    VOID
    )

/*++

Routine Description:

    This routine is the entry point for the SP804 timer hardware module.
    Its role is to detect and report the prescense of an SP804 timer.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PULONGLONG Frequencies;
    PULONGLONG PhysicalAddresses;
    KSTATUS Status;
    TIMER_DESCRIPTION Timer;
    ULONG TimerCount;
    PSP804_TIMER_DATA TimerData;
    PULONG TimerGsi;
    ULONG TimerIndex;

    if (HlRealViewTable == NULL) {
        HlRealViewTable = HlGetAcpiTable(REALVIEW_SIGNATURE, NULL);
    }

    if (HlRealViewTable != NULL) {
        Frequencies = HlRealViewTable->TimerFrequency;
        PhysicalAddresses = HlRealViewTable->TimerPhysicalAddress;
        TimerCount = REALVIEW_TIMER_COUNT;
        TimerGsi = HlRealViewTable->TimerGsi;

    } else {
        goto Sp804TimerModuleEntryEnd;
    }

    //
    // Register each of the independent timers in the timer block.
    //

    for (TimerIndex = 0; TimerIndex < TimerCount; TimerIndex += 1) {
        RtlZeroMemory(&Timer, sizeof(TIMER_DESCRIPTION));
        Timer.TableVersion = TIMER_DESCRIPTION_VERSION;
        Timer.FunctionTable.Initialize = HlpSp804TimerInitialize;
        Timer.FunctionTable.ReadCounter = HlpSp804TimerRead;
        Timer.FunctionTable.WriteCounter = NULL;
        Timer.FunctionTable.Arm = HlpSp804TimerArm;
        Timer.FunctionTable.Disarm = HlpSp804TimerDisarm;
        Timer.FunctionTable.AcknowledgeInterrupt =
                                             HlpSp804TimerAcknowledgeInterrupt;

        //
        // Each timer block is actually two timers. Allocate a single structure
        // for both, and know that if the index is one, go backwards to get
        // to the main information.
        //

        TimerData = HlAllocateMemory(sizeof(SP804_TIMER_DATA) * 2,
                                     SP804_ALLOCATION_TAG,
                                     FALSE,
                                     NULL);

        if (TimerData == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto Sp804TimerModuleEntryEnd;
        }

        RtlZeroMemory(TimerData, sizeof(SP804_TIMER_DATA) * 2);
        TimerData->PhysicalAddress = PhysicalAddresses[TimerIndex];
        TimerData->Index = 0;
        TimerData[1].Index = 1;
        Timer.Context = TimerData;
        Timer.Features = TIMER_FEATURE_READABLE |
                         TIMER_FEATURE_PERIODIC |
                         TIMER_FEATURE_ONE_SHOT;

        Timer.CounterBitWidth = 32;
        Timer.CounterFrequency = Frequencies[TimerIndex];
        Timer.Interrupt.Line.Type = InterruptLineGsi;
        Timer.Interrupt.Line.U.Gsi = TimerGsi[TimerIndex];
        Timer.Interrupt.TriggerMode = InterruptModeUnknown;
        Timer.Interrupt.ActiveLevel = InterruptActiveLevelUnknown;

        //
        // Register the timer with the system.
        //

        Status = HlRegisterHardware(HardwareModuleTimer, &Timer);
        if (!KSUCCESS(Status)) {
            goto Sp804TimerModuleEntryEnd;
        }

        //
        // Register the second one. Report it as not having interrupt
        // capabilities as there's no way to disambiguate between the two
        // timers when the interrupt comes in.
        //

        Timer.Context = TimerData + 1;
        Timer.Features = TIMER_FEATURE_READABLE;
        Status = HlRegisterHardware(HardwareModuleTimer, &Timer);
        if (!KSUCCESS(Status)) {
            goto Sp804TimerModuleEntryEnd;
        }
    }

Sp804TimerModuleEntryEnd:
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
HlpSp804TimerInitialize (
    PVOID Context
    )

/*++

Routine Description:

    This routine initializes an SP804 timer.

Arguments:

    Context - Supplies the pointer to the timer's context, provided by the
        hardware module upon initialization.

Return Value:

    Status code.

--*/

{

    PVOID Base;
    ULONG ControlValue;
    BOOL SecondTimer;
    KSTATUS Status;
    PSP804_TIMER_DATA Timer;

    //
    // If this is the second timer, go back a structure to get the real one.
    //

    SecondTimer = FALSE;
    Timer = (PSP804_TIMER_DATA)Context;
    if (Timer->Index == 1) {
        Timer -= 1;
        SecondTimer = TRUE;
    }

    //
    // Map the hardware if that has not been done.
    //

    if (Timer->BaseAddress == NULL) {
        Timer->BaseAddress = HlMapPhysicalAddress(
                                         Timer->PhysicalAddress,
                                         2 * Sp804RegisterSize * sizeof(ULONG),
                                         TRUE);

        if (Timer->BaseAddress == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto Sp804TimerInitializeEnd;
        }
    }

    Base = Timer->BaseAddress;
    if (SecondTimer != FALSE) {
        Base += Sp804RegisterSize;
    }

    //
    // Program the timer in free running mode with no interrupt generation.
    //

    ControlValue = SP804_CONTROL_ENABLED |
                   SP804_CONTROL_DIVIDE_BY_1 |
                   SP804_CONTROL_32_BIT |
                   SP804_CONTROL_MODE_FREE_RUNNING;

    WRITE_TIMER_REGISTER(Base, Sp804Control, ControlValue);
    WRITE_TIMER_REGISTER(Base, Sp804InterruptClear, 1);
    Status = STATUS_SUCCESS;

Sp804TimerInitializeEnd:
    return Status;
}

ULONGLONG
HlpSp804TimerRead (
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

    PVOID Base;
    PSP804_TIMER_DATA Timer;
    ULONG Value;

    //
    // If this is the second timer, go back a structure to get the real one.
    //

    Timer = (PSP804_TIMER_DATA)Context;
    Base = Timer->BaseAddress;
    if (Timer->Index == 1) {
        Timer -= 1;
        Base = Timer->BaseAddress + Sp804RegisterSize;
    }

    Value = 0xFFFFFFFF - READ_TIMER_REGISTER(Base, Sp804CurrentValue);
    return Value;
}

KSTATUS
HlpSp804TimerArm (
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

    PVOID Base;
    ULONG ControlValue;
    PSP804_TIMER_DATA Timer;

    //
    // If this is the second timer, go back a structure to get the real one.
    //

    Timer = (PSP804_TIMER_DATA)Context;
    Base = Timer->BaseAddress;
    if (Timer->Index == 1) {
        Timer -= 1;
        Base = Timer->BaseAddress + Sp804RegisterSize;
    }

    if (TickCount >= MAX_ULONG) {
        TickCount = MAX_ULONG - 1;
    }

    //
    // Set up the control value to program.
    //

    ControlValue = SP804_CONTROL_ENABLED |
                   SP804_CONTROL_DIVIDE_BY_1 |
                   SP804_CONTROL_32_BIT |
                   SP804_CONTROL_INTERRUPT_ENABLE;

    if (Mode == TimerModePeriodic) {
        ControlValue |= SP804_CONTROL_MODE_PERIODIC;

    } else {
        ControlValue |= SP804_CONTROL_MODE_ONE_SHOT;
    }

    //
    // Set the timer to its maximum value, set the configuration, clear the
    // interrupt, then set the value.
    //

    WRITE_TIMER_REGISTER(Base, Sp804LoadValue, 0xFFFFFFFF);
    WRITE_TIMER_REGISTER(Base, Sp804Control, ControlValue);
    WRITE_TIMER_REGISTER(Base, Sp804InterruptClear, 1);
    WRITE_TIMER_REGISTER(Base, Sp804LoadValue, TickCount);
    return STATUS_SUCCESS;
}

VOID
HlpSp804TimerDisarm (
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
    PSP804_TIMER_DATA Timer;

    Timer = (PSP804_TIMER_DATA)Context;

    //
    // Disable the timer by programming it in free running mode with no
    // interrupt generation.
    //

    ControlValue = SP804_CONTROL_ENABLED |
                   SP804_CONTROL_DIVIDE_BY_1 |
                   SP804_CONTROL_32_BIT |
                   SP804_CONTROL_MODE_FREE_RUNNING;

    WRITE_TIMER_REGISTER(Timer->BaseAddress, Sp804Control, ControlValue);
    WRITE_TIMER_REGISTER(Timer->BaseAddress, Sp804InterruptClear, 1);
    return;
}

VOID
HlpSp804TimerAcknowledgeInterrupt (
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

    PVOID Base;
    PSP804_TIMER_DATA Timer;

    //
    // If this is the second timer, go back a structure to get the real one.
    //

    Timer = (PSP804_TIMER_DATA)Context;
    Base = Timer->BaseAddress;
    if (Timer->Index == 1) {
        Timer -= 1;
        Base = Timer->BaseAddress + Sp804RegisterSize;
    }

    WRITE_TIMER_REGISTER(Base, Sp804InterruptClear, 1);
    return;
}

