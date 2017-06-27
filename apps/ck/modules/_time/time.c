/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    time.c

Abstract:

    This module implements OS-level time functionality.

Author:

    Evan Green 5-Jun-2017

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "timep.h"

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
CkpStrftime (
    PCK_VM Vm
    );

VOID
CkpTime (
    PCK_VM Vm
    );

VOID
CkpMktime (
    PCK_VM Vm
    );

VOID
CkpGmtime (
    PCK_VM Vm
    );

VOID
CkpLocaltime (
    PCK_VM Vm
    );

VOID
CkpTzset (
    PCK_VM Vm
    );

VOID
CkpClockGetres (
    PCK_VM Vm
    );

VOID
CkpClockGettime (
    PCK_VM Vm
    );

VOID
CkpClockSettime (
    PCK_VM Vm
    );

VOID
CkpSleep (
    PCK_VM Vm
    );

VOID
CkpDictToTm (
    PCK_VM Vm,
    INTN StackIndex,
    struct tm *Tm
    );

VOID
CkpTmToDict (
    PCK_VM Vm,
    const struct tm *Tm
    );

VOID
CkpSetTimeVariables (
    PCK_VM Vm
    );

//
// -------------------------------------------------------------------- Globals
//

CK_VARIABLE_DESCRIPTION CkTimeModuleValues[] = {

#ifdef CLOCK_REALTIME
    {CkTypeInteger, "CLOCK_REALTIME", NULL, CLOCK_REALTIME},
#endif

#ifdef CLOCK_MONOTONIC
    {CkTypeInteger, "CLOCK_MONOTONIC", NULL, CLOCK_MONOTONIC},
#endif

#ifdef CLOCK_PROCESS_CPUTIME_ID
    {CkTypeInteger, "CLOCK_PROCESS_CPUTIME_ID", NULL, CLOCK_PROCESS_CPUTIME_ID},
#endif

#ifdef CLOCK_THREAD_CPUTIME_ID
    {CkTypeInteger, "CLOCK_THREAD_CPUTIME_ID", NULL, CLOCK_THREAD_CPUTIME_ID},
#endif

#ifdef CLOCK_MONOTONIC_RAW
    {CkTypeInteger, "CLOCK_MONOTONIC_RAW", NULL, CLOCK_MONOTONIC_RAW},
#endif

#ifdef CLOCK_REALTIME_COARSE
    {CkTypeInteger, "CLOCK_REALTIME_COARSE", NULL, CLOCK_REALTIME_COARSE},
#endif

#ifdef CLOCK_MONOTONIC_COARSE
    {CkTypeInteger, "CLOCK_MONOTONIC_COARSE", NULL, CLOCK_MONOTONIC_COARSE},
#endif

#ifdef CLOCK_BOOTTIME
    {CkTypeInteger, "CLOCK_BOOTTIME", NULL, CLOCK_BOOTTIME},
#endif

    {CkTypeFunction, "clock_getres", CkpClockGetres, 1},
    {CkTypeFunction, "clock_gettime", CkpClockGettime, 1},
    {CkTypeFunction, "clock_settime", CkpClockSettime, 2},
    {CkTypeFunction, "sleep", CkpSleep, 2},
    {CkTypeFunction, "strftime", CkpStrftime, 2},
    {CkTypeFunction, "time", CkpTime, 0},
    {CkTypeFunction, "mktime", CkpMktime, 1},
    {CkTypeFunction, "gmtime", CkpGmtime, 1},
    {CkTypeFunction, "localtime", CkpLocaltime, 1},
    {CkTypeFunction, "tzset", CkpTzset, 0},
    {CkTypeInvalid, NULL, NULL, 0}
};

//
// ------------------------------------------------------------------ Functions
//

BOOL
CkPreloadTimeModule (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine preloads the _time module. It is called to make the presence
    of the os module known in cases where the module is statically linked.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    return CkPreloadForeignModule(Vm, "_time", NULL, NULL, CkpTimeModuleInit);
}

VOID
CkpTimeModuleInit (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine populates the _time module namespace.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    CkPushString(Vm, "TimeError", 9);
    CkGetVariable(Vm, 0, "Exception");
    CkPushClass(Vm, 0, 0);
    CkSetVariable(Vm, 0, "TimeError");

    //
    // Register the functions and definitions.
    //

    CkDeclareVariables(Vm, 0, CkTimeModuleValues);
    CkpSetTimeVariables(Vm);
    return;
}

