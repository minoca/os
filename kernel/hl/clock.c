/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    clock.c

Abstract:

    This module implements clock interrupt support at the hardware level.

Author:

    Evan Green 19-Aug-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "clock.h"
#include "timer.h"
#include "intrupt.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the number of seconds the system will wait for the first clock
// interrupt to come in.
//

#define CLOCK_START_GRACE_PERIOD_SECONDS 5

//
// ----------------------------------------------- Internal Function Prototypes
//

INTERRUPT_STATUS
HlpClockInterruptHandler (
    PVOID Context
    );

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _CLOCK_REQUEST {
    CLOCK_TIMER_MODE Mode;
    ULONGLONG DueTime;
    BOOL Hard;
} CLOCK_REQUEST, *PCLOCK_REQUEST;

//
// -------------------------------------------------------------------- Globals
//

volatile ULONG HlEarlyClockInterruptCount = 0;

//
// Store the current periodic clock rate, in ticks of the clock timer.
//

ULONGLONG HlClockRate;

//
// Also store the rate in time counter ticks.
//

ULONGLONG HlClockRateInTimeCounterTicks;

//
// Store a variable indicating whether clock interrupts are broadcast.
//

BOOL HlBroadcastClockInterrupts = FALSE;

//
// Store variables used to multiplex clock data down to a single timer.
//

BOOL HlClockAnyHardDeadlines;
BOOL HlClockAnyPeriodic = TRUE;
ULONGLONG HlClockNextHardDeadline = -1ULL;
KSPIN_LOCK HlClockDataLock;
TIMER_MODE HlClockMode;
ULONGLONG HlClockLastProgrammedValue;
ULONGLONG HlClockLastDueTime = -1ULL;
PCLOCK_REQUEST HlClockRequests;

//
// Set this to TRUE to debug clock changes.
//

BOOL HlDebugClockChanges = FALSE;

PSTR HlClockTimerModeStrings[] = {
    NULL,
    "Periodic",
    "OneShot",
    "Off"
};

//
// ------------------------------------------------------------------ Functions
//

VOID
HlSetClockTimer (
    CLOCK_TIMER_MODE Mode,
    ULONGLONG DueTime,
    BOOL Hard
    )

/*++

Routine Description:

    This routine arms or disarms the main clock timer. This routine must be
    called at or above clock level, or with interrupts disabled.

Arguments:

    Mode - Supplies the mode to arm the timer in.

    DueTime - Supplies the due time in time counter ticks (absolute) to arm the
        timer in. This is only used in one-shot mode.

    Hard - Supplies a boolean indicating if this is a hard or soft deadline.
        This is used only for one-shot mode.

Return Value:

    None.

--*/

