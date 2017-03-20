/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    calendar.c

Abstract:

    This module implements support for calendar timer hardware modules.

Author:

    Evan Green 20-Sep-2013

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
#include "efi.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure store information about a calendar timer that has been
    registered with the system.

Members:

    ListEntry - Stores pointers to the next and previous calendar timers in the
        system.

    FunctionTable - Stores pointers to functions implemented by the hardware
        module abstracting this timer.

    Identifier - Stores the unique hardware identifier of the timer.

    Features - Stores a bitfield of timer features. See
        CALENDAR_TIMER_FEATURE_* definitions.

    Flags - Stores pointers to a bitfield of flags defining state of the
        controller. See CALENDAR_TIMER_FLAG_* definitions.

    PrivateContext - Stores a pointer to the hardware module's private
        context.

--*/

typedef struct _CALENDAR_TIMER {
    LIST_ENTRY ListEntry;
    CALENDAR_TIMER_FUNCTION_TABLE FunctionTable;
    ULONG Identifier;
    ULONG Features;
    ULONG Flags;
    PVOID PrivateContext;
} CALENDAR_TIMER, *PCALENDAR_TIMER;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
HlpFirmwareUpdateCalendarTime (
    VOID
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store the list of registered calendar timers.
//

LIST_ENTRY HlCalendarTimers;

//
// Store a spin lock to synchronize access to the hardware's calendar timer.
//

KSPIN_LOCK HlCalendarTimerLock;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
HlQueryCalendarTime (
    PSYSTEM_TIME SystemTime,
    PULONGLONG TimeCounter
    )

/*++

Routine Description:

    This routine returns the current calendar time as reported by the hardware
    calendar time source.

Arguments:

    SystemTime - Supplies a pointer where the system time as read from the
        hardware will be returned.

    TimeCounter - Supplies a pointer where a time counter value corresponding
        with the approximate moment the calendar time was read will be
        returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NO_SUCH_DEVICE if there are no registered calendar timer modules.

    Other errors on calendar timer hardware failure.

--*/

{

    ULONGLONG BeginTime;
    PCALENDAR_TIMER CalendarTimer;
    PLIST_ENTRY CurrentEntry;
    BOOL Enabled;
    ULONGLONG EndTime;
    HARDWARE_MODULE_TIME HardwareTime;
    RUNLEVEL OldRunLevel;
    PCALENDAR_TIMER_READ Read;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    BeginTime = 0;
    EndTime = 0;
    KeAcquireSpinLock(&HlCalendarTimerLock);

    //
    // Loop through until a calendar timer read succeeds.
    //

    Status = STATUS_NO_SUCH_DEVICE;
    CurrentEntry = HlCalendarTimers.Next;
    while (CurrentEntry != &HlCalendarTimers) {
        CalendarTimer = LIST_VALUE(CurrentEntry, CALENDAR_TIMER, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if ((CalendarTimer->Flags & CALENDAR_TIMER_FLAG_FAILED) != 0) {
            continue;
        }

        ASSERT((CalendarTimer->Flags & CALENDAR_TIMER_FLAG_INITIALIZED) != 0);

        Read = CalendarTimer->FunctionTable.Read;
        RtlZeroMemory(&HardwareTime, sizeof(HARDWARE_MODULE_TIME));
        if ((CalendarTimer->Features &
             CALENDAR_TIMER_FEATURE_LOW_RUNLEVEL) == 0) {

            OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
            Enabled = ArDisableInterrupts();

        } else {
            OldRunLevel = KeGetRunLevel();
            Enabled = FALSE;
        }

        BeginTime = HlQueryTimeCounter();
        Status = Read(CalendarTimer->PrivateContext, &HardwareTime);
        EndTime = HlQueryTimeCounter();
        if (Enabled != FALSE) {
            ArEnableInterrupts();
        }

        KeLowerRunLevel(OldRunLevel);
        if (KSUCCESS(Status)) {
            break;
        }
    }

    if (!KSUCCESS(Status)) {
        goto QueryCalendarTimeEnd;
    }

    //
    // If the timer returned system time, then just set it.
    //

    if (HardwareTime.IsCalendarTime == FALSE) {
        SystemTime->Seconds = HardwareTime.U.SystemTime.Seconds;
        SystemTime->Nanoseconds = HardwareTime.U.SystemTime.Nanoseconds;

    } else {
        Status = RtlCalendarTimeToSystemTime(&(HardwareTime.U.CalendarTime),
                                             SystemTime);

        if (!KSUCCESS(Status)) {
            goto QueryCalendarTimeEnd;
        }
    }

    //
    // Estimate the time counter value when this calendar time was snapped as
    // halfway between begin and end. Do the subtraction first to avoid
    // overflows.
    //

    *TimeCounter = BeginTime + ((EndTime - BeginTime) / 2);

QueryCalendarTimeEnd:
    KeReleaseSpinLock(&HlCalendarTimerLock);
    return Status;
}

KERNEL_API
KSTATUS
HlUpdateCalendarTime (
    VOID
    )

/*++

Routine Description:

    This routine updates the first available hardware calendar time with a snap
    of the current system time.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    PCALENDAR_TIME CalendarTime;
    PCALENDAR_TIMER CalendarTimer;
    PLIST_ENTRY CurrentEntry;
    BOOL Enabled;
    HARDWARE_MODULE_TIME HardwareTime;
    RUNLEVEL OldRunLevel;
    KSTATUS Status;
    SYSTEM_TIME SystemTime;
    PCALENDAR_TIMER_WRITE Write;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Status = STATUS_NO_SUCH_DEVICE;
    KeAcquireSpinLock(&HlCalendarTimerLock);

    //
    // Loop through until a calendar timer write succeeds.
    //

    CurrentEntry = HlCalendarTimers.Next;
    while (CurrentEntry != &HlCalendarTimers) {
        CalendarTimer = LIST_VALUE(CurrentEntry, CALENDAR_TIMER, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if ((CalendarTimer->Flags & CALENDAR_TIMER_FLAG_FAILED) != 0) {
            continue;
        }

        ASSERT((CalendarTimer->Flags & CALENDAR_TIMER_FLAG_INITIALIZED) != 0);

        Write = CalendarTimer->FunctionTable.Write;
        RtlZeroMemory(&HardwareTime, sizeof(HARDWARE_MODULE_TIME));

        //
        // Perform the calendar time set operation at higher runlevel or with
        // interrupts diabled in order to reduce the amount of slippage between
        // snapping the system time and setting the calendar time.
        //

        if ((CalendarTimer->Features &
             CALENDAR_TIMER_FEATURE_LOW_RUNLEVEL) == 0) {

            OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
            Enabled = ArDisableInterrupts();

        } else {
            OldRunLevel = KeGetRunLevel();
            Enabled = FALSE;
        }

        //
        // Get a high precision snap of the system time.
        //

        KeGetHighPrecisionSystemTime(&SystemTime);

        //
        // Convert the system time to the data structure expected by the
        // hardware.
        //

        if ((CalendarTimer->Features &
             CALENDAR_TIMER_FEATURE_WANT_CALENDAR_FORMAT) != 0) {

            CalendarTime = &(HardwareTime.U.CalendarTime);
            RtlSystemTimeToGmtCalendarTime(&SystemTime, CalendarTime);
            HardwareTime.IsCalendarTime = TRUE;

        } else {
            HardwareTime.IsCalendarTime = FALSE;
            HardwareTime.U.SystemTime.Seconds = SystemTime.Seconds;
            HardwareTime.U.SystemTime.Nanoseconds = SystemTime.Nanoseconds;
        }

        Status = Write(CalendarTimer->PrivateContext, &HardwareTime);
        if (Enabled != FALSE) {
            ArEnableInterrupts();
        }

        KeLowerRunLevel(OldRunLevel);
        if (KSUCCESS(Status)) {
            break;
        }
    }

    KeReleaseSpinLock(&HlCalendarTimerLock);

    //
    // If not successful using the hardware module(s), try using firmware
    // services.
    //

    if (!KSUCCESS(Status)) {
        Status = HlpFirmwareUpdateCalendarTime();
    }

    return Status;
}

KSTATUS
HlpInitializeCalendarTimers (
    PKERNEL_INITIALIZATION_BLOCK Parameters
    )

/*++

Routine Description:

    This routine initializes the interrupt subsystem.

Arguments:

    Parameters - Supplies a pointer to the kernel's initialization information.

Return Value:

    Status code.

--*/

{

    ULONGLONG Frequency;
    LONG Nanoseconds;
    ULONGLONG Seconds;
    KSTATUS Status;
    SYSTEM_TIME SystemTime;
    ULONGLONG TimeCounter;
    SYSTEM_TIME TimeOffset;
    PUSER_SHARED_DATA UserSharedData;

    Status = STATUS_SUCCESS;
    if (KeGetCurrentProcessorNumber() == 0) {
        KeInitializeSpinLock(&HlCalendarTimerLock);

        //
        // The list head was initialized in the timers module so that hardware
        // modules could register both timers and calendar timers in one entry
        // point.
        //

        ASSERT(HlCalendarTimers.Next != NULL);

        //
        // Perform architecture-specific initialization.
        //

        Status = HlpArchInitializeCalendarTimers();
        if (!KSUCCESS(Status)) {
            goto InitializeCalendarTimersEnd;
        }

        //
        // Perform an initial query of the calendar time.
        //

        Status = HlQueryCalendarTime(&SystemTime, &TimeCounter);
        if (KSUCCESS(Status)) {
            UserSharedData = MmGetUserSharedData();

            //
            // Given this matched pair, figure out the system time when the
            // time counter was zero.
            //

            Frequency = HlQueryTimeCounterFrequency();
            Seconds = TimeCounter / Frequency;
            TimeOffset.Seconds = SystemTime.Seconds - Seconds;
            TimeCounter -= Seconds * Frequency;
            Nanoseconds = (TimeCounter * NANOSECONDS_PER_SECOND) / Frequency;

            ASSERT((Nanoseconds >= 0) &&
                   (Nanoseconds < NANOSECONDS_PER_SECOND));

            TimeOffset.Nanoseconds = SystemTime.Nanoseconds - Nanoseconds;
            if (TimeOffset.Nanoseconds < 0) {
                TimeOffset.Nanoseconds += NANOSECONDS_PER_SECOND;
                TimeOffset.Seconds -= 1;
            }

            ASSERT((TimeOffset.Nanoseconds >= 0) &&
                   (TimeOffset.Nanoseconds < NANOSECONDS_PER_SECOND));

            //
            // Set the time offset. Normally one has to be very careful about
            // torn reads and such, but since this is single threaded early
            // system initialization there's nothing else to worry about. There
            // are no consumers of the time offset yet, not even the clock
            // interrupt.
            //

            UserSharedData->TimeOffset = TimeOffset;
        }

        Status = STATUS_SUCCESS;
    }

InitializeCalendarTimersEnd:
    return Status;
}

KSTATUS
HlpCalendarTimerRegisterHardware (
    PCALENDAR_TIMER_DESCRIPTION TimerDescription
    )

/*++

Routine Description:

    This routine is called to register a new calendar timer with the system.

Arguments:

    TimerDescription - Supplies a pointer to a structure describing the new
        calendar timer.

Return Value:

    Status code.

--*/

{

    ULONG AllocationSize;
    PCALENDAR_TIMER CalendarTimer;
    PCALENDAR_TIMER_INITIALIZE Initialize;
    KSTATUS Status;

    CalendarTimer = NULL;

    //
    // Check the table version.
    //

    if (TimerDescription->TableVersion < CALENDAR_TIMER_DESCRIPTION_VERSION) {
        Status = STATUS_INVALID_PARAMETER;
        goto CalendarTimerRegisterHardwareEnd;
    }

    //
    // Check required function pointers.
    //

    if (TimerDescription->FunctionTable.Read == NULL) {
        Status = STATUS_INVALID_PARAMETER;
        goto CalendarTimerRegisterHardwareEnd;
    }

    //
    // Allocate the new controller object.
    //

    AllocationSize = sizeof(CALENDAR_TIMER);
    CalendarTimer = MmAllocateNonPagedPool(AllocationSize, HL_POOL_TAG);
    if (CalendarTimer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CalendarTimerRegisterHardwareEnd;
    }

    RtlZeroMemory(CalendarTimer, AllocationSize);

    //
    // Initialize the new timer based on the description.
    //

    RtlCopyMemory(&(CalendarTimer->FunctionTable),
                  &(TimerDescription->FunctionTable),
                  sizeof(CALENDAR_TIMER_FUNCTION_TABLE));

    CalendarTimer->Identifier = TimerDescription->Identifier;
    CalendarTimer->PrivateContext = TimerDescription->Context;
    CalendarTimer->Flags = 0;
    CalendarTimer->Features = TimerDescription->Features;

    //
    // Insert the timer on the list.
    //

    INSERT_BEFORE(&(CalendarTimer->ListEntry), &HlCalendarTimers);

    //
    // Initialize the new calendar timer immediately.
    //

    Status = STATUS_SUCCESS;
    Initialize = CalendarTimer->FunctionTable.Initialize;
    if (Initialize != NULL) {
        Status = Initialize(CalendarTimer->PrivateContext);
    }

    if (!KSUCCESS(Status)) {
        CalendarTimer->Flags |= CALENDAR_TIMER_FLAG_FAILED;

    } else {
        CalendarTimer->Flags |= CALENDAR_TIMER_FLAG_INITIALIZED;
    }

    Status = STATUS_SUCCESS;

CalendarTimerRegisterHardwareEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
HlpFirmwareUpdateCalendarTime (
    VOID
    )

/*++

Routine Description:

    This routine attempts to set the hardware calendar timer using EFI firmware
    calls.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    CALENDAR_TIME CalendarTime;
    EFI_TIME EfiTime;
    BOOL Enabled;
    KSTATUS Status;
    SYSTEM_TIME SystemTime;

    //
    // Get a high precision snap of the system time.
    //

    KeGetHighPrecisionSystemTime(&SystemTime);

    //
    // Convert the system time to the data structure expected by the
    // hardware.
    //

    RtlSystemTimeToGmtCalendarTime(&SystemTime, &CalendarTime);

    //
    // Convert the calendar time to an EFI time.
    //

    Enabled = ArDisableInterrupts();
    EfiTime.Year = CalendarTime.Year;
    EfiTime.Month = CalendarTime.Month + 1;
    EfiTime.Day = CalendarTime.Day;
    EfiTime.Hour = CalendarTime.Hour;
    EfiTime.Minute = CalendarTime.Minute;
    EfiTime.Second = CalendarTime.Second;
    EfiTime.Nanosecond = CalendarTime.Nanosecond;
    EfiTime.TimeZone = CalendarTime.GmtOffset / SECONDS_PER_MINUTE;
    EfiTime.Daylight = CalendarTime.IsDaylightSaving;
    Status = HlpEfiSetTime(&EfiTime);
    if (Enabled != FALSE) {
        ArEnableInterrupts();
    }

    return Status;
}

