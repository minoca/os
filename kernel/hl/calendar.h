/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    calendar.h

Abstract:

    This header contains definitions for hardware calendar timer support.

Author:

    Evan Green 20-Sep-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define calendar timer flags.
//

//
// This flag is set if the timer has been initialized.
//

#define CALENDAR_TIMER_FLAG_INITIALIZED 0x00000001

//
// This flag is set if the initialization failed.
//

#define CALENDAR_TIMER_FLAG_FAILED 0x00000002

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// Store the list of registered calendar timers.
//

extern LIST_ENTRY HlCalendarTimers;

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
HlpCalendarTimerRegisterHardware (
    PCALENDAR_TIMER_DESCRIPTION TimerDescription
    );

/*++

Routine Description:

    This routine is called to register a new calendar timer with the system.

Arguments:

    TimerDescription - Supplies a pointer to a structure describing the new
        calendar timer.

Return Value:

    Status code.

--*/

KSTATUS
HlpArchInitializeCalendarTimers (
    VOID
    );

/*++

Routine Description:

    This routine performs architecture-specific initialization for the
    calendar timer subsystem.

Arguments:

    None.

Return Value:

    Status code.

--*/