VOID
CkpTimeRaiseError (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine raises an error associated with the current errno value.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    This routine does not return. The process exits.

--*/

{

    INT Error;
    PCSTR ErrorString;

    Error = errno;
    ErrorString = strerror(Error);

    //
    // Create an OsError exception.
    //

    CkPushModule(Vm, "_time");
    CkGetVariable(Vm, -1, "TimeError");
    CkPushString(Vm, ErrorString, strlen(ErrorString));
    CkCall(Vm, 1);

    //
    // Execute instance.errno = Error.
    //

    CkPushValue(Vm, -1);
    CkPushString(Vm, "errno", 5);
    CkPushInteger(Vm, Error);
    CkCallMethod(Vm, "__set", 2);
    CkStackPop(Vm);

    //
    // Raise the exception.
    //

    CkRaiseException(Vm, -1);
    return;
}

VOID
CkpStrftime (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine implements the strftime interface to the C library. It takes
    in a format string and tm dict, and returns a formatted time string.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    CHAR Buffer[1024];
    PCSTR Format;
    size_t Size;
    struct tm Tm;

    memset(&Tm, 0, sizeof(Tm));
    if (!CkCheckArguments(Vm, 2, CkTypeString, CkTypeDict)) {
        return;
    }

    Format = CkGetString(Vm, 1, NULL);
    CkpDictToTm(Vm, 2, &Tm);
    Size = strftime(Buffer, sizeof(Buffer), Format, &Tm);
    if (Size <= 0) {
        CkReturnString(Vm, "", 0);

    } else {
        CkReturnString(Vm, Buffer, Size);
    }

    return;
}

VOID
CkpTime (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine implements the interface to the time() C library function. It
    returns the number of seconds since 1970.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    CkReturnInteger(Vm, time(NULL));
    return;
}

VOID
CkpMktime (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine implements the interface to the C library mktime function. It
    takes a tm dictionary in local time and returns seconds since 1970.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    struct tm Time;

    if (!CkCheckArguments(Vm, 1, CkTypeDict)) {
        return;
    }

    memset(&Time, 0, sizeof(Time));
    CkpDictToTm(Vm, 1, &Time);
    CkReturnInteger(Vm, mktime(&Time));
    return;
}

VOID
CkpGmtime (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine implements the interface to the C library gmtime function. It
    takes a timestamp in seconds from 1970 and returns a tm dictionary in UTC
    time.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    time_t Time;
    struct tm *TimeFields;

    if (!CkCheckArguments(Vm, 1, CkTypeInteger)) {
        return;
    }

    Time = CkGetInteger(Vm, 1);
    TimeFields = gmtime(&Time);
    CkpTmToDict(Vm, TimeFields);
    CkStackReplace(Vm, 0);
    return;
}

VOID
CkpLocaltime (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine implements the interface to the C library localtime function.
    It takes a timestamp in seconds from 1970 and returns a tm dictionary in
    local time.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    time_t Time;
    struct tm *TimeFields;

    if (!CkCheckArguments(Vm, 1, CkTypeInteger)) {
        return;
    }

    Time = CkGetInteger(Vm, 1);
    TimeFields = localtime(&Time);
    CkpTmToDict(Vm, TimeFields);
    CkStackReplace(Vm, 0);
    return;
}

VOID
CkpTzset (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine calls the C library tzset function. It takes no arguments
    and returns nothing. It does update the globals in this module.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    tzset();
    CkpSetTimeVariables(Vm);
    CkReturnInteger(Vm, 0);
    return;
}

VOID
CkpClockGetres (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine implements the interface to the clock_getres function. It
    takes in an integer containing the "clock" to get, and returns
    [seconds, nanoseconds] resolution.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    struct timespec Resolution;

    if (!CkCheckArguments(Vm, 1, CkTypeInteger)) {
        return;
    }

    if (clock_getres(CkGetInteger(Vm, 1), &Resolution) != 0) {
        CkpTimeRaiseError(Vm);
        return;
    }

    CkPushList(Vm);
    CkPushInteger(Vm, Resolution.tv_sec);
    CkListSet(Vm, -2, 0);
    CkPushInteger(Vm, Resolution.tv_nsec);
    CkListSet(Vm, -2, 1);
    CkStackReplace(Vm, 0);
    return;
}

VOID
CkpClockGettime (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine implements the interface to the clock_gettime function. It
    takes in an integer containing the "clock" to get, and returns
    [seconds, nanoseconds].

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    struct timespec Time;

    if (!CkCheckArguments(Vm, 1, CkTypeInteger)) {
        return;
    }

    if (clock_gettime(CkGetInteger(Vm, 1), &Time) != 0) {
        CkpTimeRaiseError(Vm);
        return;
    }

    CkPushList(Vm);
    CkPushInteger(Vm, Time.tv_sec);
    CkListSet(Vm, -2, 0);
    CkPushInteger(Vm, Time.tv_nsec);
    CkListSet(Vm, -2, 1);
    CkStackReplace(Vm, 0);
    return;
}

VOID
CkpClockSettime (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine implements the interface to the clock_settime function. It
    takes in an integer containing the "clock" to set and a list containing the
    new [seconds, nanoseconds] to set.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    struct timespec Time;

    if (!CkCheckArguments(Vm, 2, CkTypeInteger, CkTypeList)) {
        return;
    }

    CkListGet(Vm, 2, 0);
    Time.tv_sec = CkGetInteger(Vm, -1);
    CkStackPop(Vm);
    CkListGet(Vm, 2, 1);
    Time.tv_nsec = CkGetInteger(Vm, -1);
    CkStackPop(Vm);
    if (clock_settime(CkGetInteger(Vm, 1), &Time) != 0) {
        CkpTimeRaiseError(Vm);
        return;
    }

    CkReturnInteger(Vm, 0);
    return;
}

VOID
CkpSleep (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine simply blocks the calling thread for the given amount of time.
    It takes two integers: seconds and nanoseconds. It returns 0 on success
    or -1 if the sleep was interrupted.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    struct timespec Timespec;

    if (!CkCheckArguments(Vm, 2, CkTypeInteger, CkTypeInteger)) {
        return;
    }

    Timespec.tv_sec = CkGetInteger(Vm, 1);
    Timespec.tv_nsec = CkGetInteger(Vm, 2);
    CkReturnInteger(Vm, nanosleep(&Timespec, NULL));
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
CkpDictToTm (
    PCK_VM Vm,
    INTN StackIndex,
    struct tm *Tm
    )

/*++

Routine Description:

    This routine converts a dict to a struct tm.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    StackIndex - Supplies the index of the dictionary to read.

    Tm - Supplies a pointer where the filled out tm structure will be returned.
        The caller should probably zero this first.

Return Value:

    None.

--*/

{

    if (StackIndex < 0) {
        StackIndex -= 1;
    }

    CkPushString(Vm, "tm_sec", 6);
    if (CkDictGet(Vm, StackIndex) != FALSE) {
        Tm->tm_sec = CkGetInteger(Vm, -1);
        CkStackPop(Vm);
    }

    CkPushString(Vm, "tm_min", 6);
    if (CkDictGet(Vm, StackIndex) != FALSE) {
        Tm->tm_min = CkGetInteger(Vm, -1);
        CkStackPop(Vm);
    }

    CkPushString(Vm, "tm_hour", 7);
    if (CkDictGet(Vm, StackIndex) != FALSE) {
        Tm->tm_hour = CkGetInteger(Vm, -1);
        CkStackPop(Vm);
    }

    CkPushString(Vm, "tm_mday", 7);
    if (CkDictGet(Vm, StackIndex) != FALSE) {
        Tm->tm_mday = CkGetInteger(Vm, -1);
        CkStackPop(Vm);
    }

    CkPushString(Vm, "tm_mon", 6);
    if (CkDictGet(Vm, StackIndex) != FALSE) {
        Tm->tm_mon = CkGetInteger(Vm, -1);
        CkStackPop(Vm);
    }

    CkPushString(Vm, "tm_year", 7);
    if (CkDictGet(Vm, StackIndex) != FALSE) {
        Tm->tm_year = CkGetInteger(Vm, -1);
        CkStackPop(Vm);
    }

    CkPushString(Vm, "tm_wday", 7);
    if (CkDictGet(Vm, StackIndex) != FALSE) {
        Tm->tm_wday = CkGetInteger(Vm, -1);
        CkStackPop(Vm);
    }

    CkPushString(Vm, "tm_yday", 7);
    if (CkDictGet(Vm, StackIndex) != FALSE) {
        Tm->tm_yday = CkGetInteger(Vm, -1);
        CkStackPop(Vm);
    }

    CkPushString(Vm, "tm_isdst", 8);
    if (CkDictGet(Vm, StackIndex) != FALSE) {
        Tm->tm_isdst = CkGetInteger(Vm, -1);
        CkStackPop(Vm);
    }

#ifdef HAVE_TM_NANOSECOND

    CkPushString(Vm, "tm_nanosecond", 13);
    if (CkDictGet(Vm, StackIndex) != FALSE) {
        Tm->tm_nanosecond = CkGetInteger(Vm, -1);
        CkStackPop(Vm);
    }

#endif

#ifdef HAVE_TM_GMTOFF

    CkPushString(Vm, "tm_gmtoff", 9);
    if (CkDictGet(Vm, StackIndex) != FALSE) {
        Tm->tm_gmtoff = CkGetInteger(Vm, -1);
        CkStackPop(Vm);
    }

#endif

#ifdef HAVE_TM_ZONE

    CkPushString(Vm, "tm_zone", 7);
    if (CkDictGet(Vm, StackIndex) != FALSE) {
        Tm->tm_zone = (char *)CkGetString(Vm, -1, NULL);
        CkStackPop(Vm);
    }

#endif

    return;
}

VOID
CkpTmToDict (
    PCK_VM Vm,
    const struct tm *Tm
    )

/*++

Routine Description:

    This routine converts struct tm to a dict. The new dict will be pushed
    onto the stack.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Tm - Supplies a pointer to the time structure to convert into a dictionary.

Return Value:

    None.

--*/

{

    CkPushDict(Vm);
    CkPushString(Vm, "tm_sec", 6);
    CkPushInteger(Vm, Tm->tm_sec);
    CkDictSet(Vm, -3);
    CkPushString(Vm, "tm_min", 6);
    CkPushInteger(Vm, Tm->tm_min);
    CkDictSet(Vm, -3);
    CkPushString(Vm, "tm_hour", 7);
    CkPushInteger(Vm, Tm->tm_hour);
    CkDictSet(Vm, -3);
    CkPushString(Vm, "tm_mday", 7);
    CkPushInteger(Vm, Tm->tm_mday);
    CkDictSet(Vm, -3);
    CkPushString(Vm, "tm_mon", 6);
    CkPushInteger(Vm, Tm->tm_mon);
    CkDictSet(Vm, -3);
    CkPushString(Vm, "tm_year", 7);
    CkPushInteger(Vm, Tm->tm_year);
    CkDictSet(Vm, -3);
    CkPushString(Vm, "tm_wday", 7);
    CkPushInteger(Vm, Tm->tm_wday);
    CkDictSet(Vm, -3);
    CkPushString(Vm, "tm_yday", 7);
    CkPushInteger(Vm, Tm->tm_yday);
    CkDictSet(Vm, -3);
    CkPushString(Vm, "tm_isdst", 8);
    CkPushInteger(Vm, Tm->tm_isdst);
    CkDictSet(Vm, -3);

#ifdef HAVE_TM_NANOSECOND

    CkPushString(Vm, "tm_nanosecond", 13);
    CkPushInteger(Vm, Tm->tm_nanosecond);
    CkDictSet(Vm, -3);

#endif

#ifdef HAVE_TM_GMTOFF

    CkPushString(Vm, "tm_gmtoff", 9);
    CkPushInteger(Vm, Tm->tm_gmtoff);
    CkDictSet(Vm, -3);

#endif

#ifdef HAVE_TM_ZONE

    CkPushString(Vm, "tm_zone", 7);
    CkPushString(Vm, Tm->tm_zone, strlen(Tm->tm_zone));
    CkDictSet(Vm, -3);

#endif

    return;
}

VOID
CkpSetTimeVariables (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine sets the module level variables in the _time module based on
    the values found in the C library.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    CkPushModule(Vm, "_time");
    CkPushInteger(Vm, daylight);
    CkSetVariable(Vm, 0, "daylight");
    CkPushInteger(Vm, timezone);
    CkSetVariable(Vm, 0, "timezone");
    CkPushList(Vm);
    CkPushString(Vm, tzname[0], strlen(tzname[0]));
    CkListSet(Vm, -2, 0);
    CkPushString(Vm, tzname[1], strlen(tzname[1]));
    CkListSet(Vm, -2, 1);
    CkSetVariable(Vm, 0, "tzname");
    return;
}

