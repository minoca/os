/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    time.c

Abstract:

    This module implements OS layer support for timekeeping.

Author:

    Evan Green 28-Jul-2013

Environment:

    User Mode

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "osbasep.h"

//
// ---------------------------------------------------------------- Definitions
//

#define TIME_ALLOCATION_TAG 0x656D6954 // 'emiT'

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
OspTimerControl (
    TIMER_OPERATION Operation,
    LONG TimerNumber,
    PTIMER_INFORMATION Information
    );

KSTATUS
OspSetITimer (
    BOOL Set,
    ITIMER_TYPE Type,
    PULONGLONG DueTime,
    PULONGLONG Period
    );

VOID
OspGetTimeOffset (
    PSYSTEM_TIME TimeOffset
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

OS_API
ULONGLONG
OsGetRecentTimeCounter (
    VOID
    )

/*++

Routine Description:

    This routine returns a relatively recent snap of the time counter.

Arguments:

    None.

Return Value:

    Returns the fairly recent snap of the time counter.

--*/

{

    ULONGLONG TickCount;
    ULONGLONG TimeCounter;
    PUSER_SHARED_DATA UserSharedData;

    UserSharedData = OspGetUserSharedData();

    //
    // Loop reading the two tick count values to ensure the read of the time
    // counter variable wasn't torn.
    //

    do {
        TickCount = UserSharedData->TickCount;
        TimeCounter = UserSharedData->TimeCounter;

    } while (TickCount != UserSharedData->TickCount2);

    return TimeCounter;
}

OS_API
ULONGLONG
OsQueryTimeCounter (
    VOID
    )

/*++

Routine Description:

    This routine returns the current (most up to date) value of the system's
    time counter.

Arguments:

    None.

Return Value:

    Returns the time counter value.

--*/

{

    SYSTEM_CALL_QUERY_TIME_COUNTER Parameters;

    OsSystemCall(SystemCallQueryTimeCounter, &Parameters);
    return Parameters.Value;
}

OS_API
ULONGLONG
OsGetTimeCounterFrequency (
    VOID
    )

/*++

Routine Description:

    This routine returns the frequency of the time counter.

Arguments:

    None.

Return Value:

    Returns the frequency, in Hertz (ticks per second) of the time counter.

--*/

{

    PUSER_SHARED_DATA UserSharedData;

    UserSharedData = OspGetUserSharedData();
    return UserSharedData->TimeCounterFrequency;
}

OS_API
ULONGLONG
OsGetProcessorCounterFrequency (
    VOID
    )

/*++

Routine Description:

    This routine returns the frequency of the boot processor counter.

Arguments:

    None.

Return Value:

    Returns the frequency, in Hertz (ticks per second) of the boot processor
    counter.

--*/

{

    PUSER_SHARED_DATA UserSharedData;

    UserSharedData = OspGetUserSharedData();
    return UserSharedData->ProcessorCounterFrequency;
}

OS_API
VOID
OsConvertSystemTimeToTimeCounter (
    PSYSTEM_TIME SystemTime,
    PULONGLONG TimeCounter
    )

/*++

Routine Description:

    This routine converts a system time value into a time counter value.

Arguments:

    SystemTime - Supplies a pointer to the system time convert to a time
        counter value.

    TimeCounter - Supplies a pointer where the time counter value will be
        returned.

Return Value:

    None.

--*/

{

    ULONGLONG Frequency;
    LONGLONG Nanoseconds;
    ULONGLONG Result;
    ULONGLONG Seconds;
    SYSTEM_TIME TimeOffset;

    Frequency = OsGetTimeCounterFrequency();
    OspGetTimeOffset(&TimeOffset);
    Seconds = SystemTime->Seconds - TimeOffset.Seconds;
    Nanoseconds = SystemTime->Nanoseconds - TimeOffset.Nanoseconds;
    if (Nanoseconds < 0) {
        Seconds -= 1;
        Nanoseconds += NANOSECONDS_PER_SECOND;
    }

    Result = Seconds * Frequency;
    Result += (((ULONGLONG)Nanoseconds * Frequency) +
               (NANOSECONDS_PER_SECOND - 1)) /
              NANOSECONDS_PER_SECOND;

    *TimeCounter = Result;
    return;
}

OS_API
VOID
OsConvertTimeCounterToSystemTime (
    ULONGLONG TimeCounter,
    PSYSTEM_TIME SystemTime
    )

/*++

Routine Description:

    This routine converts a time counter value into a system time value.

Arguments:

    TimeCounter - Supplies the time counter value to convert.

    SystemTime - Supplies a pointer where the converted system time will be
        returned.

Return Value:

    None.

--*/

{

    ULONGLONG Frequency;
    ULONGLONG Nanoseconds;
    ULONGLONG Seconds;
    SYSTEM_TIME TimeOffset;

    Frequency = OsGetTimeCounterFrequency();
    OspGetTimeOffset(&TimeOffset);
    Seconds = TimeCounter / Frequency;
    SystemTime->Seconds = TimeOffset.Seconds + Seconds;
    TimeCounter -= (Seconds * Frequency);
    Nanoseconds = ((TimeCounter * NANOSECONDS_PER_SECOND) + (Frequency - 1)) /
                  Frequency;

    SystemTime->Nanoseconds = TimeOffset.Nanoseconds + Nanoseconds;
    if (SystemTime->Nanoseconds < 0) {
        SystemTime->Nanoseconds += NANOSECONDS_PER_SECOND;
        SystemTime->Seconds -= 1;
    }

    if (SystemTime->Nanoseconds >= NANOSECONDS_PER_SECOND) {
        SystemTime->Nanoseconds -= NANOSECONDS_PER_SECOND;
        SystemTime->Seconds += 1;
    }

    ASSERT((SystemTime->Nanoseconds >= 0) &&
           (SystemTime->Nanoseconds < NANOSECONDS_PER_SECOND));

    return;
}

OS_API
VOID
OsGetSystemTime (
    PSYSTEM_TIME Time
    )

/*++

Routine Description:

    This routine returns the current system time.

Arguments:

    Time - Supplies a pointer where the system time will be returned.

Return Value:

    None.

--*/

{

    ULONGLONG TickCount;
    PUSER_SHARED_DATA UserSharedData;

    UserSharedData = OspGetUserSharedData();

    //
    // Loop reading the two tick count values to ensure the read of the system
    // time structure wasn't torn.
    //

    do {
        TickCount = UserSharedData->TickCount;
        *Time = UserSharedData->SystemTime;

    } while (TickCount != UserSharedData->TickCount2);

    return;
}

OS_API
VOID
OsGetHighPrecisionSystemTime (
    PSYSTEM_TIME Time
    )

/*++

Routine Description:

    This routine returns a high precision snap of the current system time.

Arguments:

    Time - Supplies a pointer where the system time will be returned.

Return Value:

    None.

--*/

{

    ULONGLONG Delta;
    ULONGLONG Frequency;
    ULONGLONG Seconds;
    ULONGLONG TimeCounter;

    //
    // Get the time offset and time counter and calculate the system time from
    // those two values.
    //

    Frequency = OsGetTimeCounterFrequency();
    OspGetTimeOffset(Time);
    TimeCounter = OsQueryTimeCounter();
    Seconds = TimeCounter / Frequency;
    Time->Seconds += Seconds;
    Delta = TimeCounter - (Seconds * Frequency);

    //
    // Since the seconds were subtracted off, there could be at most a billion
    // nanoseconds to add. If the nanoseconds are currently under a billion like
    // they should be, then this add should never overflow. Unless the time
    // counter itself is overflowing constantly, the multiply should also be
    // nowhere near overflowing.
    //

    ASSERT(Frequency <= (MAX_ULONGLONG / NANOSECONDS_PER_SECOND));

    Time->Nanoseconds += (Delta * NANOSECONDS_PER_SECOND) / Frequency;

    //
    // Normalize the nanoseconds back into the 0 to 1 billion range.
    //

    if (Time->Nanoseconds > NANOSECONDS_PER_SECOND) {
        Time->Nanoseconds -= NANOSECONDS_PER_SECOND;
        Time->Seconds += 1;

        ASSERT((Time->Nanoseconds > 0) &&
               (Time->Nanoseconds < NANOSECONDS_PER_SECOND));
    }

    return;
}

OS_API
KSTATUS
OsSetSystemTime (
    PSYSTEM_TIME NewTime,
    ULONGLONG TimeCounter
    )

/*++

Routine Description:

    This routine sets the current system time.

Arguments:

    NewTime - Supplies a pointer to the new system time to set.

    TimeCounter - Supplies the time counter value corresponding with the
        moment the system time was meant to be set by the caller.

Return Value:

    Status code.

--*/

{

    SYSTEM_CALL_SET_SYSTEM_TIME Parameters;

    Parameters.SystemTime = *NewTime;
    Parameters.TimeCounter = TimeCounter;
    return OsSystemCall(SystemCallSetSystemTime, &Parameters);
}

OS_API
KSTATUS
OsGetResourceUsage (
    RESOURCE_USAGE_REQUEST Request,
    PROCESS_ID Id,
    PRESOURCE_USAGE Usage,
    PULONGLONG Frequency
    )

/*++

Routine Description:

    This routine returns resource usage information for the specified process
    or thread.

Arguments:

    Request - Supplies the request type, indicating whether to get resource
        usage for a process, a process' children, or a thread.

    Id - Supplies the process or thread ID. Supply -1 to use the current
        process or thread.

    Usage - Supplies a pointer where the resource usage is returned on success.

    Frequency - Supplies a pointer that receives the frequency of the
        processor(s).

Return Value:

    Status code.

--*/

{

    SYSTEM_CALL_GET_RESOURCE_USAGE Parameters;
    KSTATUS Status;

    Parameters.Request = Request;
    Parameters.Id = Id;
    Status = OsSystemCall(SystemCallGetResourceUsage, &Parameters);
    if (KSUCCESS(Status)) {
        if (Usage != NULL) {
            RtlCopyMemory(Usage, &(Parameters.Usage), sizeof(RESOURCE_USAGE));
        }

        if (Frequency != NULL) {
            *Frequency = Parameters.Frequency;
        }
    }

    return Status;
}

OS_API
KSTATUS
OsCreateTimer (
    ULONG SignalNumber,
    PUINTN SignalValue,
    PTHREAD_ID ThreadId,
    PLONG TimerHandle
    )

/*++

Routine Description:

    This routine creates a new timer.

Arguments:

    SignalNumber - Supplies the signal number to raise when the timer expires.

    SignalValue - Supplies an optional pointer to the signal value to put in
        the signal information structure when the signal is raised. If this is
         NULL, the timer number will be returned as the signal value.

    ThreadId - Supplies an optional ID of the thread to signal when the timer
        expires. If not supplied, the process will be signaled.

    TimerHandle - Supplies a pointer where the timer handle will be returned on
        success.

Return Value:

    Status code.

--*/

{

    SYSTEM_CALL_TIMER_CONTROL Parameters;
    KSTATUS Status;

    RtlZeroMemory(&Parameters, sizeof(SYSTEM_CALL_TIMER_CONTROL));
    Parameters.Operation = TimerOperationCreateTimer;
    Parameters.SignalNumber = SignalNumber;
    if (SignalValue != NULL) {
        Parameters.SignalValue = *SignalValue;

    } else {
        Parameters.Flags |= TIMER_CONTROL_FLAG_USE_TIMER_NUMBER;
    }

    if (ThreadId != NULL) {
        Parameters.ThreadId = *ThreadId;
        Parameters.Flags |= TIMER_CONTROL_FLAG_SIGNAL_THREAD;
    }

    Status = OsSystemCall(SystemCallTimerControl, &Parameters);
    *TimerHandle = Parameters.TimerNumber;
    return Status;
}

OS_API
KSTATUS
OsDeleteTimer (
    LONG Timer
    )

/*++

Routine Description:

    This routine disarms and deletes a timer.

Arguments:

    Timer - Supplies the timer to delete.

Return Value:

    Status code.

--*/

{

    return OspTimerControl(TimerOperationDeleteTimer, Timer, NULL);
}

OS_API
KSTATUS
OsGetTimerInformation (
    LONG Timer,
    PTIMER_INFORMATION Information
    )

/*++

Routine Description:

    This routine gets the given timer's information.

Arguments:

    Timer - Supplies the timer to query.

    Information - Supplies a pointer where the timer information will be
        returned.

Return Value:

    Status code.

--*/

{

    //
    // Be helpful in debugging, but don't tolerate incompetence for new APIs.
    //

    ASSERT(Information != NULL);

    return OspTimerControl(TimerOperationGetTimer, Timer, Information);
}

OS_API
KSTATUS
OsSetTimerInformation (
    LONG Timer,
    PTIMER_INFORMATION Information
    )

/*++

Routine Description:

    This routine sets the given timer's information.

Arguments:

    Timer - Supplies the timer to set.

    Information - Supplies a pointer to the information to set.

Return Value:

    Status code.

--*/

{

    ASSERT(Information != NULL);

    return OspTimerControl(TimerOperationSetTimer, Timer, Information);
}

OS_API
KSTATUS
OsGetITimer (
    ITIMER_TYPE Type,
    PULONGLONG DueTime,
    PULONGLONG Period
    )

/*++

Routine Description:

    This routine gets the current value of one of the per-thread interval
    timers.

Arguments:

    Type - Supplies the timer type. Valid values are ITimerReal, which tracks
        wall clock time, ITimerVirtual, which tracks user mode CPU cycles
        spent in this thread, and ITimerProfile, which tracks user and kernel
        CPU cycles spent in this thread.

    DueTime - Supplies a pointer where the relative due time will be returned
        for this timer. Zero will be returned if the timer is not currently
        armed or has already expired. The units here are time counter ticks for
        the real timer, and processor counter ticks for the virtual and profile
        timers.

    Period - Supplies a pointer where the periodic interval of the timer
        will be returned. Zero indicates the timer is not set to rearm itself.
        The units here are time counter ticks for the real timer, and processor
        counter ticks for the firtual and profile timers.

Return Value:

    Status code.

--*/

{

    return OspSetITimer(FALSE, Type, DueTime, Period);
}

OS_API
KSTATUS
OsSetITimer (
    ITIMER_TYPE Type,
    PULONGLONG DueTime,
    PULONGLONG Period
    )

/*++

Routine Description:

    This routine sets the current value of one of the per-thread interval
    timers.

Arguments:

    Type - Supplies the timer type. Valid values are ITimerReal, which tracks
        wall clock time, ITimerVirtual, which tracks user mode CPU cycles
        spent in this thread, and ITimerProfile, which tracks user and kernel
        CPU cycles spent in this thread.

    DueTime - Supplies a pointer to the relative time to set in the timer.
        Supply zero to disable the timer. The units here are time counter ticks
        for the real timer, and processor counter ticks for the virtual and
        profile timers. On output, this will contain the remaining time left on
        the previously set value for the timer.

    Period - Supplies a pointer to the periodic interval to set. Set zero to
        make the timer only fire once. The units here are time counter ticks
        for the real timer, and processor counter ticks for the firtual and
        profile timers. On output, the previous period will be returned.

Return Value:

    Status code.

--*/

{

    return OspSetITimer(TRUE, Type, DueTime, Period);
}

OS_API
KSTATUS
OsDelayExecution (
    BOOL TimeTicks,
    ULONGLONG Interval
    )

/*++

Routine Description:

    This routine blocks the current thread for the specified amount of time.

Arguments:

    TimeTicks - Supplies a boolean indicating if the interval parameter is
        represented in time counter ticks (TRUE) or microseconds (FALSE).

    Interval - Supplies the interval to wait. If the time ticks parameter is
        TRUE, this parameter represents an absolute time in time counter ticks.
        If the time ticks parameter is FALSE, this parameter represents a
        relative time from now in microseconds. If an interval of 0 is
        supplied, this routine is equivalent to KeYield.

Return Value:

    STATUS_SUCCESS if the wait completed.

    STATUS_INTERRUPTED if the wait was interrupted.

--*/

{

    SYSTEM_CALL_DELAY_EXECUTION Parameters;

    Parameters.TimeTicks = TimeTicks;
    Parameters.Interval = Interval;
    return OsSystemCall(SystemCallDelayExecution, &Parameters);
}

PUSER_SHARED_DATA
OspGetUserSharedData (
    VOID
    )

/*++

Routine Description:

    This routine returns a pointer to the user shared data.

Arguments:

    None.

Return Value:

    Returns a pointer to the user shared data area.

--*/

{

    return (PUSER_SHARED_DATA)USER_SHARED_DATA_USER_ADDRESS;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
OspTimerControl (
    TIMER_OPERATION Operation,
    LONG TimerNumber,
    PTIMER_INFORMATION Information
    )

/*++

Routine Description:

    This routine performs a timer control operation.

Arguments:

    Operation - Supplies the timer operation to perform.

    TimerNumber - Supplies the timer number to operate on.

    Information - Supplies an optional pointer to the timer information to get
        or set.

Return Value:

    Status code.

--*/

{

    SYSTEM_CALL_TIMER_CONTROL Parameters;
    KSTATUS Status;

    RtlZeroMemory(&Parameters, sizeof(SYSTEM_CALL_TIMER_CONTROL));
    Parameters.Operation = Operation;
    Parameters.TimerNumber = TimerNumber;
    if (Operation == TimerOperationSetTimer) {
        RtlCopyMemory(&(Parameters.TimerInformation),
                      Information,
                      sizeof(TIMER_INFORMATION));
    }

    Status = OsSystemCall(SystemCallTimerControl, &Parameters);
    if (Information != NULL) {
        RtlCopyMemory(Information,
                      &(Parameters.TimerInformation),
                      sizeof(TIMER_INFORMATION));
    }

    return Status;
}

KSTATUS
OspSetITimer (
    BOOL Set,
    ITIMER_TYPE Type,
    PULONGLONG DueTime,
    PULONGLONG Period
    )

/*++

Routine Description:

    This routine gets or sets the current value of one of the per-thread
    interval timers.

Arguments:

    Set - Supplies a boolean indicating whether to get or set the interval
        timer.

    Type - Supplies the timer type.

    DueTime - Supplies a pointer that for set operations takes the relative
        due time in ticks (either time counter or processor counter). On
        output, returns the previous due time relative to now.

    Period - Supplies a pointer that for set operations take the periodic
        interval to set. On output, returns the previous period for both get
        and set operations.

Return Value:

    Status code.

--*/

{

    SYSTEM_CALL_SET_ITIMER Request;
    KSTATUS Status;

    Request.Set = Set;
    Request.Type = Type;
    Request.DueTime = *DueTime;
    Request.Period = *Period;
    Status = OsSystemCall(SystemCallSetITimer, &Request);
    *DueTime = Request.DueTime;
    *Period = Request.Period;
    return Status;
}

VOID
OspGetTimeOffset (
    PSYSTEM_TIME TimeOffset
    )

/*++

Routine Description:

    This routine reads the time offset from the shared user data page.

Arguments:

    TimeOffset - Supplies a pointer that receives the time offset from the
        shared user data page.

Return Value:

    None.

--*/

{

    ULONGLONG TickCount;
    PUSER_SHARED_DATA UserSharedData;

    UserSharedData = OspGetUserSharedData();

    //
    // Loop reading the two tick count values to ensure the read of the time
    // offset structure wasn't torn.
    //

    do {
        TickCount = UserSharedData->TickCount;
        *TimeOffset = UserSharedData->TimeOffset;

    } while (TickCount != UserSharedData->TickCount2);

    return;
}

