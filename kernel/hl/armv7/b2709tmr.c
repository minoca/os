/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    b2709tmr.c

Abstract:

    This module implements support for the BCM2709's timers.

Author:

    Chris Stevens 24-Mar-2014

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
#include "bcm2709.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ARM Timer Control register bits.
//
// The BCM2709's version of the SP804 does not support one-shot mode and is
// always periodic based on the load value, making those bits defunct. It also
// introduces extra control bits for controlling its extra free-running counter.
//

#define BCM2709_ARM_TIMER_CONTROL_FREE_RUNNING_DIVIDE_MASK  0x00FF0000
#define BCM2709_ARM_TIMER_CONTROL_FREE_RUNNING_DIVIDE_SHIFT 16
#define BCM2709_ARM_TIMER_CONTROL_FREE_RUNNING_ENABLED      0x00000200
#define BCM2709_ARM_TIMER_CONTROL_HALT_ON_DEBUG             0x00000100
#define BCM2709_ARM_TIMER_CONTROL_ENABLED                   0x00000080
#define BCM2709_ARM_TIMER_CONTROL_INTERRUPT_ENABLE          0x00000020
#define BCM2709_ARM_TIMER_CONTROL_DIVIDE_BY_1               0x00000000
#define BCM2709_ARM_TIMER_CONTROL_DIVIDE_BY_16              0x00000004
#define BCM2709_ARM_TIMER_CONTROL_DIVIDE_BY_256             0x00000008
#define BCM2709_ARM_TIMER_CONTROL_32_BIT                    0x00000002
#define BCM2709_ARM_TIMER_CONTROL_16_BIT                    0x00000000

//
// Define the target default frequency to use for the BCM2709 timer, if
// possible.
//

#define BCM2709_ARM_TIMER_TARGET_FREQUENCY 1000000

//
// Define the maximum predivider.
//

#define BCM2709_ARM_TIMER_PREDIVIDER_MAX 0x1FF

//
// Define the BCM2709 System Timer's control register values.
//

#define BCM2709_SYSTEM_TIMER_CONTROL_MATCH_3 0x00000008
#define BCM2709_SYSTEM_TIMER_CONTROL_MATCH_2 0x00000004
#define BCM2709_SYSTEM_TIMER_CONTROL_MATCH_1 0x00000002
#define BCM2709_SYSTEM_TIMER_CONTROL_MATCH_0 0x00000001

//
// --------------------------------------------------------------------- Macros
//

//
// This macro reads from a BCM2709 ARM Timer register.
//

#define READ_ARM_TIMER_REGISTER(_Register) \
    HlReadRegister32(HlBcm2709ArmTimerBase + (_Register))

//
// This macro writes to a BCM2709 ARM Timer register.
//

#define WRITE_ARM_TIMER_REGISTER(_Register, _Value) \
    HlWriteRegister32(HlBcm2709ArmTimerBase + (_Register), (_Value))

//
// This macro reads from a BCM2709 System Timer register.
//

#define READ_SYSTEM_TIMER_REGISTER(_Register) \
    HlReadRegister32(HlBcm2709SystemTimerBase + (_Register))

//
// This macro writes to a BCM2709 System Timer register.
//

#define WRITE_SYSTEM_TIMER_REGISTER(_Register, _Value) \
    HlWriteRegister32(HlBcm2709SystemTimerBase + (_Register), (_Value))

//
// This macro compares two counter values accounting for roll over.
//

#define BCM2709_COUNTER_LESS_THAN(_Counter1, _Counter2) \
            (((LONG)((_Counter1) - (_Counter2))) < 0)

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
HlpBcm2709TimerInitialize (
    PVOID Context
    );

ULONGLONG
HlpBcm2709TimerRead (
    PVOID Context
    );

KSTATUS
HlpBcm2709TimerArm (
    PVOID Context,
    TIMER_MODE Mode,
    ULONGLONG TickCount
    );

VOID
HlpBcm2709TimerDisarm (
    PVOID Context
    );