{

    BOOL AllOff;
    BOOL AnyHard;
    BOOL AnyPeriodic;
    BOOL ArmTimer;
    ULONGLONG ClockTicks;
    ULONGLONG CurrentTime;
    ULONG Index;
    ULONGLONG NextHardDeadline;
    ULONGLONG NextPeriodic;
    ULONGLONG NextSoftDeadline;
    RUNLEVEL OldRunLevel;
    ULONG ProcessorCount;
    ULONGLONG ProcessorDueTime;
    ULONG ProcessorNumber;
    TIMER_MODE SupportedTimerMode;
    TIMER_MODE TimerMode;

    ASSERT(KeGetRunLevel() >= RunLevelClock);

    ProcessorNumber = KeGetCurrentProcessorNumber();
    if (HlDebugClockChanges != FALSE) {
        RtlDebugPrint("P%d: %s 0x%I64x (0x%I64x)\n",
                      ProcessorNumber,
                      HlClockTimerModeStrings[Mode],
                      DueTime,
                      HlQueryTimeCounter());
    }

    if (((HlClockTimer->Features & TIMER_FEATURE_PER_PROCESSOR) != 0) ||
        (HlMaxProcessors == 1)) {

        switch (Mode) {
        case ClockTimerPeriodic:
            HlpTimerArm(HlClockTimer, TimerModePeriodic, HlClockRate);
            break;

        case ClockTimerOneShot:
            CurrentTime = HlQueryTimeCounter();
            if (CurrentTime < DueTime) {
                ClockTicks = ((DueTime - CurrentTime) *
                              HlClockTimer->CounterFrequency) /
                             HlTimeCounter->CounterFrequency;

            } else {
                ClockTicks = 0;
            }

            TimerMode = TimerModeOneShot;
            if ((HlClockTimer->Features & TIMER_FEATURE_ONE_SHOT) == 0) {
                TimerMode = TimerModePeriodic;
            }

            HlpTimerArm(HlClockTimer, TimerMode, ClockTicks);
            break;

        case ClockTimerOff:
            HlpTimerDisarm(HlClockTimer);
            break;

        default:

            ASSERT(FALSE);

            break;
        }

    //
    // There's only one timer for the clock, so all the processors' data is
    // multiplexed together.
    //

    } else {

        //
        // Figure out the next deadline globally, and whether it is soft or
        // hard.
        //

        ProcessorCount = KeGetActiveProcessorCount();
        NextHardDeadline = -1ULL;
        NextSoftDeadline = -1ULL;
        AllOff = TRUE;
        AnyPeriodic = FALSE;
        AnyHard = FALSE;
        OldRunLevel = KeRaiseRunLevel(RunLevelClock);
        KeAcquireSpinLock(&HlClockDataLock);
        HlClockRequests[ProcessorNumber].DueTime = DueTime;
        HlClockRequests[ProcessorNumber].Mode = Mode;
        HlClockRequests[ProcessorNumber].Hard = Hard;
        for (Index = 0; Index < ProcessorCount; Index += 1) {
            AnyHard |= HlClockRequests[Index].Hard;
            if (HlClockRequests[Index].Mode != ClockTimerOff) {
                AllOff = FALSE;
            }

            if (HlClockRequests[Index].Mode == ClockTimerPeriodic) {
                AnyPeriodic = TRUE;

            } else if (HlClockRequests[Index].Mode == ClockTimerOneShot) {
                ProcessorDueTime = HlClockRequests[Index].DueTime;
                if (HlClockRequests[Index].Hard != FALSE) {
                    if (ProcessorDueTime < NextHardDeadline) {
                        NextHardDeadline = ProcessorDueTime;
                    }

                } else {
                    if (ProcessorDueTime < NextSoftDeadline) {
                        NextSoftDeadline = ProcessorDueTime;
                    }
                }
            }
        }

        HlClockAnyPeriodic = AnyPeriodic;
        HlClockAnyHardDeadlines = AnyHard;
        HlClockNextHardDeadline = NextHardDeadline;

        //
        // If everyone's off, shut off the clock.
        //

        if (AllOff != FALSE) {
            if (HlClockMode != TimerModeInvalid) {
                HlpTimerDisarm(HlClockTimer);
                HlClockLastDueTime = -1ULL;
            }

            TimerMode = TimerModeInvalid;

        } else {

            //
            // If there are no periodic timers, take the minimum of the one-shot
            // deadlines.
            //

            if (AnyPeriodic == FALSE) {
                DueTime = NextSoftDeadline;
                if (NextHardDeadline < DueTime) {
                    DueTime = NextHardDeadline;
                }

                TimerMode = TimerModeOneShot;

            //
            // There is at least one vote for periodic. Go periodic unless the
            // hard deadline is first.
            //

            } else {
                if (NextHardDeadline == -1ULL) {
                    TimerMode = TimerModePeriodic;

                } else {
                    CurrentTime = KeGetRecentTimeCounter();
                    NextPeriodic = CurrentTime + HlClockRateInTimeCounterTicks;
                    if (NextHardDeadline < NextPeriodic) {
                        DueTime = NextHardDeadline;
                        TimerMode = TimerModeOneShot;

                    } else {
                        TimerMode = TimerModePeriodic;
                    }
                }
            }

            ArmTimer = TRUE;
            if (TimerMode == TimerModePeriodic) {
                ClockTicks = HlClockRate;
                DueTime = -1ULL;

            } else if (DueTime != HlClockLastDueTime) {
                CurrentTime = HlQueryTimeCounter();
                if (CurrentTime < DueTime) {
                    ClockTicks = ((DueTime - CurrentTime) *
                                  HlClockTimer->CounterFrequency) /
                                 HlTimeCounter->CounterFrequency;

                } else {
                    ClockTicks = 0;
                }

            } else {
                ArmTimer = FALSE;
            }

            //
            // Always set one-shot timers. Set periodic timers unless the clock
            // is already periodic.
            //

            ASSERT(TimerMode != TimerModeInvalid);

            if (ArmTimer != FALSE) {
                if ((TimerMode != TimerModePeriodic) ||
                    (TimerMode != HlClockMode)) {

                    SupportedTimerMode = TimerMode;
                    if ((TimerMode == TimerModeOneShot) &&
                        ((HlClockTimer->Features &
                          TIMER_FEATURE_ONE_SHOT) == 0)) {

                        SupportedTimerMode = TimerModePeriodic;
                    }

                    HlpTimerArm(HlClockTimer, SupportedTimerMode, ClockTicks);
                    HlClockLastProgrammedValue = ClockTicks;
                    HlClockLastDueTime = DueTime;
                }
            }
        }

        HlClockMode = TimerMode;
        KeReleaseSpinLock(&HlClockDataLock);
        KeLowerRunLevel(OldRunLevel);
    }

    return;
}

