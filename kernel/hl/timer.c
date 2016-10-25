/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    timer.c

Abstract:

    This module implements timer support for the hardware library.

Author:

    Evan Green 28-Oct-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/bootload.h>
#include "hlp.h"
#include "calendar.h"
#include "intrupt.h"
#include "timer.h"
#include "clock.h"
#include "profiler.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Store the amount of time, in microseconds, that the refernce stall should
// take.
//

#define REFERENCE_STALL_DURATION 250000

//
// Find timer options.
//

#define FIND_TIMER_OPTION_INCLUDE_USED_FOR_INTERRUPT_ANY      0x00000001
#define FIND_TIMER_OPTION_INCLUDE_USED_FOR_COUNTER            0x00000002
#define FIND_TIMER_OPTION_INCLUDE_USED_FOR_INTERRUPT_ABSOLUTE 0x00000004

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
HlpTimerInitialize (
    PHARDWARE_TIMER Timer
    );

KSTATUS
HlpTimerMeasureUnknownFrequencies (
    VOID
    );

PHARDWARE_TIMER
HlpTimerFindIdealMeasuringSource (
    VOID
    );

PHARDWARE_TIMER
HlpTimerFindIdealClockSource (
    VOID
    );

PHARDWARE_TIMER
HlpTimerFindIdealProfilerSource (
    VOID
    );

PHARDWARE_TIMER
HlpTimerFindIdealTimeCounter (
    VOID
    );

PHARDWARE_TIMER
HlpTimerFindIdealProcessorCounter (
    VOID
    );

PHARDWARE_TIMER
HlpTimerFind (
    ULONG RequiredFeatures,
    ULONG RequiredNonFeatures,
    ULONG FindOptions
    );

VOID
HlpTimerBusyStall (
    PHARDWARE_TIMER Timer,
    ULONG Microseconds
    );

KSTATUS
HlpTimerAssignRoles (
    VOID
    );

VOID
HlpTimerResetCounterOffset (
    PHARDWARE_TIMER Timer,
    ULONGLONG NewValue
    );

