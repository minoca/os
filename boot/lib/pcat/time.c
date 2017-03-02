/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    time.c

Abstract:

    This module contains time support routines for running on a BIOS PC/AT
    system.

Author:

    Evan Green 10-Apr-2014

Environment:

    Boot

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/x86.h>
#include "firmware.h"
#include "bootlib.h"
#include "realmode.h"
#include "bios.h"

//
// ---------------------------------------------------------------- Definitions
//

#define BIOS_TIME_SERVICES 0x1A

#define INT1A_GET_TICK_COUNT 0x00
#define INT1A_READ_RTC_TIME  0x02
#define INT1A_READ_RTC_DATE  0x04

#define BIOS_GET_TIME_TRY_COUNT 6

//
// The BIOS timer ticks 18.2065 times per second. So there are 54925.439
// microseconds per tick.
//

#define BIOS_MICROSECONDS_PER_TICK 54925

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
FwpPcatGetDate (
    PULONG Year,
    PULONG Month,
    PULONG Day
    );

KSTATUS
FwpPcatGetTime (
    PULONG Hour,
    PULONG Minute,
    PULONG Second
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
FwPcatGetCurrentTime (
    PSYSTEM_TIME Time
    )

/*++

Routine Description:

    This routine attempts to get the current system time.

Arguments:

    Time - Supplies a pointer where the current time will be returned.

Return Value:

    Status code.

--*/

{

    CALENDAR_TIME CalendarTime;
    ULONG Day;
    ULONG Day2;
    ULONG Hour;
    ULONG Minute;
    ULONG Month;
    ULONG Month2;
    ULONG Second;
    KSTATUS Status;
    UINTN Try;
    ULONG Year;
    ULONG Year2;

    //
    // Loop reading the date, time and date again to get a consistent read.
    // Try each operation a few times in case it catches the RTC in the middle
    // of an update.
    //

    do {
        for (Try = 0; Try < BIOS_GET_TIME_TRY_COUNT; Try += 1) {
            Status = FwpPcatGetDate(&Year, &Month, &Day);
            if (KSUCCESS(Status)) {
                break;
            }
        }

        if (!KSUCCESS(Status)) {
            goto PcatGetCurrentTimeEnd;
        }

        for (Try = 0; Try < BIOS_GET_TIME_TRY_COUNT; Try += 1) {
            Status = FwpPcatGetTime(&Hour, &Minute, &Second);
            if (KSUCCESS(Status)) {
                break;
            }
        }

        if (!KSUCCESS(Status)) {
            goto PcatGetCurrentTimeEnd;
        }

        for (Try = 0; Try < BIOS_GET_TIME_TRY_COUNT; Try += 1) {
            Status = FwpPcatGetDate(&Year2, &Month2, &Day2);
            if (KSUCCESS(Status)) {
                break;
            }
        }

        if (!KSUCCESS(Status)) {
            goto PcatGetCurrentTimeEnd;
        }

    } while ((Day != Day2) || (Month != Month2) || (Year != Year2));

    //
    // Initialize a calendar time structure.
    //

    RtlZeroMemory(&CalendarTime, sizeof(CALENDAR_TIME));
    CalendarTime.Year = Year;
    CalendarTime.Month = Month - 1;
    CalendarTime.Day = Day;
    CalendarTime.Hour = Hour;
    CalendarTime.Minute = Minute;
    CalendarTime.Second = Second;
    Status = RtlCalendarTimeToSystemTime(&CalendarTime, Time);
    if (!KSUCCESS(Status)) {
        goto PcatGetCurrentTimeEnd;
    }

PcatGetCurrentTimeEnd:
    return Status;
}

KSTATUS
FwPcatStall (
    ULONG Microseconds
    )

/*++

Routine Description:

    This routine performs a short busy stall using INT 0x1A function 0, which
    returns a counter that increments 18.6025 times per second. Callers are
    advised to perform a "warm-up" stall to align to tick boundaries for more
    accurate results.

Arguments:

    Microseconds - Supplies the number of microseconds to stall for.

Return Value:

    Status code.

--*/

{

    ULONG OriginalEflags;
    ULONG OriginalEip;
    ULONG OriginalEsp;
    ULONG PreviousTick;
    REAL_MODE_CONTEXT RealModeContext;
    KSTATUS Status;
    ULONG Tick;
    ULONG TicksNeeded;
    ULONG TicksSeen;

    //
    // Create a standard BIOS call context.
    //

    Status = FwpRealModeCreateBiosCallContext(&RealModeContext,
                                              BIOS_TIME_SERVICES);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    OriginalEip = RealModeContext.Eip;
    OriginalEsp = RealModeContext.Esp;
    OriginalEflags = RealModeContext.Eflags;

    //
    // Convert the number of microseconds to a tick count, truncating up.
    //

    TicksNeeded = (Microseconds + BIOS_MICROSECONDS_PER_TICK - 1) /
                  BIOS_MICROSECONDS_PER_TICK;

    RealModeContext.Eax = INT1A_GET_TICK_COUNT << 8;
    FwpRealModeExecute(&RealModeContext);
    PreviousTick = ((RealModeContext.Ecx & 0xFFFF) << 16) |
                   (RealModeContext.Edx & 0xFFFF);

    TicksSeen = 0;
    while (TicksSeen < TicksNeeded) {
        RealModeContext.Eip = OriginalEip;
        RealModeContext.Esp = OriginalEsp;
        RealModeContext.Eflags = OriginalEflags;
        RealModeContext.Eax = INT1A_GET_TICK_COUNT << 8;
        FwpRealModeExecute(&RealModeContext);
        Tick = ((RealModeContext.Ecx & 0xFFFF) << 16) |
               (RealModeContext.Edx & 0xFFFF);

        if (Tick != PreviousTick) {
            TicksSeen += Tick - PreviousTick;
            PreviousTick = Tick;
        }
    }

    Status = STATUS_SUCCESS;
    FwpRealModeDestroyBiosCallContext(&RealModeContext);
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
FwpPcatGetDate (
    PULONG Year,
    PULONG Month,
    PULONG Day
    )

/*++

Routine Description:

    This routine uses the BIOS to read the current date.

Arguments:

    Year - Supplies a pointer where the year will be returned on success.

    Month - Supplies a pointer where the month will be returned on success.

    Day - Supplies a pointer where the day will be returned on success.

Return Value:

    STATUS_SUCCESS if the operation completed successfully.

    STATUS_FIRMWARE_ERROR if the BIOS returned an error.

    Other error codes.

--*/

{

    REAL_MODE_CONTEXT RealModeContext;
    KSTATUS Status;

    //
    // Create a standard BIOS call context.
    //

    Status = FwpRealModeCreateBiosCallContext(&RealModeContext,
                                              BIOS_TIME_SERVICES);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // Int 0x1A function 4 reads the real time clock data. Ah takes the
    // function number. On return, CH contains the century (19 or 20), CL
    // contains the year, DH contains the month, and DL contains the day, all
    // in binary coded decimal.
    //

    RealModeContext.Eax = INT1A_READ_RTC_DATE << 8;

    //
    // Execute the firmware call.
    //

    FwpRealModeExecute(&RealModeContext);

    //
    // Check for an error.
    //

    if ((RealModeContext.Eflags & IA32_EFLAG_CF) != 0) {
        Status = STATUS_FIRMWARE_ERROR;
        goto PcatGetDateEnd;
    }

    *Year = BCD_TO_BINARY((RealModeContext.Ecx >> 8) & 0xFF) * 100;
    *Year += BCD_TO_BINARY(RealModeContext.Ecx & 0xFF);
    *Month = BCD_TO_BINARY((RealModeContext.Edx >> 8) & 0xFF);
    *Day = BCD_TO_BINARY(RealModeContext.Edx & 0xFF);
    Status = STATUS_SUCCESS;

PcatGetDateEnd:
    FwpRealModeDestroyBiosCallContext(&RealModeContext);
    return Status;
}

KSTATUS
FwpPcatGetTime (
    PULONG Hour,
    PULONG Minute,
    PULONG Second
    )

/*++

Routine Description:

    This routine uses the BIOS to read the current time.

Arguments:

    Hour - Supplies a pointer where the hour will be returned on success.

    Minute - Supplies a pointer where the minute will be returned on success.

    Second - Supplies a pointer where the second will be returned on success.

Return Value:

    STATUS_SUCCESS if the operation completed successfully.

    STATUS_FIRMWARE_ERROR if the BIOS returned an error.

    Other error codes.

--*/

{

    REAL_MODE_CONTEXT RealModeContext;
    KSTATUS Status;

    //
    // Create a standard BIOS call context.
    //

    Status = FwpRealModeCreateBiosCallContext(&RealModeContext,
                                              BIOS_TIME_SERVICES);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // Int 0x1A function 4 reads the real time clock data. Ah takes the
    // function number. On return, CH contains the century (19 or 20), CL
    // contains the year, DH contains the month, and DL contains the day, all
    // in binary coded decimal.
    //

    RealModeContext.Eax = INT1A_READ_RTC_TIME << 8;

    //
    // Execute the firmware call.
    //

    FwpRealModeExecute(&RealModeContext);

    //
    // Check for an error.
    //

    if ((RealModeContext.Eflags & IA32_EFLAG_CF) != 0) {
        Status = STATUS_FIRMWARE_ERROR;
        goto PcatGetDateEnd;
    }

    *Hour = BCD_TO_BINARY((RealModeContext.Ecx >> 8) & 0xFF);
    *Minute = BCD_TO_BINARY(RealModeContext.Ecx & 0xFF);
    *Second = BCD_TO_BINARY((RealModeContext.Edx >> 8) & 0xFF);
    Status = STATUS_SUCCESS;

PcatGetDateEnd:
    FwpRealModeDestroyBiosCallContext(&RealModeContext);
    return Status;
}