VOID
HlpBcm2709TimerAcknowledgeInterrupt (
    PVOID Context
    );

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _BCM2709_TIMER_TYPE {
    Bcm2709TimerArmPeriodic,
    Bcm2709TimerArmCounter,
    Bcm2709TimerSystemPeriodic0,
    Bcm2709TimerSystemPeriodic1,
    Bcm2709TimerSystemPeriodic2,
    Bcm2709TimerSystemPeriodic3,
    Bcm2709TimerSystemCounter
} BCM2709_TIMER_TYPE, *PBCM2709_TIMER_TYPE;

//
// Define the registers for the ARM timer, in bytes.
//

typedef enum _BCM2709_ARM_TIMER_REGISTER {
    Bcm2709ArmTimerLoadValue           = 0x00,
    Bcm2709ArmTimerCurrentValue        = 0x04,
    Bcm2709ArmTimerControl             = 0x08,
    Bcm2709ArmTimerInterruptClear      = 0x0C,
    Bcm2709ArmTimerInterruptRawStatus  = 0x10,
    Bcm2709ArmTimerInterruptStatus     = 0x14,
    Bcm2709ArmTimerBackgroundLoadValue = 0x18,
    Bcm2709ArmTimerPredivider          = 0x1C,
    Bcm2709ArmTimerFreeRunningCounter  = 0x20,
    Bcm2709ArmTimerRegisterSize        = 0x24
} BCM2709_ARM_TIMER_REGISTER, *PBCM2709_ARM_TIMER_REGISTER;

//
// Define the registers for the System timer, in bytes.
//

typedef enum _BCM2709_SYSTEM_TIMER_REGISTER {
    Bcm2709SystemTimerControl      = 0x00,
    Bcm2709SystemTimerCounterLow   = 0x04,
    Bcm2709SystemTimerCounterHigh  = 0x08,
    Bcm2709SystemTimerCompare0     = 0x0C,
    Bcm2709SystemTimerCompare1     = 0x10,
    Bcm2709SystemTimerCompare2     = 0x14,
    Bcm2709SystemTimerCompare3     = 0x18,
    Bcm2709SystemTimerRegisterSize = 0x1C
} BCM2709_SYSTEM_TIMER_REGISTER, *PBCM2709_SYSTEM_TIMER_REGISTER;

/*++

Structure Description:

    This structure defines a default BCM2709 timer.

Members:

    Type - Stores the type of BCM2709 timer.

    Predivider - Stores the optional predivider used to program the frequency.

--*/

typedef struct _BCM2709_TIMER {
    BCM2709_TIMER_TYPE Type;
    ULONG Predivider;
} BCM2709_TIMER, *PBCM2709_TIMER;

/*++

Structure Description:

    This structure defines a BCM2709 periodic system timer.

Members:

    Type - Stores the type of BCM2709 timer.

    Mode - Stores the current mode.

    TickCount - Stores the current tick count. This is either a periodic
        interval or the relative one-shot tick count.

    Generation - Stores the generation counter used to synchronize attempts to
        arm and disarm the system timers with acknowledge interrupt rearming
        the timer for periodic mode.

--*/

typedef struct _BCM2709_SYSTEM_TIMER {
    BCM2709_TIMER_TYPE Type;
    TIMER_MODE Mode;
    ULONG TickCount;
    volatile ULONG Generation;
} BCM2709_SYSTEM_TIMER, *PBCM2709_SYSTEM_TIMER;

//
// -------------------------------------------------------------------- Globals
//

//
// Store the virtual address of the mapped timer bases.
//

PVOID HlBcm2709ArmTimerBase = NULL;
PVOID HlBcm2709SystemTimerBase = NULL;

//
// ------------------------------------------------------------------ Functions
//

VOID
HlpBcm2709TimerModuleEntry (
    VOID
    )

/*++

Routine Description:

    This routine is the entry point for the BCM2709 timer hardware module.
    Its role is to detect and report the prescense of the BCM2709 timer.

Arguments:

    Services - Supplies a pointer to the services/APIs made available by the
        kernel to the hardware module.

Return Value:

    None.

--*/