KSTATUS
HlpTimerCreateSoftUpdateTimer (
    PHARDWARE_TIMER Timer
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store the list of timers.
//

LIST_ENTRY HlTimers;

//
// Store pointers to the timers backing system services.
//

PHARDWARE_TIMER HlClockTimer;
PHARDWARE_TIMER HlProfilerTimer;
PHARDWARE_TIMER HlTimeCounter;
PHARDWARE_TIMER HlProcessorCounter;

//
// If KD stalls are temporarily disabled, remember the original value so they
// can be re-enabled when a stall source is brought online.
//

ULONG HlOriginalKdConnectionTimeout;

//
// ------------------------------------------------------------------ Functions
//

KERNEL_API
ULONGLONG
HlQueryTimeCounter (
    VOID
    )

/*++

Routine Description:

    This routine queries the time counter hardware and returns a 64-bit
    monotonically non-decreasing value that represents the number of timer ticks
    since the system was started. This value will continue to count through all
    idle and sleep states.

    This routine can be called at any runlevel.

Arguments:

    None.

Return Value:

    Returns the number of timer ticks that have elapsed since the system was
    booted. The absolute time between successive ticks can be retrieved from the
    Query Time Counter Frequency function.

--*/

{

    return HlpTimerExtendedQuery(HlTimeCounter);
}

KERNEL_API
ULONGLONG
HlQueryTimeCounterFrequency (
    VOID
    )

/*++

Routine Description:

    This routine returns the frequency of the time counter. This frequency will
    never change after it is set on boot.

    This routine can be called at any runlevel.

Arguments:

    None.

Return Value:

    Returns the frequency of the time counter, in Hertz.

--*/

{

    if (HlTimeCounter == NULL) {
        return 0;
    }

    return HlTimeCounter->CounterFrequency;
}

KERNEL_API
ULONGLONG
HlQueryProcessorCounterFrequency (
    VOID
    )

/*++

Routine Description:

    This routine returns the frequency of the processor counter. This frequency
    will never change after it is set on boot.

    This routine can be called at any runlevel.

Arguments:

    None.

Return Value:

    Returns the frequency of the time counter, in Hertz.

--*/

{

    if (HlProcessorCounter == NULL) {
        return 0;
    }

    return HlProcessorCounter->CounterFrequency;
}

KERNEL_API
VOID
HlBusySpin (
    ULONG Microseconds
    )

/*++

Routine Description:

    This routine spins for at least the given number of microseconds by
    repeatedly reading a hardware timer. This routine should be avoided if at
    all possible, as it simply burns CPU cycles.

    This routine can be called at any runlevel.

Arguments:

    Microseconds - Supplies the number of microseconds to spin for.

Return Value:

    Returns the frequency of the time counter, in Hertz.

--*/

{

    HlpTimerBusyStall(HlTimeCounter, Microseconds);
    return;
}

KSTATUS
HlpInitializeTimersPreDebugger (
    PKERNEL_INITIALIZATION_BLOCK Parameters,
    ULONG ProcessorNumber
    )

/*++

Routine Description:

    This routine implements early timer initialization for the hardware module
    API layer. This routine is *undebuggable*, as it is called before the
    debugger is brought online.

Arguments:

    Parameters - Supplies an optional pointer to the kernel initialization
        parameters. This parameter may be NULL.

    ProcessorNumber - Supplies the processor index of this processor.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    KSTATUS Status;
    PHARDWARE_TIMER Timer;

    if ((ProcessorNumber != 0) || (Parameters == NULL)) {
        return STATUS_SUCCESS;
    }

    INITIALIZE_LIST_HEAD(&HlTimers);
    HlpArchInitializeTimersPreDebugger();

    //
    // Attempt to find and initialize the processor counter.
    //

    CurrentEntry = HlTimers.Next;
    while (CurrentEntry != &HlTimers) {
        Timer = LIST_VALUE(CurrentEntry, HARDWARE_TIMER, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if ((Timer->Features & TIMER_FEATURE_PROCESSOR_COUNTER) != 0) {
            if (Timer->CounterFrequency == 0) {
                Timer->CounterFrequency = Parameters->CycleCounterFrequency;
            }
        }

        if (Timer->CounterFrequency != 0) {
            Status = HlpTimerInitialize(Timer);
            if (KSUCCESS(Status)) {
                if ((Timer->Features & TIMER_FEATURE_READABLE) != 0) {
                    HlProcessorCounter = Timer;
                    HlTimeCounter = Timer;
                }
            }
        }
    }

    //
    // If no stall source was set up, then inhibit the debugger from using it.
    //

    if (HlTimeCounter == NULL) {
        HlOriginalKdConnectionTimeout = KdSetConnectionTimeout(MAX_ULONG);
    }

    return STATUS_SUCCESS;
}

KSTATUS
HlpInitializeTimers (
    PKERNEL_INITIALIZATION_BLOCK Parameters
    )

/*++

Routine Description:

    This routine initializes the timer subsystem.

Arguments:

    Parameters - Supplies a pointer to the kernel's initialization information.

Return Value:

    Status code.

--*/

{

    UINTN AllocationSize;
    PULONGLONG CountArray;
    PLIST_ENTRY CurrentEntry;
    ULONG ProcessorCount;
    BOOL RestoreKd;
    KSTATUS Status;
    PHARDWARE_TIMER Timer;

    if (KeGetCurrentProcessorNumber() == 0) {
        INITIALIZE_LIST_HEAD(&HlCalendarTimers);

        //
        // If no time counter was able to be set up during early
        // initialization, then KD stalls were disabled. Mark now to restore
        // them.
        //

        RestoreKd = FALSE;
        if (HlTimeCounter == NULL) {
            RestoreKd = TRUE;
        }

        //
        // Loop through any timers that were created super early and allocate
        // per-processor arrays for them if necessary.
        //

        ProcessorCount = HlGetMaximumProcessorCount();

        ASSERT(ProcessorCount != 0);

        CurrentEntry = HlTimers.Next;
        while (CurrentEntry != &HlTimers) {
            Timer = LIST_VALUE(CurrentEntry, HARDWARE_TIMER, ListEntry);
            CurrentEntry = CurrentEntry->Next;
            if ((Timer->CounterBitWidth < 64) &&
                ((Timer->Features & TIMER_FEATURE_VARIANT) != 0) &&
                ((Timer->Features & TIMER_FEATURE_PER_PROCESSOR) != 0)) {

                AllocationSize = ProcessorCount * sizeof(ULONGLONG);
                CountArray = HlAllocateMemory(AllocationSize,
                                              HL_POOL_TAG,
                                              FALSE,
                                              NULL);

                if (CountArray == NULL) {
                    Status = STATUS_INSUFFICIENT_RESOURCES;
                    goto InitializeTimersEnd;
                }

                RtlZeroMemory(CountArray, AllocationSize);
                CountArray[0] = Timer->CurrentCount;
                Timer->CurrentCounts = CountArray;
            }
        }

        //
        // Perform architecture-specific initialization.
        //

        Status = HlpArchInitializeTimers();
        if (!KSUCCESS(Status)) {
            goto InitializeTimersEnd;
        }

        //
        // Measure the frequencies of any unknown timers.
        //

        Status = HlpTimerMeasureUnknownFrequencies();
        if (!KSUCCESS(Status)) {
            goto InitializeTimersEnd;
        }

        //
        // Assign timers to system services.
        //

        Status = HlpTimerAssignRoles();
        if (!KSUCCESS(Status)) {
            goto InitializeTimersEnd;
        }

        //
        // Set t = 0 for the time counter.
        //

        HlpTimerResetCounterOffset(HlTimeCounter, 0);

        //
        // Restore the original KD connection timeout if it was disabled.
        //

        if (RestoreKd != FALSE) {
            KdSetConnectionTimeout(HlOriginalKdConnectionTimeout);
        }

        //
        // Fire up the clock.
        //

        Status = HlpTimerInitializeClock();
        if (!KSUCCESS(Status)) {
            goto InitializeTimersEnd;
        }

        //
        // Initialize the profiler.
        //

        Status = HlpTimerInitializeProfiler();
        if (!KSUCCESS(Status)) {
            goto InitializeTimersEnd;
        }

        //
        // Initialize the clock for polling the debugger now that the final
        // time counter source has been set up.
        //

        KeUpdateClockForProfiling(FALSE);

        //
        // Create a soft timer to ensure that the system wakes from idle at
        // least often enough to observe every half-rollover of the time
        // counter.
        //

        Status = HlpTimerCreateSoftUpdateTimer(HlTimeCounter);
        if (!KSUCCESS(Status)) {
            goto InitializeTimersEnd;
        }

    //
    // Perform initialization for all other processors.
    //

    } else {
        CurrentEntry = HlTimers.Next;
        while (CurrentEntry != &HlTimers) {
            Timer = LIST_VALUE(CurrentEntry, HARDWARE_TIMER, ListEntry);
            CurrentEntry = CurrentEntry->Next;
            if (((Timer->Features & TIMER_FEATURE_PER_PROCESSOR) != 0) &&
                ((Timer->Flags & TIMER_FLAG_INITIALIZED) != 0)) {

                //
                // Initialize the timer on this new processor. If the timer
                // fails and it's backing a system service, this is a problem.
                //

                Status = HlpTimerInitialize(Timer);
                if (!KSUCCESS(Status)) {
                    goto InitializeTimersEnd;
                }
            }
        }

        //
        // Fire up the clock.
        //

        Status = HlpTimerInitializeClock();
        if (!KSUCCESS(Status)) {
            goto InitializeTimersEnd;
        }

        //
        // Finish profiler initialization.
        //

        Status = HlpTimerInitializeProfiler();
        if (!KSUCCESS(Status)) {
            goto InitializeTimersEnd;
        }
    }

InitializeTimersEnd:
    return Status;
}

KSTATUS
HlpTimerRegisterHardware (
    PTIMER_DESCRIPTION TimerDescription
    )

/*++

Routine Description:

    This routine is called to register a new timer with the system.

Arguments:

    TimerDescription - Supplies a pointer to a structure describing the new
        timer.

Return Value:

    Status code.

--*/

{

    UINTN AbsoluteArrayOffset;
    UINTN AllocationSize;
    ULONG ArmFlags;
    UINTN CounterArrayOffset;
    ULONG DisarmFlags;
    ULONG InterruptFlags;
    ULONG ProcessorCount;
    KSTATUS Status;
    PHARDWARE_TIMER Timer;

    Timer = NULL;

    //
    // Check the table version.
    //

    if (TimerDescription->TableVersion < TIMER_DESCRIPTION_VERSION) {
        Status = STATUS_INVALID_PARAMETER;
        goto TimerRegisterHardwareEnd;
    }

    //
    // Check required function pointers.
    //

    if (TimerDescription->FunctionTable.Initialize == NULL) {
        Status = STATUS_INVALID_PARAMETER;
        goto TimerRegisterHardwareEnd;
    }

    //
    // Non-periodic, absolute timers must be readable and per-processor.
    //

    if (((TimerDescription->Features & TIMER_FEATURE_ABSOLUTE) != 0) &&
        ((TimerDescription->Features & TIMER_FEATURE_PERIODIC) == 0)) {

        if ((TimerDescription->Features & TIMER_FEATURE_READABLE) == 0) {
            Status = STATUS_INVALID_PARAMETER;
            goto TimerRegisterHardwareEnd;
        }

        //
        // The requirement that non-periodic absolute timers must be
        // per-processor stems from the difficulty of synchronizing rearming
        // the timer during acknowledge interrupt. Handling races between one
        // core arming the timer and another core acknowledging the timer's
        // interrupt adds complexity that is not yet worth including.
        //

        if ((TimerDescription->Features & TIMER_FEATURE_PER_PROCESSOR) == 0) {
            Status = STATUS_INVALID_PARAMETER;
            goto TimerRegisterHardwareEnd;
        }
    }

    //
    // If the readable feature is set then the read counter routine is required.
    //

    if (((TimerDescription->Features & TIMER_FEATURE_READABLE) != 0) &&
        (TimerDescription->FunctionTable.ReadCounter == NULL)) {

        Status = STATUS_INVALID_PARAMETER;
        goto TimerRegisterHardwareEnd;
    }

    //
    // If the writable feature is set then the write counter routine is
    // required.
    //

    if (((TimerDescription->Features & TIMER_FEATURE_WRITABLE) != 0) &&
        (TimerDescription->FunctionTable.WriteCounter == NULL)) {

        Status = STATUS_INVALID_PARAMETER;
        goto TimerRegisterHardwareEnd;
    }

    //
    // If the one-shot, periodic or absolute deadline features are set then the
    // arm timer routine is required.
    //

    ArmFlags = TIMER_FEATURE_ONE_SHOT |
               TIMER_FEATURE_PERIODIC |
               TIMER_FEATURE_ABSOLUTE;

    if (((TimerDescription->Features & ArmFlags) != 0) &&
        (TimerDescription->FunctionTable.Arm == NULL)) {

        Status = STATUS_INVALID_PARAMETER;
        goto TimerRegisterHardwareEnd;
    }

    //
    // If the periodic or absolute deadline features are set then the disarm
    // timer routine is required.
    //

    DisarmFlags = TIMER_FEATURE_PERIODIC | TIMER_FEATURE_ABSOLUTE;
    if (((TimerDescription->Features & DisarmFlags) != 0) &&
        (TimerDescription->FunctionTable.Disarm == NULL)) {

        Status = STATUS_INVALID_PARAMETER;
        goto TimerRegisterHardwareEnd;
    }

    //
    // The counter bit width had better not be zero.
    //

    if (TimerDescription->CounterBitWidth < 2) {
        Status = STATUS_INVALID_PARAMETER;
        goto TimerRegisterHardwareEnd;
    }

    //
    // If the timer generates interrupts, it had better have properly described
    // its interrupt.
    //

    InterruptFlags = TIMER_FEATURE_ONE_SHOT |
                     TIMER_FEATURE_PERIODIC |
                     TIMER_FEATURE_ABSOLUTE;

    if (((TimerDescription->Features & InterruptFlags) != 0) &&
        (TimerDescription->Interrupt.Line.Type == InterruptLineInvalid)) {

        Status = STATUS_INVALID_PARAMETER;
        goto TimerRegisterHardwareEnd;
    }

    //
    // Allocate the new controller object. If the timer varies per processor
    // then the current count array needs to be kept per processor. If this is
    // very early initialization (pre-debugger), then the processor count might
    // return zero, in which case don't allocate the per-processor array yet.
    // That will be created later.
    //

    CounterArrayOffset = 0;
    AllocationSize = sizeof(HARDWARE_TIMER);
    if ((TimerDescription->CounterBitWidth < 64) &&
        ((TimerDescription->Features & TIMER_FEATURE_VARIANT) != 0) &&
        ((TimerDescription->Features & TIMER_FEATURE_PER_PROCESSOR) != 0)) {

        ProcessorCount = HlGetMaximumProcessorCount();
        if (ProcessorCount > 1) {

            //
            // Align the allocation size so that the array of 64-bit integers is
            // aligned to 64-bits, which is required for doing atomic access on
            // some architectures.
            //

            AllocationSize = ALIGN_RANGE_UP(AllocationSize, sizeof(ULONGLONG));
            CounterArrayOffset = AllocationSize;
            AllocationSize += ProcessorCount * sizeof(ULONGLONG);
        }
    }

    //
    // Non-periodic, absolute timers require extra data in order to fake
    // periodic support.
    //

    AbsoluteArrayOffset = 0;
    if (((TimerDescription->Features & TIMER_FEATURE_ABSOLUTE) != 0) &&
        ((TimerDescription->Features & TIMER_FEATURE_PERIODIC) == 0)) {

        ASSERT((TimerDescription->Features & TIMER_FEATURE_PER_PROCESSOR) != 0);

        ProcessorCount = HlGetMaximumProcessorCount();

        ASSERT(ProcessorCount != 0);

        AllocationSize = ALIGN_RANGE_UP(AllocationSize, sizeof(ULONGLONG));
        AbsoluteArrayOffset = AllocationSize;
        AllocationSize += ProcessorCount * sizeof(HARDWARE_TIMER_ABSOLUTE_DATA);
    }

    Timer = HlAllocateMemory(AllocationSize, HL_POOL_TAG, FALSE, NULL);
    if (Timer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto TimerRegisterHardwareEnd;
    }

    RtlZeroMemory(Timer, AllocationSize);

    //
    // Initialize the new timer based on the description.
    //

    RtlCopyMemory(&(Timer->FunctionTable),
                  &(TimerDescription->FunctionTable),
                  sizeof(TIMER_FUNCTION_TABLE));

    Timer->Identifier = TimerDescription->Identifier;
    Timer->PrivateContext = TimerDescription->Context;
    Timer->Features = TimerDescription->Features;
    Timer->CounterBitWidth = TimerDescription->CounterBitWidth;
    Timer->CounterFrequency = TimerDescription->CounterFrequency;
    Timer->Flags = 0;
    Timer->Interrupt = TimerDescription->Interrupt;
    Timer->InterruptRunLevel = RunLevelCount;
    if (CounterArrayOffset != 0) {
        Timer->CurrentCounts = (PVOID)Timer + CounterArrayOffset;
    }

    if (AbsoluteArrayOffset != 0) {
        Timer->AbsoluteData = (PVOID)Timer + AbsoluteArrayOffset;
    }

    //
    // Insert the timer on the list.
    //

    INSERT_BEFORE(&(Timer->ListEntry), &HlTimers);
    Status = STATUS_SUCCESS;

TimerRegisterHardwareEnd:
    return Status;
}

KSTATUS
HlpTimerArm (
    PHARDWARE_TIMER Timer,
    TIMER_MODE Mode,
    ULONGLONG TickCount
    )

/*++

Routine Description:

    This routine arms a timer to fire an interrupt after the given interval.

Arguments:

    Timer - Supplies a pointer to the timer to arm.

    Mode - Supplies whether or not this should be a recurring timer or not.

    TickCount - Supplies the number of timer ticks from now for the timer to
        fire in. In absolute mode, this supplies the time in timer ticks at
        which to fire an interrupt.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NOT_SUPPORTED if the timer cannot support the requested mode.

    Other status codes on failure.

--*/

{

    PHARDWARE_TIMER_ABSOLUTE_DATA AbsoluteData;
    ULONG AbsoluteIndex;
    ULONGLONG CurrentTime;
    ULONGLONG DueTime;
    ULONGLONG HalfRolloverTicks;
    RUNLEVEL OldRunLevel;
    PVOID PrivateContext;
    KSTATUS Status;

    //
    // Non-periodic, absolute timers require a little extra massaging in order
    // to support periodic requests - they need to be converted to absolute
    // requests.
    //

    OldRunLevel = RunLevelCount;
    if (((Timer->Features & TIMER_FEATURE_ABSOLUTE) != 0) &&
        ((Timer->Features & TIMER_FEATURE_PERIODIC) == 0)) {

        ASSERT((Timer->Features & TIMER_FEATURE_PER_PROCESSOR) != 0);
        ASSERT(Timer->InterruptRunLevel != RunLevelCount);

        //
        // Raise to the interrupt's run level in order to synchronize rearming
        // the timer during acknowledge interrupt with this new call to arm the
        // timer.
        //

        AbsoluteIndex = KeGetCurrentProcessorNumber();
        AbsoluteData = &(Timer->AbsoluteData[AbsoluteIndex]);
        OldRunLevel = KeRaiseRunLevel(Timer->InterruptRunLevel);
        switch (Mode) {

        //
        // Periodic mode is faked for non-periodic absolute timers, by having
        // acknowledge interrupt rearm the timer. Convert the tick count to an
        // absolute deadline and save the period so that the timer can be
        // rearmed during interrupt handling.
        //

        case TimerModePeriodic:

            //
            // Make sure the tick count is less than half the rollover rate.
            // Absolute timers need to be able to differentiate between a time
            // shortly in the past and a time in the far future.
            //

            HalfRolloverTicks = 1ULL << (Timer->CounterBitWidth - 1);
            if (TickCount > HalfRolloverTicks) {
                TickCount = HalfRolloverTicks;
            }

            //
            // Calculate the absolute due time, only sending the hardware bits
            // it knows about.
            //

            PrivateContext = Timer->PrivateContext;
            CurrentTime = Timer->FunctionTable.ReadCounter(PrivateContext);
            DueTime = CurrentTime + TickCount;
            DueTime &= (1ULL << Timer->CounterBitWidth) - 1;

            //
            // Save the periodic state so acknowledge interrupt can rearm the
            // timer.
            //

            AbsoluteData->Period = TickCount;
            AbsoluteData->DueTime = DueTime;

            //
            // Convert from periodic mode to absolute mode.
            //

            TickCount = DueTime;
            Mode = TimerModeAbsolute;
            break;

        //
        // For both one-shot and absolute modes, set the period to 0 so that
        // acknowledge interrupt does not rearm the timer.
        //

        case TimerModeOneShot:
            if ((Timer->Features & TIMER_FEATURE_ONE_SHOT) == 0) {
                Status = STATUS_NOT_SUPPORTED;
                goto TimerArmEnd;
            }

            //
            // Fall through.
            //

        case TimerModeAbsolute:
            AbsoluteData->Period = 0;
            break;

        default:
            Status = STATUS_INVALID_PARAMETER;
            goto TimerArmEnd;
        }

    //
    // Otherwise, all periodic requests take relative tick counts and there is
    // no extra work to do.
    //

    } else {
        switch (Mode) {
        case TimerModePeriodic:
            if ((Timer->Features & TIMER_FEATURE_PERIODIC) == 0) {
                Status = STATUS_NOT_SUPPORTED;
                goto TimerArmEnd;
            }

            break;

        case TimerModeOneShot:
            if ((Timer->Features & TIMER_FEATURE_ONE_SHOT) == 0) {
                Status = STATUS_NOT_SUPPORTED;
                goto TimerArmEnd;
            }

            break;

        case TimerModeAbsolute:
            if ((Timer->Features & TIMER_FEATURE_ABSOLUTE) == 0) {
                Status = STATUS_NOT_SUPPORTED;
                goto TimerArmEnd;
            }

            break;

        default:
            Status = STATUS_INVALID_PARAMETER;
            goto TimerArmEnd;
        }
    }

    //
    // Arm the timer to begin counting.
    //

    Status = Timer->FunctionTable.Arm(Timer->PrivateContext, Mode, TickCount);
    if (!KSUCCESS(Status)) {
        goto TimerArmEnd;
    }

TimerArmEnd:

    ASSERT(KSUCCESS(Status));

    if (OldRunLevel != RunLevelCount) {
        KeLowerRunLevel(OldRunLevel);
    }

    return Status;
}

VOID
HlpTimerDisarm (
    PHARDWARE_TIMER Timer
    )

/*++

Routine Description:

    This routine disarms a timer, stopping it from firing interrupts.

Arguments:

    Timer - Supplies a pointer to the timer to disarm.

Return Value:

    None.

--*/

{

    PHARDWARE_TIMER_ABSOLUTE_DATA AbsoluteData;
    ULONG AbsoluteIndex;
    RUNLEVEL OldRunLevel;

    ASSERT(Timer != NULL);

    //
    // If this is a non-periodic absolute timer, raise to the interrupt's run
    // level and set the period to 0 so that acknowledge interrupt does not
    // rearm the timer.
    //

    OldRunLevel = RunLevelCount;
    if (((Timer->Features & TIMER_FEATURE_ABSOLUTE) != 0) &&
        ((Timer->Features & TIMER_FEATURE_PERIODIC) == 0)) {

        ASSERT((Timer->Features & TIMER_FEATURE_PER_PROCESSOR) != 0);
        ASSERT(Timer->InterruptRunLevel != RunLevelCount);

        AbsoluteIndex = KeGetCurrentProcessorNumber();
        AbsoluteData = &(Timer->AbsoluteData[AbsoluteIndex]);
        OldRunLevel = KeRaiseRunLevel(Timer->InterruptRunLevel);
        AbsoluteData->Period = 0;
    }

    //
    // Disarm the timer.
    //

    Timer->FunctionTable.Disarm(Timer->PrivateContext);
    if (OldRunLevel != RunLevelCount) {
        KeLowerRunLevel(OldRunLevel);
    }

    return;
}

VOID
HlpTimerAcknowledgeInterrupt (
    PHARDWARE_TIMER Timer
    )

/*++

Routine Description:

    This routine acknowledges a timer interrupt. If the timer is a non-periodic
    absolute timer, this will rearm the timer if it is still in periodic mode.

Arguments:

    Timer - Supplies a pointer to the timer whose interrupt is to be
        acknowledged.

Return Value:

    None.

--*/

{

    PHARDWARE_TIMER_ABSOLUTE_DATA AbsoluteData;
    ULONG AbsoluteIndex;
    PTIMER_ACKNOWLEDGE_INTERRUPT AcknowledgeInterrupt;
    PVOID Context;
    ULONGLONG CurrentTime;
    ULONGLONG DueTime;
    ULONGLONG ExtendedCurrentTime;
    ULONGLONG ExtendedDueTime;
    ULONGLONG Period;

    AcknowledgeInterrupt = Timer->FunctionTable.AcknowledgeInterrupt;
    if (AcknowledgeInterrupt != NULL) {
        AcknowledgeInterrupt(Timer->PrivateContext);
    }

    //
    // If it's a non-periodic absolute timer, then attempt to rearm it.
    //

    if (((Timer->Features & TIMER_FEATURE_ABSOLUTE) != 0) &&
        ((Timer->Features & TIMER_FEATURE_PERIODIC) == 0)) {

        ASSERT((Timer->Features & TIMER_FEATURE_PER_PROCESSOR) != 0);
        ASSERT(KeGetRunLevel() == Timer->InterruptRunLevel);

        AbsoluteIndex = KeGetCurrentProcessorNumber();
        AbsoluteData = &(Timer->AbsoluteData[AbsoluteIndex]);
        Period = AbsoluteData->Period;
        if (Period != 0) {
            Context = Timer->PrivateContext;
            DueTime = AbsoluteData->DueTime + Period;
            DueTime &= (1ULL << Timer->CounterBitWidth) - 1;
            CurrentTime = Timer->FunctionTable.ReadCounter(Context);

            //
            // If the current time is already ahead of the calculated due time,
            // then this timer got behind (likely due to a debug break). Catch
            // it up by programming a time in the future.
            //

            ExtendedDueTime = DueTime << (64 - Timer->CounterBitWidth);
            ExtendedCurrentTime = CurrentTime << (64 - Timer->CounterBitWidth);
            if ((LONGLONG)(ExtendedDueTime - ExtendedCurrentTime) < 0) {
                DueTime = CurrentTime + Period;
            }

            AbsoluteData->DueTime = DueTime;
            Timer->FunctionTable.Arm(Context, TimerModeAbsolute, DueTime);
        }
    }

    return;
}

ULONGLONG
HlpTimerExtendedQuery (
    PHARDWARE_TIMER Timer
    )

/*++

Routine Description:

    This routine returns a 64-bit monotonically non-decreasing value based on
    the given timer. For this routine to ensure the non-decreasing part, this
    routine must be called at more than twice the rollover rate. The inner
    workings of this routine rely on observing the top bit of the hardware
    timer each time it changes.

Arguments:

    Timer - Supplies the timer to query.

Return Value:

    Returns the timers count, with software rollovers extended out to 64-bits
    if needed.

--*/

{

    ULONGLONG Count1;
    ULONGLONG Count2;
    volatile ULONGLONG *CurrentCountPointer;
    ULONGLONG HardwareMask;
    ULONGLONG HardwareValue;
    ULONGLONG MostSignificantHardwareBit;
    ULONGLONG NewCount;
    PTIMER_READ_COUNTER ReadCounter;
    ULONGLONG SoftwareOffset1;
    ULONGLONG SoftwareOffset2;

    ReadCounter = Timer->FunctionTable.ReadCounter;

    ASSERT(ReadCounter != NULL);

    if (Timer->CurrentCounts == NULL) {
        CurrentCountPointer = &(Timer->CurrentCount);

    } else {
        CurrentCountPointer =
                        &(Timer->CurrentCounts[KeGetCurrentProcessorNumber()]);
    }

    //
    // Get a consistent read between the hardware counter and current count
    // with respect to the software offset. The current count needs to be read
    // twice to avoid a torn read that could result from a race with the
    // extended read's atomic update from either another core or an interrupt
    // arriving on top of this loop.
    //

    do {
        READ_INT64_SYNC(&(Timer->SoftwareOffset), &SoftwareOffset1);
        Count1 = *CurrentCountPointer;
        HardwareValue = ReadCounter(Timer->PrivateContext);
        READ_INT64_SYNC(&(Timer->SoftwareOffset), &SoftwareOffset2);
        Count2 = *CurrentCountPointer;

    } while ((SoftwareOffset1 != SoftwareOffset2) || (Count1 != Count2));

    //
    // For 64-bit timers, just return the raw timer, no software rollover
    // accounting is needed.
    //

    if (Timer->CounterBitWidth >= 64) {
        return HardwareValue + SoftwareOffset1;
    }

    MostSignificantHardwareBit = 1ULL << (Timer->CounterBitWidth - 1);
    HardwareMask = (1ULL << Timer->CounterBitWidth) - 1;

    //
    // The new count is the old count, but with the hardware timer replacing
    // the lower bits.
    //

    NewCount = (Count1 & ~HardwareMask) | HardwareValue;

    //
    // If the most significant bit has flipped, the new count will need to
    // be written back into the global.
    //

    if (((NewCount ^ Count1) & MostSignificantHardwareBit) != 0) {

        //
        // Add a rollover if the transition was from a 1 to a 0.
        //

        if ((NewCount & MostSignificantHardwareBit) == 0) {
            NewCount += HardwareMask + 1;
        }

        //
        // Compare exchange the new count with the global. If the compare
        // exchange was won, then the value is nicely updated. If the compare
        // exchange was lost, then someone else must have done the update, and
        // no further action is needed (as both parties will have added the
        // rollover independently if necessary).
        //

        RtlAtomicCompareExchange64(CurrentCountPointer, NewCount, Count1);
    }

    return NewCount + SoftwareOffset1;
}

ULONGLONG
HlpTimerTimeToTicks (
    PHARDWARE_TIMER Timer,
    ULONGLONG TimeIn100ns
    )

/*++

Routine Description:

    This routine returns the tick count that best approximates the desired
    time interval on the given timer.

Arguments:

    Timer - Supplies a pointer to the timer on which this interval will be
        run.

    TimeIn100ns - Supplies the desired interval to fire in, with units of
        100 nanoseconds (10^-7 seconds).

Return Value:

    Returns the tick count that most closely approximates the requested tick
    count.

    0 on failure.

--*/

{

    ULONGLONG HalfRolloverTicks;
    ULONGLONG MaxTimerValue;
    ULONGLONG TickCount;

    //
    // The tick count is the frequency (ticks / s) * Time (hns) /
    // 10^7 (s / hns). All units cancel but ticks.
    //

    TickCount = Timer->CounterFrequency * TimeIn100ns / 10000000ULL;
    if (TickCount == 0) {
        TickCount = 1;
    }

    //
    // If the requested tick count is more than the timer can handle, return
    // the maximum value the timer can handle.
    //

    if (Timer->CounterBitWidth < 64) {
        MaxTimerValue = (1ULL << Timer->CounterBitWidth) - 1;
        if (TickCount > MaxTimerValue) {
            TickCount = MaxTimerValue;
        }
    }

    //
    // If this is a non-periodic absolute timer, then the tick count needs to
    // be truncated to half the rollover rate. Otherwise it is difficult for
    // the timer to detect if an absolute time is shortly in the past or
    // far in the future.
    //

    if (((Timer->Features & TIMER_FEATURE_ABSOLUTE) != 0) &&
        ((Timer->Features & TIMER_FEATURE_PERIODIC) == 0)) {

        HalfRolloverTicks = 1ULL << (Timer->CounterBitWidth - 1);
        if (TickCount > HalfRolloverTicks) {
            TickCount = HalfRolloverTicks;
        }
    }

    return TickCount;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
HlpTimerInitialize (
    PHARDWARE_TIMER Timer
    )

/*++

Routine Description:

    This routine initializes or reinitializes a hardware timer.

Arguments:

    Timer - Supplies a pointer to the timer to initialize or
        reinitialize.

Return Value:

    Status code.

--*/

{

    PTIMER_INITIALIZE Initialize;
    KSTATUS Status;

    //
    // Reinitialize the timer.
    //

    Initialize = Timer->FunctionTable.Initialize;
    Status = Initialize(Timer->PrivateContext);
    if (!KSUCCESS(Status)) {
        goto TimerInitializeEnd;
    }

TimerInitializeEnd:
    if (KSUCCESS(Status)) {
        Timer->Flags &= ~TIMER_FLAG_FAILED;
        Timer->Flags |= TIMER_FLAG_INITIALIZED;

    } else {
        Timer->Flags &= ~TIMER_FLAG_INITIALIZED;
        Timer->Flags |= TIMER_FLAG_FAILED;
    }

    return Status;
}

KSTATUS
HlpTimerMeasureUnknownFrequencies (
    VOID
    )

/*++

Routine Description:

    This routine measures the timers whose frequencies are not currently known.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PULONGLONG EndTimes;
    PHARDWARE_TIMER MeasuringTimer;
    PULONGLONG StartTimes;
    KSTATUS Status;
    PHARDWARE_TIMER Timer;
    ULONG TimerCount;
    ULONG TimerIndex;

    StartTimes = NULL;

    //
    // Find and initialize the timer that should be used to measure all other
    // timers.
    //

    while (TRUE) {
        MeasuringTimer = HlpTimerFindIdealMeasuringSource();
        if (MeasuringTimer == NULL) {
            Status = STATUS_NO_ELIGIBLE_DEVICES;
            goto TimerMeasureUnknownFrequenciesEnd;
        }

        Status = HlpTimerInitialize(MeasuringTimer);
        if (!KSUCCESS(Status)) {
            continue;
        }

        break;
    }

    //
    // Scan through all timers, and initialize the ones that need to be fired up
    // to be measured.
    //

    TimerCount = 0;
    CurrentEntry = HlTimers.Next;
    while (CurrentEntry != &HlTimers) {
        Timer = LIST_VALUE(CurrentEntry, HARDWARE_TIMER, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if (Timer->CounterFrequency == 0) {
            Status = HlpTimerInitialize(Timer);
            if (KSUCCESS(Status)) {
                TimerCount += 1;
            }
        }
    }

    //
    // If there's nothing to measure, don't even worry about it.
    //

    if (TimerCount == 0) {
        Status = STATUS_SUCCESS;
        goto TimerMeasureUnknownFrequenciesEnd;
    }

    //
    // Allocate space to store the start and end times of all the timers.
    //

    StartTimes = MmAllocateNonPagedPool(TimerCount * sizeof(ULONGLONG) * 2,
                                        HL_POOL_TAG);

    if (StartTimes == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto TimerMeasureUnknownFrequenciesEnd;
    }

    RtlZeroMemory(StartTimes, TimerCount * sizeof(ULONGLONG) * 2);
    EndTimes = StartTimes + TimerCount;

    //
    // Perform a "warm-up" read of all timers. This read clears the pipes,
    // warming up caches and flushing out any hardware quirks associated with
    // the "first" read.
    //

    TimerIndex = 0;
    CurrentEntry = HlTimers.Next;
    while (CurrentEntry != &HlTimers) {
        Timer = LIST_VALUE(CurrentEntry, HARDWARE_TIMER, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if (((Timer->Flags & TIMER_FLAG_INITIALIZED) != 0) &&
            (Timer->CounterFrequency == 0)) {

            StartTimes[TimerIndex] = HlpTimerExtendedQuery(Timer);
            TimerIndex += 1;
        }
    }

    //
    // Do a warm-up read of the measuring source and serializing instruction
    // as well.
    //

    HlpTimerExtendedQuery(MeasuringTimer);
    ArSerializeExecution();

    //
    // Mark the beginning time for each timer.
    //

    TimerIndex = 0;
    CurrentEntry = HlTimers.Next;
    while (CurrentEntry != &HlTimers) {
        Timer = LIST_VALUE(CurrentEntry, HARDWARE_TIMER, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if (((Timer->Flags & TIMER_FLAG_INITIALIZED) != 0) &&
            (Timer->CounterFrequency == 0)) {

            StartTimes[TimerIndex] = HlpTimerExtendedQuery(Timer);
            TimerIndex += 1;
        }
    }

    //
    // Serialize to ensure that all reads have actually occurred.
    //

    ArSerializeExecution();

    //
    // Stall for a quarter of a second against the reference timer.
    //

    HlpTimerBusyStall(MeasuringTimer, REFERENCE_STALL_DURATION);

    //
    // Serialize again to ensure the stall completed.
    //

    ArSerializeExecution();

    //
    // Take the end marking reads.
    //

    TimerIndex = 0;
    CurrentEntry = HlTimers.Next;
    while (CurrentEntry != &HlTimers) {
        Timer = LIST_VALUE(CurrentEntry, HARDWARE_TIMER, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if (((Timer->Flags & TIMER_FLAG_INITIALIZED) != 0) &&
            (Timer->CounterFrequency == 0)) {

            EndTimes[TimerIndex] = HlpTimerExtendedQuery(Timer);
            TimerIndex += 1;
        }
    }

    //
    // Whew, the time sensitive part is over. Now calculate the frequencies of
    // all these bad boys.
    //

    TimerIndex = 0;
    CurrentEntry = HlTimers.Next;
    while (CurrentEntry != &HlTimers) {
        Timer = LIST_VALUE(CurrentEntry, HARDWARE_TIMER, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if (((Timer->Flags & TIMER_FLAG_INITIALIZED) != 0) &&
            (Timer->CounterFrequency == 0)) {

            //
            // Frequency is ticks per second. So take the number of ticks and
            // divide by the number of seconds.
            //

            Timer->CounterFrequency =
                            (EndTimes[TimerIndex] - StartTimes[TimerIndex]) *
                            (1000000 / REFERENCE_STALL_DURATION);

            if (Timer->CounterFrequency == 0) {
                Timer->Flags |= TIMER_FLAG_FAILED | TIMER_FLAG_NOT_TICKING;
            }

            TimerIndex += 1;
        }
    }

    Status = STATUS_SUCCESS;

TimerMeasureUnknownFrequenciesEnd:
    if (StartTimes != NULL) {
        MmFreeNonPagedPool(StartTimes);
    }

    return Status;
}

PHARDWARE_TIMER
HlpTimerFindIdealMeasuringSource (
    VOID
    )

/*++

Routine Description:

    This routine attempts to find a timer suitable for measuring all other
    timers.

Arguments:

    None.

Return Value:

    Returns a pointer to the timer on success.

    NULL if no suitable timers could be found.

--*/

{

    PHARDWARE_TIMER BestTimer;
    PLIST_ENTRY CurrentEntry;
    PHARDWARE_TIMER Timer;

    BestTimer = NULL;
    CurrentEntry = HlTimers.Next;
    while (CurrentEntry != &HlTimers) {
        Timer = LIST_VALUE(CurrentEntry, HARDWARE_TIMER, ListEntry);
        CurrentEntry = CurrentEntry->Next;

        //
        // Look for a readable timer with the fastest frequency.
        //

        if ((Timer->Features & TIMER_FEATURE_READABLE) == 0) {
            continue;
        }

        if (Timer->CounterFrequency == 0) {
            continue;
        }

        if ((Timer->Flags & TIMER_FLAG_FAILED) != 0) {
            continue;
        }

        if ((BestTimer == NULL) ||
            (Timer->CounterFrequency > BestTimer->CounterFrequency)) {

            BestTimer = Timer;
        }
    }

    return BestTimer;
}

PHARDWARE_TIMER
HlpTimerFindIdealClockSource (
    VOID
    )

/*++

Routine Description:

    This routine attempts to find a timer suitable for running the periodic
    system clock interrupt.

Arguments:

    None.

Return Value:

    Returns a pointer to the timer on success.

    NULL if no suitable timers could be found.

--*/

{

    ULONG RequiredFeatures;
    PHARDWARE_TIMER Timer;

    RequiredFeatures = TIMER_FEATURE_PERIODIC | TIMER_FEATURE_PER_PROCESSOR;
    Timer = HlpTimerFind(RequiredFeatures, 0, 0);
    if (Timer != NULL) {
        return Timer;
    }

    RequiredFeatures = TIMER_FEATURE_ABSOLUTE | TIMER_FEATURE_PER_PROCESSOR;
    Timer = HlpTimerFind(RequiredFeatures, 0, 0);
    if (Timer != NULL) {
        return Timer;
    }

    Timer = HlpTimerFind(TIMER_FEATURE_PERIODIC, 0, 0);
    if (Timer != NULL) {
        return Timer;
    }

    return Timer;
}

PHARDWARE_TIMER
HlpTimerFindIdealProfilerSource (
    VOID
    )

/*++

Routine Description:

    This routine attempts to find a timer suitable for running the periodic
    system profiler.

Arguments:

    None.

Return Value:

    Returns a pointer to the timer on success.

    NULL if no suitable timers could be found.

--*/

{

    ULONG RequiredNonFeatures;
    PHARDWARE_TIMER Timer;

    //
    // Attempt to find a periodic non-variable timer. It should not be
    // per-processor.
    //

    RequiredNonFeatures = TIMER_FEATURE_PER_PROCESSOR | TIMER_FEATURE_VARIANT;
    Timer = HlpTimerFind(TIMER_FEATURE_PERIODIC, RequiredNonFeatures, 0);
    return Timer;
}

PHARDWARE_TIMER
HlpTimerFindIdealTimeCounter (
    VOID
    )

/*++

Routine Description:

    This routine attempts to find a timer suitable for running the system's
    concept of time.

Arguments:

    None.

Return Value:

    Returns a pointer to the timer on success.

    NULL if no suitable timers could be found.

--*/

{

    ULONG Options;
    ULONG RequiredFeatures;
    PHARDWARE_TIMER Timer;

    //
    // Attempt to find a per-processor timer for fastest access.
    //

    Options = FIND_TIMER_OPTION_INCLUDE_USED_FOR_INTERRUPT_ABSOLUTE;
    RequiredFeatures = TIMER_FEATURE_PER_PROCESSOR | TIMER_FEATURE_READABLE;
    Timer = HlpTimerFind(RequiredFeatures, TIMER_FEATURE_VARIANT, Options);
    if (Timer != NULL) {
        return Timer;
    }

    //
    // Attempt to find any readable timer.
    //

    RequiredFeatures = TIMER_FEATURE_READABLE;
    Timer = HlpTimerFind(RequiredFeatures, TIMER_FEATURE_VARIANT, Options);
    if (Timer != NULL) {
        return Timer;
    }

    return Timer;
}

PHARDWARE_TIMER
HlpTimerFindIdealProcessorCounter (
    VOID
    )

/*++

Routine Description:

    This routine attempts to find a timer suitable for running the system's
    concept of processor time, which may not line up well to wall clock time.
    Performance is key here, as this counter is queried frequently by the
    scheduler.

Arguments:

    None.

Return Value:

    Returns a pointer to the timer on success.

    NULL if no suitable timers could be found.

--*/

{

    ULONG Options;
    PHARDWARE_TIMER Timer;

    //
    // Attempt to find the processor counter.
    //

    Options = FIND_TIMER_OPTION_INCLUDE_USED_FOR_COUNTER |
              FIND_TIMER_OPTION_INCLUDE_USED_FOR_INTERRUPT_ABSOLUTE;

    Timer = HlpTimerFind(TIMER_FEATURE_PROCESSOR_COUNTER, 0, Options);
    if (Timer != NULL) {
        return Timer;
    }

    //
    // If for some reason there is no processor cycle counter, use the
    // time counter.
    //

    return HlTimeCounter;
}

PHARDWARE_TIMER
HlpTimerFind (
    ULONG RequiredFeatures,
    ULONG RequiredNonFeatures,
    ULONG FindOptions
    )

/*++

Routine Description:

    This routine attempts to find a timer matching the given characteristics.

Arguments:

    RequiredFeatures - Supplies a bitfield of the features that the hardware
        timer must implement.

    RequiredNonFeatures - Supplies a bitfield of the features that the hardware
        timer must NOT have implemented.

    FindOptions - Supplies a bitfield of options governing additional parameters
        of the search. See FIND_TIMER_OPTION_* definitions.

Return Value:

    Returns a pointer to a timer matching the given criteria on success.

    NULL if no timer matching the requested criteria could be found.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PHARDWARE_TIMER Timer;
    BOOL TimerFound;

    Timer = NULL;
    TimerFound = FALSE;
    CurrentEntry = HlTimers.Next;
    while (CurrentEntry != &HlTimers) {
        Timer = LIST_VALUE(CurrentEntry, HARDWARE_TIMER, ListEntry);
        CurrentEntry = CurrentEntry->Next;

        //
        // Skip this timer if it does not have the required features set.
        //

        if ((Timer->Features & RequiredFeatures) != RequiredFeatures) {
            continue;
        }

        //
        // Skip this timer if it has any of the non-features set.
        //

        if ((Timer->Features & RequiredNonFeatures) != 0) {
            continue;
        }

        //
        // Unless the include used option is set, skip this timer if it's
        // already marked as in use.
        //

        if (((Timer->Flags & TIMER_FLAG_IN_USE_FOR_INTERRUPT) != 0) &&
            ((FindOptions &
              FIND_TIMER_OPTION_INCLUDE_USED_FOR_INTERRUPT_ANY) == 0)) {

            if ((FindOptions &
                 FIND_TIMER_OPTION_INCLUDE_USED_FOR_INTERRUPT_ABSOLUTE) == 0) {

                continue;
            }

            if ((Timer->Features & TIMER_FEATURE_ABSOLUTE) == 0) {
                continue;
            }
        }

        if ((FindOptions & FIND_TIMER_OPTION_INCLUDE_USED_FOR_COUNTER) == 0) {
            if ((Timer->Flags & TIMER_FLAG_IN_USE_FOR_COUNTER) != 0) {
                continue;
            }
        }

        //
        // Skip this timer if it has failed initialization.
        //

        if ((Timer->Flags & TIMER_FLAG_FAILED) != 0) {
            continue;
        }

        //
        // The timer doesn't not match all the required criteria... so return
        // it.
        //

        TimerFound = TRUE;
        break;
    }

    if (TimerFound != FALSE) {
        return Timer;
    }

    return NULL;
}

VOID
HlpTimerBusyStall (
    PHARDWARE_TIMER Timer,
    ULONG Microseconds
    )

/*++

Routine Description:

    This routine executes for at least the given number of microseconds by
    continually reading the given timer until the specified duration has passed
    (as measured by that timer).

Arguments:

    Timer - Supplies the timer to use as the reference for stalling.

    Microseconds - Supplies the number of microseconds to stall for.

Return Value:

    None.

--*/

{

    ULONGLONG EndCount;
    ULONG TickCount;

    TickCount = (ULONGLONG)Microseconds * Timer->CounterFrequency / 1000000ULL;

    //
    // The end count is a read of the timer plus the number of ticks to stall
    // for.
    //

    EndCount = HlpTimerExtendedQuery(Timer) + TickCount;

    //
    // Loop until the timer's count exceeds the end time.
    //

    while (HlpTimerExtendedQuery(Timer) < EndCount) {
        ArProcessorYield();
    }

    return;
}

KSTATUS
HlpTimerAssignRoles (
    VOID
    )

/*++

Routine Description:

    This routine surveys the timers registered across the system and assigns
    them to the various required system services.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;
    PHARDWARE_TIMER Timer;

    //
    // Assign the clock role.
    //

    while (TRUE) {
        Timer = HlpTimerFindIdealClockSource();
        if (Timer == NULL) {
            Status = STATUS_NO_ELIGIBLE_DEVICES;
            goto TimerAssignRolesEnd;
        }

        Status = HlpTimerInitialize(Timer);
        if (KSUCCESS(Status)) {
            HlClockTimer = Timer;
            Timer->Flags |= TIMER_FLAG_IN_USE_FOR_INTERRUPT;
            break;
        }
    }

    //
    // Assign the time counter role.
    //

    while (TRUE) {
        Timer = HlpTimerFindIdealTimeCounter();
        if (Timer == NULL) {
            Status = STATUS_NO_ELIGIBLE_DEVICES;
            goto TimerAssignRolesEnd;
        }

        Status = HlpTimerInitialize(Timer);
        if (KSUCCESS(Status)) {
            HlTimeCounter = Timer;
            Timer->Flags |= TIMER_FLAG_IN_USE_FOR_COUNTER;
            break;
        }
    }

    //
    // Assign the processor counter role.
    //

    while (TRUE) {
        Timer = HlpTimerFindIdealProcessorCounter();
        if (Timer == NULL) {
            Status = STATUS_NO_ELIGIBLE_DEVICES;
            goto TimerAssignRolesEnd;
        }

        if ((Timer->Flags & TIMER_FLAG_INITIALIZED) == 0) {
            Status = HlpTimerInitialize(Timer);

        } else {
            Status = STATUS_SUCCESS;
        }

        if (KSUCCESS(Status)) {
            HlProcessorCounter = Timer;
            Timer->Flags |= TIMER_FLAG_IN_USE_FOR_COUNTER;
            break;
        }
    }

    //
    // Assign the profiler role. If there are no available timers, still
    // succeeded. The system just will not be able to be profiled.
    //

    while (TRUE) {
        Timer = HlpTimerFindIdealProfilerSource();
        if (Timer == NULL) {

            ASSERT(HlProfilerTimer == NULL);

            Status = STATUS_SUCCESS;
            goto TimerAssignRolesEnd;
        }

        Status = HlpTimerInitialize(Timer);
        if (KSUCCESS(Status)) {
            HlProfilerTimer = Timer;
            Timer->Flags |= TIMER_FLAG_IN_USE_FOR_INTERRUPT;
            break;
        }
    }

    Status = STATUS_SUCCESS;

TimerAssignRolesEnd:
    return Status;
}

VOID
HlpTimerResetCounterOffset (
    PHARDWARE_TIMER Timer,
    ULONGLONG NewValue
    )

/*++

Routine Description:

    This routine resets the software offset of the given timer to make it
    appear as if the counter is starting from the given value.

Arguments:

    Timer - Supplies a pointer to the timer to reset.

    NewValue - Supplies a pointer to the new value to make extended reads
        return.

Return Value:

    None.

--*/

{

    ULONGLONG Counter;
    ULONGLONG NewOffset;

    ASSERT((Timer->Features & TIMER_FEATURE_READABLE) != 0);

    Counter = Timer->FunctionTable.ReadCounter(Timer->PrivateContext);

    //
    // Extended reads are Counter + Offset = Value. Flip the equation around and
    // it becomes Offset = Value - Counter.
    //

    NewOffset = NewValue - Counter;
    WRITE_INT64_SYNC(&(Timer->SoftwareOffset), NewOffset);
    return;
}

KSTATUS
HlpTimerCreateSoftUpdateTimer (
    PHARDWARE_TIMER Timer
    )

/*++

Routine Description:

    This routine creates a software timer that fires a little more frequently
    than half the timer rollover rate. This is used to ensure that each
    significant bit flip of the timer is observed. If the timer's counter is
    64-bits in length, then no timer is created.

Arguments:

    Timer - Supplies a pointer to the timer to create the software timer for.

Return Value:

    Status code.

--*/

{

    ULONGLONG HalfRolloverSeconds;
    ULONGLONG Microseconds;
    PKTIMER SoftTimer;
    KSTATUS Status;
    ULONG Ticks;

    //
    // Do nothing if the timer is 64-bits (and so there's no rollover to keep
    // track of).
    //

    if (Timer->CounterBitWidth == 64) {
        return STATUS_SUCCESS;
    }

    //
    // Compute the half rollover rate in seconds. If it's huge, then also don't
    // bother with a software timer.
    //

    Ticks = 1ULL << (Timer->CounterBitWidth - 1);
    HalfRolloverSeconds = Ticks / Timer->CounterFrequency;
    if (HalfRolloverSeconds > (SECONDS_PER_DAY * 90)) {
        return STATUS_SUCCESS;
    }

    //
    // Figure out the microseconds per half-rollover, and then take about 80
    // percent of that for safety.
    //

    Microseconds = (Ticks * MICROSECONDS_PER_SECOND) / Timer->CounterFrequency;
    Microseconds = (Microseconds * 820) / 1024;

    //
    // Create the timer, which itself is leaked. If this function is needed by
    // timers other than the time counter, then 1) the timer should be saved
    // in the hardware timer structure so that it can be shut off if the timer
    // is no longer used, and 2) the timer should fire a DPC that actually
    // queries the counter (rather than assuming the clock interrupt will do it,
    // which is true only for the time counter).
    //

    ASSERT(Timer == HlTimeCounter);

    SoftTimer = KeCreateTimer(HL_POOL_TAG);
    if (SoftTimer == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Now convert those microseconds to time counter ticks and queue the timer.
    //

    Ticks = KeConvertMicrosecondsToTimeTicks(Microseconds);
    Status = KeQueueTimer(SoftTimer, TimerQueueSoftWake, 0, Ticks, 0, NULL);
    return Status;
}