KSTATUS
HlpTimerInitializeClock (
    VOID
    )

/*++

Routine Description:

    This routine initializes the system clock source and start it ticking.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    UINTN AllocationSize;
    PKINTERRUPT ClockInterrupt;
    ULONGLONG GiveUpTime;
    ULONG Index;
    ULONG Processor;
    INTERRUPT_LINE_STATE State;
    KSTATUS Status;
    PROCESSOR_SET Target;

    Processor = KeGetCurrentProcessorNumber();
    RtlZeroMemory(&Target, sizeof(PROCESSOR_SET));
    Target.Target = ProcessorTargetSelf;
    State.Mode = HlClockTimer->Interrupt.TriggerMode;
    State.Polarity = HlClockTimer->Interrupt.ActiveLevel;
    State.Flags = INTERRUPT_LINE_STATE_FLAG_ENABLED;
    HlpInterruptGetStandardCpuLine(&(State.Output));
    if (Processor == 0) {
        KeInitializeSpinLock(&HlClockDataLock);
        if (((HlClockTimer->Features & TIMER_FEATURE_PER_PROCESSOR) == 0) &&
            (HlMaxProcessors > 1)) {

            AllocationSize = sizeof(CLOCK_REQUEST) * HlMaxProcessors;
            HlClockRequests = MmAllocateNonPagedPool(AllocationSize,
                                                     HL_POOL_TAG);

            if (HlClockRequests == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto InitializeClockEnd;
            }

            //
            // Start all clocks in periodic mode.
            //

            RtlZeroMemory(HlClockRequests, AllocationSize);
            for (Index = 0; Index < HlMaxProcessors; Index += 1) {
                HlClockRequests[Index].Mode = ClockTimerPeriodic;
            }
        }

        //
        // Configure the interrupt for the clock timer.
        //

        ClockInterrupt = HlpInterruptGetClockKInterrupt();
        HlClockTimer->InterruptRunLevel = ClockInterrupt->RunLevel;
        Status = HlpInterruptSetLineState(&(HlClockTimer->Interrupt.Line),
                                          &State,
                                          ClockInterrupt,
                                          &Target,
                                          NULL,
                                          0);

        if (!KSUCCESS(Status)) {
            goto InitializeClockEnd;
        }

        //
        // Compute the default periodic rate in ticks.
        //

        HlClockRate = HlpTimerTimeToTicks(HlClockTimer, DEFAULT_CLOCK_RATE);
        HlClockRateInTimeCounterTicks = HlpTimerTimeToTicks(HlTimeCounter,
                                                            DEFAULT_CLOCK_RATE);

        //
        // Fire up the clock timer.
        //

        Status = HlpTimerArm(HlClockTimer, TimerModePeriodic, HlClockRate);
        if (!KSUCCESS(Status)) {
            goto InitializeClockEnd;
        }

        //
        // Figure out when to give up if the clock interrupt doesn't seem to
        // be coming in.
        //

        GiveUpTime = HlQueryTimeCounter() +
                     (HlQueryTimeCounterFrequency() *
                      CLOCK_START_GRACE_PERIOD_SECONDS);

        //
        // Wait for interrupts to come in.
        //

        while (HlEarlyClockInterruptCount == 0) {
            if (HlQueryTimeCounter() >= GiveUpTime) {
                break;
            }

            ArProcessorYield();
        }

        if (HlEarlyClockInterruptCount == 0) {
            KeCrashSystem(CRASH_HARDWARE_LAYER_FAILURE,
                          HL_CRASH_CLOCK_WONT_START,
                          (UINTN)HlClockTimer,
                          HlClockTimer->Interrupt.Line.U.Gsi,
                          0);
        }

    //
    // Initialize the clock on all other processors.
    //

    } else {

        //
        // If the selected timer is per-processor, fire up the clock on this
        // processor.
        //

        if ((HlClockTimer->Features & TIMER_FEATURE_PER_PROCESSOR) != 0) {

            //
            // Configure the interrupt for the clock timer.
            //

            ClockInterrupt = HlpInterruptGetClockKInterrupt();
            Status = HlpInterruptSetLineState(&(HlClockTimer->Interrupt.Line),
                                              &State,
                                              ClockInterrupt,
                                              &Target,
                                              NULL,
                                              0);

            if (!KSUCCESS(Status)) {
                goto InitializeClockEnd;
            }

            Status = HlpTimerArm(HlClockTimer, TimerModePeriodic, HlClockRate);
            if (!KSUCCESS(Status)) {
                goto InitializeClockEnd;
            }

        //
        // If the timer is not per-processor, then set up to broadcast clock
        // interrupts.
        //

        } else {
            HlBroadcastClockInterrupts = TRUE;
            Status = STATUS_SUCCESS;
        }
    }

InitializeClockEnd:
    return Status;
}

KSTATUS
HlpTimerActivateClock (
    VOID
    )

/*++

Routine Description:

    This routine sets the clock handler routine to the main clock ISR.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    PKINTERRUPT ClockInterrupt;
    BOOL Enabled;
    KSTATUS Status;

    ClockInterrupt = HlpInterruptGetClockKInterrupt();
    Enabled = ArDisableInterrupts();
    ClockInterrupt->InterruptServiceRoutine = HlpClockInterruptHandler;
    if (Enabled != FALSE) {
        ArEnableInterrupts();
    }

    Status = STATUS_SUCCESS;
    return Status;
}

INTERRUPT_STATUS
HlpEarlyClockInterruptHandler (
    PVOID Context
    )

/*++

Routine Description:

    This routine responds to clock interrupts while the system is still in
    early initialization.

Arguments:

    Context - Supplies a context pointer. Currently unused.

Return Value:

    Claimed always.

--*/

