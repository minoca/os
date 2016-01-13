/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

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

#include <minoca/kernel.h>
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
    HlBcm2709KernelServices->ReadRegister32(HlBcm2709ArmTimerBase + (_Register))

//
// This macro writes to a BCM2709 ARM Timer register.
//

#define WRITE_ARM_TIMER_REGISTER(_Register, _Value)                   \
    HlBcm2709KernelServices->WriteRegister32((HlBcm2709ArmTimerBase + \
                                              (_Register)),           \
                                             (_Value))

//
// This macro reads from a BCM2709 System Timer register.
//

#define READ_SYSTEM_TIMER_REGISTER(_Register)                          \
    HlBcm2709KernelServices->ReadRegister32(HlBcm2709SystemTimerBase + \
                                            (_Register))

//
// This macro writes to a BCM2709 System Timer register.
//

#define WRITE_SYSTEM_TIMER_REGISTER(_Register, _Value)                  \
    HlBcm2709KernelServices->WriteRegister32(HlBcm2709SystemTimerBase + \
                                             (_Register),               \
                                             (_Value))

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
    Bcm2709TimerSystem0,
    Bcm2709TimerSystem1,
    Bcm2709TimerSystem2,
    Bcm2709TimerSystem3,
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

    This structure defines a BCM2709 timer.

Members:

    Type - Stores the type of BCM2709 timer that owns the data.

    Date - Stores a data value private to the timer type. This can be a
        predivider value for the ARM timers or a tick period for the system
        timers.

--*/