{

    PBCM2709_TIMER DefaultTimer;
    ULONGLONG Frequency;
    ULONGLONG MaxFrequency;
    ULONG Predivider;
    KSTATUS Status;
    PBCM2709_SYSTEM_TIMER SystemTimer;
    TIMER_DESCRIPTION Timer;

    //
    // Interrupt controllers are always initialized before timers, so the table
    // should already be set up.
    //

    if (HlBcm2709Table == NULL) {
        goto Bcm2709TimerModuleEntryEnd;
    }

    //
    // Initialize the ARM timers first. Determine the frequency based on the
    // APB clock frequency. The formula is "ARM Timer Frequency" =
    // ("APB Clock Frequency") / (Predivider + 1).
    //

    MaxFrequency = BCM2709_ARM_TIMER_TARGET_FREQUENCY *
                   (BCM2709_ARM_TIMER_PREDIVIDER_MAX + 1);

    //
    // If the APB clock frequency is less than the target, just use that
    // frequency.
    //

    if (HlBcm2709Table->ApbClockFrequency <=
        BCM2709_ARM_TIMER_TARGET_FREQUENCY) {

        Frequency = HlBcm2709Table->ApbClockFrequency;
        Predivider = 0;

    //
    // If the APB clock frequency is less than or equal to the maximum
    // frequency that still allows the target frequency to be achieved, then
    // use the target frequency and associated predivider.
    //

    } else if (HlBcm2709Table->ApbClockFrequency <= MaxFrequency) {
        Frequency = BCM2709_ARM_TIMER_TARGET_FREQUENCY;
        Predivider = (HlBcm2709Table->ApbClockFrequency / Frequency) - 1;

    //
    // Otherwise get as close to the target frequency as possible by using the
    // maximum predivider.
    //

    } else {
        Predivider = BCM2709_ARM_TIMER_PREDIVIDER_MAX;
        Frequency = HlBcm2709Table->ApbClockFrequency / (Predivider + 1);
    }

    //
    // Register each of the independent timers in the timer block.
    //

    RtlZeroMemory(&Timer, sizeof(TIMER_DESCRIPTION));
    Timer.TableVersion = TIMER_DESCRIPTION_VERSION;
    Timer.FunctionTable.Initialize = HlpBcm2709TimerInitialize;
    Timer.FunctionTable.ReadCounter = HlpBcm2709TimerRead;
    Timer.FunctionTable.WriteCounter = NULL;
    Timer.FunctionTable.Arm = HlpBcm2709TimerArm;
    Timer.FunctionTable.Disarm = HlpBcm2709TimerDisarm;
    Timer.FunctionTable.AcknowledgeInterrupt =
                                           HlpBcm2709TimerAcknowledgeInterrupt;

    //
    // Register the BCM2709 ARM Timer based on the SP804. It is periodic and
    // readable, but can change dynamically in reduced power states. It also
    // supports "one-shot" mode in that the maximum next deadline can be
    // auto-programmed after a one-shot timer fires.
    //

    DefaultTimer = HlAllocateMemory(sizeof(BCM2709_TIMER),
                                    BCM2709_ALLOCATION_TAG,
                                    FALSE,
                                    NULL);

    if (DefaultTimer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto Bcm2709TimerModuleEntryEnd;
    }

    DefaultTimer->Type = Bcm2709TimerArmPeriodic;
    DefaultTimer->Predivider = Predivider;
    Timer.Context = DefaultTimer;
    Timer.Features = TIMER_FEATURE_READABLE |
                     TIMER_FEATURE_PERIODIC |
                     TIMER_FEATURE_ONE_SHOT |
                     TIMER_FEATURE_P_STATE_VARIANT;

    Timer.CounterBitWidth = 32;
    Timer.CounterFrequency = Frequency;
    Timer.Interrupt.Line.Type = InterruptLineGsi;
    Timer.Interrupt.Line.U.Gsi = HlBcm2709Table->ArmTimerGsi;
    Timer.Interrupt.TriggerMode = InterruptModeUnknown;
    Timer.Interrupt.ActiveLevel = InterruptActiveLevelUnknown;
    Status = HlRegisterHardware(HardwareModuleTimer, &Timer);
    if (!KSUCCESS(Status)) {
        goto Bcm2709TimerModuleEntryEnd;
    }

    //
    // Register the BCM2709 ARM free running counter. It is readable but its
    // speed can change dynamically in reduced power status.
    //

    DefaultTimer = HlAllocateMemory(sizeof(BCM2709_TIMER),
                                    BCM2709_ALLOCATION_TAG,
                                    FALSE,
                                    NULL);

    if (DefaultTimer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto Bcm2709TimerModuleEntryEnd;
    }

    DefaultTimer->Type = Bcm2709TimerArmCounter;
    DefaultTimer->Predivider = Predivider;
    Timer.Context = DefaultTimer;
    Timer.CounterBitWidth = 32;
    Timer.Features = TIMER_FEATURE_READABLE | TIMER_FEATURE_P_STATE_VARIANT;
    Timer.CounterFrequency = Frequency;
    Timer.Interrupt.Line.Type = InterruptLineInvalid;
    Status = HlRegisterHardware(HardwareModuleTimer, &Timer);
    if (!KSUCCESS(Status)) {
        goto Bcm2709TimerModuleEntryEnd;
    }

    //
    // Initialize the BCM2709 System timer's free running counter. The counter
    // is writable, but since the Video Core maybe using it, altering the
    // counter is dangerous.
    //

    DefaultTimer = HlAllocateMemory(sizeof(BCM2709_TIMER),
                                    BCM2709_ALLOCATION_TAG,
                                    FALSE,
                                    NULL);

    if (DefaultTimer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto Bcm2709TimerModuleEntryEnd;
    }

    RtlZeroMemory(DefaultTimer, sizeof(BCM2709_TIMER));
    DefaultTimer->Type = Bcm2709TimerSystemCounter;
    Timer.Context = DefaultTimer;
    Timer.CounterBitWidth = 64;
    Timer.Features = TIMER_FEATURE_READABLE;
    Timer.CounterFrequency = HlBcm2709Table->SystemTimerFrequency;
    Timer.Interrupt.Line.Type = InterruptLineInvalid;
    Status = HlRegisterHardware(HardwareModuleTimer, &Timer);
    if (!KSUCCESS(Status)) {
        goto Bcm2709TimerModuleEntryEnd;
    }

    //
    // Initialize the two "periodic" BCM2709 System Timers that are not in use
    // by the GPU. Those are timers 1 and 3. They are not truly periodic in
    // that they do not automatically reload, but they make do for profiler
    // timers.
    //

    SystemTimer = HlAllocateMemory(sizeof(BCM2709_SYSTEM_TIMER),
                                   BCM2709_ALLOCATION_TAG,
                                   FALSE,
                                   NULL);

    if (SystemTimer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto Bcm2709TimerModuleEntryEnd;
    }

    RtlZeroMemory(SystemTimer, sizeof(BCM2709_SYSTEM_TIMER));
    SystemTimer->Type = Bcm2709TimerSystemPeriodic1;
    Timer.Context = SystemTimer;
    Timer.CounterBitWidth = 32;
    Timer.Features = TIMER_FEATURE_READABLE |
                     TIMER_FEATURE_PERIODIC |
                     TIMER_FEATURE_ONE_SHOT;

    Timer.CounterFrequency = HlBcm2709Table->SystemTimerFrequency;
    Timer.Interrupt.Line.Type = InterruptLineGsi;
    Timer.Interrupt.Line.U.Gsi = HlBcm2709Table->SystemTimerGsiBase + 1;
    Timer.Interrupt.TriggerMode = InterruptModeUnknown;
    Timer.Interrupt.ActiveLevel = InterruptActiveLevelUnknown;
    Status = HlRegisterHardware(HardwareModuleTimer, &Timer);
    if (!KSUCCESS(Status)) {
        goto Bcm2709TimerModuleEntryEnd;
    }

    SystemTimer = HlAllocateMemory(sizeof(BCM2709_SYSTEM_TIMER),
                                   BCM2709_ALLOCATION_TAG,
                                   FALSE,
                                   NULL);

    if (SystemTimer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto Bcm2709TimerModuleEntryEnd;
    }

    RtlZeroMemory(SystemTimer, sizeof(BCM2709_SYSTEM_TIMER));
    SystemTimer->Type = Bcm2709TimerSystemPeriodic3;
    Timer.Context = SystemTimer;
    Timer.CounterBitWidth = 32;
    Timer.Features = TIMER_FEATURE_READABLE |
                     TIMER_FEATURE_PERIODIC |
                     TIMER_FEATURE_ONE_SHOT;

    Timer.CounterFrequency = HlBcm2709Table->SystemTimerFrequency;
    Timer.Interrupt.Line.Type = InterruptLineGsi;
    Timer.Interrupt.Line.U.Gsi = HlBcm2709Table->SystemTimerGsiBase + 3;
    Timer.Interrupt.TriggerMode = InterruptModeUnknown;
    Timer.Interrupt.ActiveLevel = InterruptActiveLevelUnknown;
    Status = HlRegisterHardware(HardwareModuleTimer, &Timer);
    if (!KSUCCESS(Status)) {
        goto Bcm2709TimerModuleEntryEnd;
    }

Bcm2709TimerModuleEntryEnd:
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
HlpBcm2709TimerInitialize (
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

    ULONG ControlValue;
    PHYSICAL_ADDRESS PhysicalAddress;
    ULONG Size;
    KSTATUS Status;
    PBCM2709_TIMER Timer;

    //
    // Map the hardware for the given timer if that has not been done.
    //

    Timer = (PBCM2709_TIMER)Context;
    if ((Timer->Type == Bcm2709TimerArmPeriodic) ||
        (Timer->Type == Bcm2709TimerArmCounter)) {

        if (HlBcm2709ArmTimerBase == NULL) {
            PhysicalAddress = HlBcm2709Table->ArmTimerPhysicalAddress;
            Size = Bcm2709ArmTimerRegisterSize;
            HlBcm2709ArmTimerBase = HlMapPhysicalAddress(PhysicalAddress,
                                                         Size,
                                                         TRUE);

            if (HlBcm2709ArmTimerBase == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto Bcm2709TimerInitializeEnd;
            }
        }

    } else {
        if (HlBcm2709SystemTimerBase == NULL) {
            PhysicalAddress = HlBcm2709Table->SystemTimerPhysicalAddress;
            Size = Bcm2709SystemTimerRegisterSize;
            HlBcm2709SystemTimerBase = HlMapPhysicalAddress(PhysicalAddress,
                                                            Size,
                                                            TRUE);

            if (HlBcm2709SystemTimerBase == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto Bcm2709TimerInitializeEnd;
            }
        }
    }

    //
    // Initialize the given timer.
    //

    switch (Timer->Type) {
    case Bcm2709TimerArmPeriodic:
        WRITE_ARM_TIMER_REGISTER(Bcm2709ArmTimerPredivider, Timer->Predivider);
        ControlValue = READ_ARM_TIMER_REGISTER(Bcm2709ArmTimerControl);
        ControlValue &= ~BCM2709_ARM_TIMER_CONTROL_INTERRUPT_ENABLE;
        ControlValue |= (BCM2709_ARM_TIMER_CONTROL_ENABLED |
                         BCM2709_ARM_TIMER_CONTROL_DIVIDE_BY_1 |
                         BCM2709_ARM_TIMER_CONTROL_32_BIT);

        WRITE_ARM_TIMER_REGISTER(Bcm2709ArmTimerLoadValue, 0xFFFFFFFF);
        WRITE_ARM_TIMER_REGISTER(Bcm2709ArmTimerControl, ControlValue);
        WRITE_ARM_TIMER_REGISTER(Bcm2709ArmTimerInterruptClear, 1);
        break;

    case Bcm2709TimerArmCounter:
        ControlValue = READ_ARM_TIMER_REGISTER(Bcm2709ArmTimerControl);
        ControlValue &= ~BCM2709_ARM_TIMER_CONTROL_FREE_RUNNING_DIVIDE_MASK;
        ControlValue |= (Timer->Predivider <<
                         BCM2709_ARM_TIMER_CONTROL_FREE_RUNNING_DIVIDE_SHIFT) &
                        BCM2709_ARM_TIMER_CONTROL_FREE_RUNNING_DIVIDE_MASK;

        ControlValue |= BCM2709_ARM_TIMER_CONTROL_FREE_RUNNING_ENABLED;
        WRITE_ARM_TIMER_REGISTER(Bcm2709ArmTimerControl, ControlValue);
        break;

    case Bcm2709TimerSystemPeriodic1:
        WRITE_SYSTEM_TIMER_REGISTER(Bcm2709SystemTimerControl,
                                    BCM2709_SYSTEM_TIMER_CONTROL_MATCH_1);

        break;

    case Bcm2709TimerSystemPeriodic3:
        WRITE_SYSTEM_TIMER_REGISTER(Bcm2709SystemTimerControl,
                                    BCM2709_SYSTEM_TIMER_CONTROL_MATCH_3);

        break;

    case Bcm2709TimerSystemCounter:
        break;

    default:
        Status = STATUS_INVALID_PARAMETER;
        goto Bcm2709TimerInitializeEnd;
    }

    Status = STATUS_SUCCESS;

Bcm2709TimerInitializeEnd:
    return Status;
}

ULONGLONG
HlpBcm2709TimerRead (
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

    ULONG High1;
    ULONG High2;
    ULONG Low;
    PBCM2709_TIMER Timer;
    ULONGLONG Value;

    Timer = (PBCM2709_TIMER)Context;
    switch (Timer->Type) {
    case Bcm2709TimerArmPeriodic:
        Value = 0xFFFFFFFF;
        Value -= READ_ARM_TIMER_REGISTER(Bcm2709ArmTimerCurrentValue);
        break;

    case Bcm2709TimerArmCounter:
        Value = READ_ARM_TIMER_REGISTER(Bcm2709ArmTimerFreeRunningCounter);
        break;

    case Bcm2709TimerSystemPeriodic1:
    case Bcm2709TimerSystemPeriodic3:
        Value = READ_SYSTEM_TIMER_REGISTER(Bcm2709SystemTimerCounterLow);
        break;

    case Bcm2709TimerSystemCounter:

        //
        // Do a high-low-high read to make sure sure the words didn't tear.
        //

        do {
            High1 = READ_SYSTEM_TIMER_REGISTER(Bcm2709SystemTimerCounterHigh);
            Low = READ_SYSTEM_TIMER_REGISTER(Bcm2709SystemTimerCounterLow);
            High2 = READ_SYSTEM_TIMER_REGISTER(Bcm2709SystemTimerCounterHigh);

        } while (High1 != High2);

        Value = (((ULONGLONG)High1) << 32) | Low;
        break;

    default:
        Value = 0;
        break;
    }

    return Value;
}

KSTATUS
HlpBcm2709TimerArm (
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

    BCM2709_SYSTEM_TIMER_REGISTER CompareRegister;
    ULONG ControlValue;
    ULONG Counter;
    PBCM2709_SYSTEM_TIMER Timer;

    Timer = (PBCM2709_SYSTEM_TIMER)Context;
    if ((Timer->Type == Bcm2709TimerArmCounter) ||
        (Timer->Type == Bcm2709TimerSystemCounter)) {

        return STATUS_INVALID_PARAMETER;
    }

    if (TickCount > MAX_ULONG) {
        TickCount = MAX_ULONG;
    }

    switch (Timer->Type) {

    //
    // The ARM timer is armed by enabling it and setting the given ticks in the
    // load register. The timer will then count down, reloading said value once
    // the timer hits 0.
    //

    case Bcm2709TimerArmPeriodic:

        //
        // This Broadcom version of the SP804 does not appear to follow the
        // SP804 spec. with regards to the background load register. According
        // to the spec., when written, the load register is moved to the
        // current value register on the next rising edge of the clock. And
        // when the background value register is written, the value is stored
        // in the load register but not transferred to the current value
        // register until the current value reaches 0. Now, the spec. details
        // that if both the load register and background load register are
        // written before the next rising clock edge, that the current value
        // will be replaced by the value written to the load register (not the
        // background register) on the next clock edge. Unfortunately, the
        // Broadcom chip appears to load the background value into current
        // count if they are both written between clock edges.
        //
        // The workaround is to disable the counter, write the load register,
        // re-enable the counter to move the loaded value to the current value
        // register, and then, if necessary, write the background load value
        // register.
        //
        // Do not clear the interrupt here. On a multi-core system, this arm
        // request can race with the interrupt firing. If the interrupt were
        // cleared, then the other core would get interrupted only to find no
        // pending interrupts.
        //

        ControlValue = READ_ARM_TIMER_REGISTER(Bcm2709ArmTimerControl);
        ControlValue &= ~BCM2709_ARM_TIMER_CONTROL_ENABLED;
        WRITE_ARM_TIMER_REGISTER(Bcm2709ArmTimerControl, ControlValue);
        WRITE_ARM_TIMER_REGISTER(Bcm2709ArmTimerLoadValue, TickCount);
        ControlValue |= (BCM2709_ARM_TIMER_CONTROL_ENABLED |
                         BCM2709_ARM_TIMER_CONTROL_DIVIDE_BY_1 |
                         BCM2709_ARM_TIMER_CONTROL_32_BIT |
                         BCM2709_ARM_TIMER_CONTROL_INTERRUPT_ENABLE);

        WRITE_ARM_TIMER_REGISTER(Bcm2709ArmTimerControl, ControlValue);
        if (Mode == TimerModeOneShot) {
            WRITE_ARM_TIMER_REGISTER(Bcm2709ArmTimerBackgroundLoadValue,
                                     0xFFFFFFFF);

        }

        break;

    //
    // The System timers are armed by reading the low 32-bits of the counter,
    // adding the given ticks and setting that in the compare register. The
    // timer's interrupt will go off when the low 32-bits of the counter equals
    // the compare value.
    //

    case Bcm2709TimerSystemPeriodic1:
    case Bcm2709TimerSystemPeriodic3:
        CompareRegister = Bcm2709SystemTimerCompare1;
        ControlValue = BCM2709_SYSTEM_TIMER_CONTROL_MATCH_1;
        if (Timer->Type == Bcm2709TimerSystemPeriodic3) {
            CompareRegister = Bcm2709SystemTimerCompare3;
            ControlValue = BCM2709_SYSTEM_TIMER_CONTROL_MATCH_3;
        }

        Timer->Generation += 1;
        Timer->Mode = Mode;
        Timer->TickCount = (ULONG)TickCount;
        Timer->Generation += 1;
        WRITE_SYSTEM_TIMER_REGISTER(Bcm2709SystemTimerControl, ControlValue);
        Counter = READ_SYSTEM_TIMER_REGISTER(Bcm2709SystemTimerCounterLow);
        Counter += (ULONG)TickCount;
        WRITE_SYSTEM_TIMER_REGISTER(CompareRegister, Counter);
        break;

    default:
        return STATUS_INVALID_PARAMETER;
    }

    return STATUS_SUCCESS;
}

VOID
HlpBcm2709TimerDisarm (
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
    PBCM2709_SYSTEM_TIMER Timer;

    Timer = (PBCM2709_SYSTEM_TIMER)Context;
    switch (Timer->Type) {

    //
    // Disarm the ARM Timer by disabling its interrupt in the control register.
    //

    case Bcm2709TimerArmPeriodic:
        ControlValue = READ_ARM_TIMER_REGISTER(Bcm2709ArmTimerControl);
        ControlValue &= ~BCM2709_ARM_TIMER_CONTROL_INTERRUPT_ENABLE;
        ControlValue |= (BCM2709_ARM_TIMER_CONTROL_ENABLED |
                         BCM2709_ARM_TIMER_CONTROL_DIVIDE_BY_1 |
                         BCM2709_ARM_TIMER_CONTROL_32_BIT);

        WRITE_ARM_TIMER_REGISTER(Bcm2709ArmTimerControl, ControlValue);
        WRITE_ARM_TIMER_REGISTER(Bcm2709ArmTimerInterruptClear, 1);
        break;

    //
    // The System's periodic timers do not have an interrupt disable control
    // bit. Just leave the compare register programmed as is, but make sure it
    // does not get rearmed after it fires. At a frequency of 1MHz, the timer
    // will still expire every 71 minutes. So be it.
    //

    case Bcm2709TimerSystemPeriodic1:
    case Bcm2709TimerSystemPeriodic3:
        Timer->Generation += 1;
        Timer->Mode = TimerModeInvalid;
        Timer->TickCount = 0;
        Timer->Generation += 1;
        break;

    default:
        break;
    }

    return;
}

VOID
HlpBcm2709TimerAcknowledgeInterrupt (
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

    ULONG Compare;
    BCM2709_SYSTEM_TIMER_REGISTER CompareRegister;
    ULONG ControlValue;
    ULONG Counter;
    ULONG Generation1;
    ULONG Generation2;
    TIMER_MODE Mode;
    ULONG TickCount;
    PBCM2709_SYSTEM_TIMER Timer;

    Timer = (PBCM2709_SYSTEM_TIMER)Context;
    switch (Timer->Type) {

    //
    // Just write a 1 to the interrupt clear register to acknowledge the ARM
    // Timer's interrupt.
    //

    case Bcm2709TimerArmPeriodic:
        WRITE_ARM_TIMER_REGISTER(Bcm2709ArmTimerInterruptClear, 1);
        break;

    //
    // Acknowledge the interrupt by clearing the match bit in the control
    // register. If necessary, reprogram the compare register, as it does not
    // automatically get set for the next period. That said, if the compare
    // value has slipped behind the counter (possibly due to debugger
    // activity), make sure to schedule the next period in the future.
    //

    case Bcm2709TimerSystemPeriodic1:
    case Bcm2709TimerSystemPeriodic3:
        CompareRegister = Bcm2709SystemTimerCompare1;
        ControlValue = BCM2709_SYSTEM_TIMER_CONTROL_MATCH_1;
        if (Timer->Type == Bcm2709TimerSystemPeriodic3) {
            CompareRegister = Bcm2709SystemTimerCompare3;
            ControlValue = BCM2709_SYSTEM_TIMER_CONTROL_MATCH_3;
        }

        WRITE_SYSTEM_TIMER_REGISTER(Bcm2709SystemTimerControl, ControlValue);

        //
        // Loop attempting to get a consistent view of the mode and tick count.
        // If the read generations are not the same or both odd, it means that
        // the timer is actively being armed. Do not rearm it here. If the
        // generations read the same, a consistent view is found; arm the
        // timer. Unfortunately, this consistent view may be out of date by
        // the time it is armed. So, read the generation once again. If it got
        // updated, try to get another consistent view.
        //

        do {
            Generation1 = Timer->Generation;
            Mode = Timer->Mode;
            TickCount = Timer->TickCount;
            Generation2 = Timer->Generation;
            if ((Generation1 != Generation2) || ((Generation1 % 2) != 0)) {
                break;
            }

            if (Mode == TimerModeInvalid) {
                break;
            }

            Counter = READ_SYSTEM_TIMER_REGISTER(Bcm2709SystemTimerCounterLow);
            if (Mode == TimerModePeriodic) {
                Compare = READ_SYSTEM_TIMER_REGISTER(CompareRegister);
                Compare += TickCount;
                if (BCM2709_COUNTER_LESS_THAN(Counter, Compare) == FALSE) {
                    Compare = Counter + TickCount;
                }

            } else {
                Compare = Counter + TickCount;
            }

            WRITE_SYSTEM_TIMER_REGISTER(CompareRegister, Compare);

            //
            // Read the generation again. If it is different, it means another
            // core ran through and either disabled or armed the timer. The
            // above arming may have been incorrect.
            //

            Generation1 = Timer->Generation;

        } while (Generation1 != Generation2);

        break;

    default:
        break;
    }

    return;
}