{

    HlEarlyClockInterruptCount += 1;
    HlpTimerAcknowledgeInterrupt(HlClockTimer);
    return InterruptStatusClaimed;
}

INTERRUPT_STATUS
HlpClockInterruptHandler (
    PVOID Context
    )

/*++

Routine Description:

    This routine is the main clock ISR. This should only be called if there was
    a real clock interrupt on the current processor. IPIs do not count and
    should not run through this routine.

Arguments:

    Context - Supplies a context pointer. Currently unused.

Return Value:

    Claimed always.

--*/

{

    ULONGLONG ClockTicks;
    ULONGLONG CurrentTime;
    PROCESSOR_SET Processors;
    TIMER_MODE SupportedMode;

    //
    // Always acknowledge the interrupt. A real clock interrupt came in on this
    // processor and must be acknowledged.
    //

    HlpTimerAcknowledgeInterrupt(HlClockTimer);

    //
    // If it's not a per-processor timer and this is P0, then the next hard
    // deadline may require the timer to be rearmed differently and the clock
    // interrupt needs to be broadcast to the other cores.
    //

    if (((HlClockTimer->Features & TIMER_FEATURE_PER_PROCESSOR) == 0) &&
        (HlMaxProcessors > 1) &&
        (KeGetCurrentProcessorNumber() == 0)) {

        //
        // If running in periodic mode with a hard deadline coming up, see if
        // the timer needs to be rearmed.
        //

        if ((HlClockMode == TimerModePeriodic) &&
            (HlClockAnyHardDeadlines != FALSE)) {

            KeAcquireSpinLock(&HlClockDataLock);
            CurrentTime = HlQueryTimeCounter();
            if (CurrentTime + HlClockRateInTimeCounterTicks >
                HlClockNextHardDeadline) {

                if (CurrentTime > HlClockNextHardDeadline) {
                    CurrentTime = HlClockNextHardDeadline;
                }

                ClockTicks = (HlClockNextHardDeadline - CurrentTime) *
                             HlClockTimer->CounterFrequency /
                             HlTimeCounter->CounterFrequency;

                SupportedMode = TimerModeOneShot;
                if ((HlClockTimer->Features &
                     TIMER_FEATURE_ONE_SHOT) == 0) {

                    SupportedMode = TimerModePeriodic;
                }

                HlpTimerArm(HlClockTimer, SupportedMode, ClockTicks);
                HlClockMode = TimerModeOneShot;
                HlClockLastProgrammedValue = ClockTicks;
                HlClockLastDueTime = HlClockNextHardDeadline;
            }

            KeReleaseSpinLock(&HlClockDataLock);

        //
        // If the timer is in one-shot mode but there are periodic souls,
        // go back to periodic. Whoever called for the one-shot should
        // send down an updated mandate soon.
        //

        } else if ((HlClockMode == TimerModeOneShot) &&
                   (HlClockAnyPeriodic != FALSE)) {

            KeAcquireSpinLock(&HlClockDataLock);
            HlpTimerArm(HlClockTimer, TimerModePeriodic, HlClockRate);
            HlClockMode = TimerModePeriodic;
            HlClockLastProgrammedValue = HlClockRate;
            HlClockLastDueTime = -1ULL;
            KeReleaseSpinLock(&HlClockDataLock);
        }

        //
        // Broadcast the clock interrupt if needed.
        //

        if (HlBroadcastClockInterrupts != FALSE) {
            Processors.Target = ProcessorTargetAllExcludingSelf;
            HlSendIpi(IpiTypeClock, &Processors);
        }
    }

    KeClockInterrupt();
    return InterruptStatusClaimed;
}

INTERRUPT_STATUS
HlpClockIpiHandler (
    PVOID Context
    )

/*++

Routine Description:

    This routine is the ISR for clock IPIs. The main difference being that it
    does not need to acknowledge the clock interrupt in the hardware module as
    this interrupt is software generated. The second difference is that it does
    not need to broadcast the interrupt to other cores; that is only required
    for real clock ticks.

Arguments:

    Context - Supplies a context pointer. Currently unused.

Return Value:

    Claimed always.

--*/

{

    KeClockInterrupt();
    return InterruptStatusClaimed;
}

//
// --------------------------------------------------------- Internal Functions
//