typedef struct _BCM2709_TIMER {
    BCM2709_TIMER_TYPE Type;
    ULONG Data;
} BCM2709_TIMER, *PBCM2709_TIMER;

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
    PHARDWARE_MODULE_KERNEL_SERVICES Services
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

    PBCM2709_TIMER Context;
    ULONGLONG Frequency;
    ULONGLONG MaxFrequency;
    ULONG Predivider;
    KSTATUS Status;
    TIMER_DESCRIPTION Timer;

    //
    // Interrupt controllers are always initialized before timers, so the table
    // and services should already be set up.
    //

    if ((HlBcm2709Table == NULL) || (HlBcm2709KernelServices == NULL)) {
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

    HlBcm2709KernelServices->ZeroMemory(&Timer, sizeof(TIMER_DESCRIPTION));
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
    // readable, but can change dynamically in reduced power states.
    //

    Context = HlBcm2709KernelServices->AllocateMemory(sizeof(BCM2709_TIMER),
                                                      BCM2709_ALLOCATION_TAG,
                                                      FALSE,
                                                      NULL);

    if (Context == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto Bcm2709TimerModuleEntryEnd;
    }

    Context->Type = Bcm2709TimerArmPeriodic;
    Context->Data = Predivider;
    Timer.Context = Context;
    Timer.Features = TIMER_FEATURE_READABLE |
                     TIMER_FEATURE_PERIODIC |
                     TIMER_FEATURE_SPEED_VARIES;

    Timer.CounterBitWidth = 32;
    Timer.CounterFrequency = Frequency;
    Timer.Interrupt.Line.Type = InterruptLineGsi;
    Timer.Interrupt.Line.Gsi = HlBcm2709Table->ArmTimerGsi;
    Timer.Interrupt.TriggerMode = InterruptModeUnknown;
    Timer.Interrupt.ActiveLevel = InterruptActiveLevelUnknown;
    Status = HlBcm2709KernelServices->Register(HardwareModuleTimer, &Timer);
    if (!KSUCCESS(Status)) {
        goto Bcm2709TimerModuleEntryEnd;
    }

    //
    // Register the BCM2709 ARM free running counter. It is readable but its
    // speed can change dynamically in reduced power status.
    //

    Context = HlBcm2709KernelServices->AllocateMemory(sizeof(BCM2709_TIMER),
                                                      BCM2709_ALLOCATION_TAG,
                                                      FALSE,
                                                      NULL);

    if (Context == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto Bcm2709TimerModuleEntryEnd;
    }

    Context->Type = Bcm2709TimerArmCounter;
    Context->Data = Predivider;
    Timer.Context = Context;
    Timer.CounterBitWidth = 32;
    Timer.Features = TIMER_FEATURE_READABLE | TIMER_FEATURE_SPEED_VARIES;
    Timer.CounterFrequency = Frequency;
    Timer.Interrupt.Line.Type = InterruptLineInvalid;
    Status = HlBcm2709KernelServices->Register(HardwareModuleTimer, &Timer);
    if (!KSUCCESS(Status)) {
        goto Bcm2709TimerModuleEntryEnd;
    }

    //
    // Initialize the BCM2709 System timer's free running counter. It is
    // actually 64-bits, but reading both the high and low registers
    // synchronously on every request when it runs at 1MHz seems wasteful. Let
    // the HL software handle the rollover.
    //

    Context = HlBcm2709KernelServices->AllocateMemory(sizeof(BCM2709_TIMER),
                                                      BCM2709_ALLOCATION_TAG,
                                                      FALSE,
                                                      NULL);

    if (Context == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto Bcm2709TimerModuleEntryEnd;
    }

    Context->Type = Bcm2709TimerSystemCounter;
    Context->Data = 0;
    Timer.Context = Context;
    Timer.CounterBitWidth = 32;
    Timer.Features = TIMER_FEATURE_READABLE;
    Timer.CounterFrequency = HlBcm2709Table->SystemTimerFrequency;
    Timer.Interrupt.Line.Type = InterruptLineInvalid;
    Status = HlBcm2709KernelServices->Register(HardwareModuleTimer, &Timer);
    if (!KSUCCESS(Status)) {
        goto Bcm2709TimerModuleEntryEnd;
    }

    //
    // Initialize the two "periodic" BCM2709 System Timers that are not in use
    // by the GPU. Those are timers 1 and 3. They are not truly periodic in
    // that they do not automatically reload, but they make do for profiler
    // timers.
    //

    Context = HlBcm2709KernelServices->AllocateMemory(sizeof(BCM2709_TIMER),
                                                      BCM2709_ALLOCATION_TAG,
                                                      FALSE,
                                                      NULL);

    if (Context == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto Bcm2709TimerModuleEntryEnd;
    }

    Context->Type = Bcm2709TimerSystem1;
    Context->Data = 0;
    Timer.Context = Context;
    Timer.CounterBitWidth = 32;
    Timer.Features = TIMER_FEATURE_READABLE | TIMER_FEATURE_PERIODIC;
    Timer.CounterFrequency = HlBcm2709Table->SystemTimerFrequency;
    Timer.Interrupt.Line.Type = InterruptLineGsi;
    Timer.Interrupt.Line.Gsi = HlBcm2709Table->SystemTimerGsiBase + 1;
    Timer.Interrupt.TriggerMode = InterruptModeUnknown;
    Timer.Interrupt.ActiveLevel = InterruptActiveLevelUnknown;
    Status = HlBcm2709KernelServices->Register(HardwareModuleTimer, &Timer);
    if (!KSUCCESS(Status)) {
        goto Bcm2709TimerModuleEntryEnd;
    }

    Context = HlBcm2709KernelServices->AllocateMemory(sizeof(BCM2709_TIMER),
                                                      BCM2709_ALLOCATION_TAG,
                                                      FALSE,
                                                      NULL);

    if (Context == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto Bcm2709TimerModuleEntryEnd;
    }

    Context->Type = Bcm2709TimerSystem3;
    Context->Data = 0;
    Timer.Context = Context;
    Timer.CounterBitWidth = 32;
    Timer.Features = TIMER_FEATURE_READABLE | TIMER_FEATURE_PERIODIC;
    Timer.CounterFrequency = HlBcm2709Table->SystemTimerFrequency;
    Timer.Interrupt.Line.Type = InterruptLineGsi;
    Timer.Interrupt.Line.Gsi = HlBcm2709Table->SystemTimerGsiBase + 3;
    Timer.Interrupt.TriggerMode = InterruptModeUnknown;
    Timer.Interrupt.ActiveLevel = InterruptActiveLevelUnknown;
    Status = HlBcm2709KernelServices->Register(HardwareModuleTimer, &Timer);
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
    PHARDWARE_MODULE_MAP_PHYSICAL_ADDRESS MapPhysicalAddress;
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
            MapPhysicalAddress = HlBcm2709KernelServices->MapPhysicalAddress;
            PhysicalAddress = HlBcm2709Table->ArmTimerPhysicalAddress;
            Size = Bcm2709ArmTimerRegisterSize;
            HlBcm2709ArmTimerBase = MapPhysicalAddress(PhysicalAddress,
                                                       Size,
                                                       TRUE);

            if (HlBcm2709ArmTimerBase == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto Bcm2709TimerInitializeEnd;
            }
        }

    } else {
        if (HlBcm2709SystemTimerBase == NULL) {
            MapPhysicalAddress = HlBcm2709KernelServices->MapPhysicalAddress;
            PhysicalAddress = HlBcm2709Table->SystemTimerPhysicalAddress;
            Size = Bcm2709SystemTimerRegisterSize;
            HlBcm2709SystemTimerBase = MapPhysicalAddress(PhysicalAddress,
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
        WRITE_ARM_TIMER_REGISTER(Bcm2709ArmTimerPredivider, Timer->Data);
        ControlValue = READ_ARM_TIMER_REGISTER(Bcm2709ArmTimerControl);
        ControlValue &= ~BCM2709_ARM_TIMER_CONTROL_INTERRUPT_ENABLE;
        ControlValue |= (BCM2709_ARM_TIMER_CONTROL_ENABLED |
                         BCM2709_ARM_TIMER_CONTROL_DIVIDE_BY_1 |
                         BCM2709_ARM_TIMER_CONTROL_32_BIT);

        WRITE_ARM_TIMER_REGISTER(Bcm2709ArmTimerControl, ControlValue);
        WRITE_ARM_TIMER_REGISTER(Bcm2709ArmTimerInterruptClear, 1);
        break;

    case Bcm2709TimerArmCounter:
        ControlValue = READ_ARM_TIMER_REGISTER(Bcm2709ArmTimerControl);
        ControlValue &= ~BCM2709_ARM_TIMER_CONTROL_FREE_RUNNING_DIVIDE_MASK;
        ControlValue |= (Timer->Data <<
                         BCM2709_ARM_TIMER_CONTROL_FREE_RUNNING_DIVIDE_SHIFT) &
                        BCM2709_ARM_TIMER_CONTROL_FREE_RUNNING_DIVIDE_MASK;

        ControlValue |= BCM2709_ARM_TIMER_CONTROL_FREE_RUNNING_ENABLED;
        WRITE_ARM_TIMER_REGISTER(Bcm2709ArmTimerControl, ControlValue);
        break;

    case Bcm2709TimerSystem1:
        WRITE_SYSTEM_TIMER_REGISTER(Bcm2709SystemTimerControl,
                                    BCM2709_SYSTEM_TIMER_CONTROL_MATCH_1);

        break;

    case Bcm2709TimerSystem3:
        WRITE_SYSTEM_TIMER_REGISTER(Bcm2709SystemTimerControl,
                                    BCM2709_SYSTEM_TIMER_CONTROL_MATCH_3);

        break;

    case Bcm2709TimerSystemCounter:
        break;

    default:

        ASSERT(FALSE);

        break;
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

    ULONG Compare;
    BCM2709_SYSTEM_TIMER_REGISTER CompareRegister;
    ULONG Counter;
    PBCM2709_TIMER Timer;
    ULONG Value;

    Timer = (PBCM2709_TIMER)Context;
    switch (Timer->Type) {
    case Bcm2709TimerArmPeriodic:
        Value = 0xFFFFFFFF;
        Value -= READ_ARM_TIMER_REGISTER(Bcm2709ArmTimerCurrentValue);
        break;

    case Bcm2709TimerArmCounter:
        Value = READ_ARM_TIMER_REGISTER(Bcm2709ArmTimerFreeRunningCounter);
        break;

    case Bcm2709TimerSystem1:
    case Bcm2709TimerSystem3:
        CompareRegister = Bcm2709SystemTimerCompare1;
        if (Timer->Type == Bcm2709TimerSystem3) {
            CompareRegister = Bcm2709SystemTimerCompare3;
        }

        Compare = READ_SYSTEM_TIMER_REGISTER(CompareRegister);
        Counter = READ_SYSTEM_TIMER_REGISTER(Bcm2709SystemTimerCounterLow);
        Value = Timer->Data - (Compare - Counter);
        break;

    case Bcm2709TimerSystemCounter:
        Value = READ_SYSTEM_TIMER_REGISTER(Bcm2709SystemTimerCounterLow);
        break;

    default:

        ASSERT(FALSE);

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

    Mode - Supplies the mode to arm the timer in (periodic or one-shot). The
        system will never request a mode not supported by the timer's feature
        bits.

    TickCount - Supplies the interval, in ticks, from now for the timer to fire
        in.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    BCM2709_SYSTEM_TIMER_REGISTER CompareRegister;
    ULONG ControlValue;
    ULONG Counter;
    PBCM2709_TIMER Timer;

    Timer = (PBCM2709_TIMER)Context;
    if ((Timer->Type == Bcm2709TimerArmCounter) ||
        (Timer->Type == Bcm2709TimerSystemCounter)) {

        return STATUS_INVALID_PARAMETER;
    }

    if (TickCount >= MAX_ULONG) {
        return STATUS_INVALID_PARAMETER;
    }

    if (Mode == TimerModeOneShot) {
        return STATUS_INVALID_PARAMETER;
    }

    switch (Timer->Type) {

    //
    // The ARM timer is armed by enabling it and setting the given ticks in the
    // load register. The timer will then count down, reloading said value once
    // the timer hits 0.
    //

    case Bcm2709TimerArmPeriodic:
        ControlValue = READ_ARM_TIMER_REGISTER(Bcm2709ArmTimerControl);
        ControlValue |= (BCM2709_ARM_TIMER_CONTROL_ENABLED |
                         BCM2709_ARM_TIMER_CONTROL_DIVIDE_BY_1 |
                         BCM2709_ARM_TIMER_CONTROL_32_BIT |
                         BCM2709_ARM_TIMER_CONTROL_INTERRUPT_ENABLE);

        //
        // Set the timer to its maximum value, set the configuration, clear the
        // interrupt, then set the value.
        //

        WRITE_ARM_TIMER_REGISTER(Bcm2709ArmTimerLoadValue, 0xFFFFFFFF);
        WRITE_ARM_TIMER_REGISTER(Bcm2709ArmTimerControl, ControlValue);
        WRITE_ARM_TIMER_REGISTER(Bcm2709ArmTimerInterruptClear, 1);
        WRITE_ARM_TIMER_REGISTER(Bcm2709ArmTimerLoadValue, TickCount);
        break;

    //
    // The System timers are armed by reading the low 32-bits of the counter,
    // adding the given ticks and setting that in the compare register. The
    // timer's interrupt will go off when the low 32-bits of the counter equals
    // the compare value.
    //

    case Bcm2709TimerSystem1:
    case Bcm2709TimerSystem3:
        CompareRegister = Bcm2709SystemTimerCompare1;
        ControlValue = BCM2709_SYSTEM_TIMER_CONTROL_MATCH_1;
        if (Timer->Type == Bcm2709TimerSystem3) {
            CompareRegister = Bcm2709SystemTimerCompare3;
            ControlValue = BCM2709_SYSTEM_TIMER_CONTROL_MATCH_3;
        }

        Timer->Data = (ULONG)TickCount;
        WRITE_SYSTEM_TIMER_REGISTER(Bcm2709SystemTimerControl, ControlValue);
        Counter = READ_SYSTEM_TIMER_REGISTER(Bcm2709SystemTimerCounterLow);
        Counter += (ULONG)TickCount;
        WRITE_SYSTEM_TIMER_REGISTER(CompareRegister, Counter);
        break;

    default:

        ASSERT(FALSE);

        break;
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

    BCM2709_SYSTEM_TIMER_REGISTER CompareRegister;
    ULONG ControlValue;
    PBCM2709_TIMER Timer;

    Timer = (PBCM2709_TIMER)Context;
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
    // bit. Just zero out the compare register. With a frequency of 1MHz, it
    // may be that this interrupt keeps hitting every 71 minutes. So be it.
    //

    case Bcm2709TimerSystem1:
    case Bcm2709TimerSystem3:
        CompareRegister = Bcm2709SystemTimerCompare1;
        ControlValue = BCM2709_SYSTEM_TIMER_CONTROL_MATCH_1;
        if (Timer->Type == Bcm2709TimerSystem3) {
            CompareRegister = Bcm2709SystemTimerCompare3;
            ControlValue = BCM2709_SYSTEM_TIMER_CONTROL_MATCH_3;
        }

        WRITE_SYSTEM_TIMER_REGISTER(Bcm2709SystemTimerControl, ControlValue);
        WRITE_SYSTEM_TIMER_REGISTER(CompareRegister, 0);
        break;

    default:

        ASSERT(FALSE);

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
    PBCM2709_TIMER Timer;

    Timer = (PBCM2709_TIMER)Context;
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
    // register. Also reprogram the compare register, as it does not
    // automatically get set for the next period. That said, if the compare
    // value has slipped behind the counter (possibly due debugger activity),
    // make sure to schedule the next period in the future.
    //

    case Bcm2709TimerSystem1:
    case Bcm2709TimerSystem3:
        CompareRegister = Bcm2709SystemTimerCompare1;
        ControlValue = BCM2709_SYSTEM_TIMER_CONTROL_MATCH_1;
        if (Timer->Type == Bcm2709TimerSystem3) {
            CompareRegister = Bcm2709SystemTimerCompare3;
            ControlValue = BCM2709_SYSTEM_TIMER_CONTROL_MATCH_3;
        }

        WRITE_SYSTEM_TIMER_REGISTER(Bcm2709SystemTimerControl, ControlValue);
        Compare = READ_SYSTEM_TIMER_REGISTER(CompareRegister);
        Compare += Timer->Data;
        Counter = READ_SYSTEM_TIMER_REGISTER(Bcm2709SystemTimerCounterLow);
        if (BCM2709_COUNTER_LESS_THAN(Counter, Compare) == FALSE) {
            Compare = Counter + Timer->Data;
        }

        WRITE_SYSTEM_TIMER_REGISTER(CompareRegister, Compare);
        break;

    default:

        ASSERT(FALSE);

        break;
    }

    return;
}

