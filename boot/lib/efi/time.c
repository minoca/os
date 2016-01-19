/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    time.c

Abstract:

    This module implements support for time-based EFI functionality.

Author:

    Evan Green 10-Apr-2014

Environment:

    Boot

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel.h>
#include <minoca/uefi/uefi.h>
#include "firmware.h"
#include "bootlib.h"
#include "efisup.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
BopEfiGetCurrentTime (
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
    EFI_STATUS EfiStatus;
    EFI_TIME EfiTime;
    KSTATUS Status;

    EfiStatus = BopEfiGetTime(&EfiTime, NULL);
    if (EFI_ERROR(EfiStatus)) {
        return BopEfiStatusToKStatus(EfiStatus);
    }

    RtlZeroMemory(&CalendarTime, sizeof(CALENDAR_TIME));
    CalendarTime.Year = EfiTime.Year;
    CalendarTime.Month = EfiTime.Month - 1;
    CalendarTime.Day = EfiTime.Day;
    CalendarTime.Hour = EfiTime.Hour;
    CalendarTime.Minute = EfiTime.Minute;
    CalendarTime.Second = EfiTime.Second;
    CalendarTime.Nanosecond = EfiTime.Nanosecond;
    Status = RtlCalendarTimeToSystemTime(&CalendarTime, Time);
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

