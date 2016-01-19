/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    time.h

Abstract:

    This header contains time related definitions, and the select function for
    whatever reason.

Author:

    Evan Green 3-May-2013

--*/

#ifndef _SYS_TIME_H
#define _SYS_TIME_H

//
// ------------------------------------------------------------------- Includes
//

#include <sys/select.h>

//
// --------------------------------------------------------------------- Macros
//

//
// These convenience macros operate on struct timeval pointers.
//

#define timerisset(_TimeVal) \
    (((_TimeVal)->tv_sec != 0) || ((_TimeVal)->tv_usec != 0))

#define timerclear(_TimeVal) \
    ((_TimeVal)->tv_sec = (_TimeVal)->tv_usec = 0)

//
// This macro compares two timeval structures. Operator is an actual C
// operator, like <, >, or ==. The operators <= and >= do not work properly
// with this macro.
//

#define timercmp(_TimeVal1, _TimeVal2, _Operator) \
    (((_TimeVal1)->tv_sec == (_TimeVal2)->tv_sec) ? \
     ((_TimeVal1)->tv_usec _Operator (_TimeVal2)->tv_usec) : \
     ((_TimeVal1)->tv_sec _Operator (_TimeVal2)->tv_sec))

#define timeradd(_TimeVal1, _TimeVal2, _Result)                             \
    do {                                                                    \
        (_Result)->tv_sec = (_TimeVal1)->tv_sec + (_TimeVal2)->tv_sec;      \
        (_Result)->tv_usec = (_TimeVal1)->tv_usec + (_TimeVal2)->tv_usec;   \
        while ((_Result)->tv_usec >= 1000000) {                             \
            (_Result)->tv_sec += 1;                                         \
            (_Result)->tv_usec -= 1000000;                                  \
        }                                                                   \
                                                                            \
    } while (0)

#define timersub(_TimeVal1, _TimeVal2, _Result)                             \
    do {                                                                    \
        (_Result)->tv_sec = (_TimeVal1)->tv_sec - (_TimeVal2)->tv_sec;      \
        (_Result)->tv_usec = (_TimeVal1)->tv_usec - (_TimeVal2)->tv_usec;   \
        while ((_Result)->tv_usec < 0) {                                    \
            (_Result)->tv_sec -= 1;                                         \
            (_Result)->tv_usec += 1000000;                                  \
        }                                                                   \
                                                                            \
    } while (0)

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
int
gettimeofday (
    struct timeval *Time,
    void *UnusedParameter
    );

/*++

Routine Description:

    This routine returns the current time in terms of seconds from the Epoch,
    midnight on January 1, 1970 GMT. The timezone is always GMT.

Arguments:

    Time - Supplies a pointer where the result will be returned.

    UnusedParameter - Supplies an unused parameter provided for legacy reasons.
        It used to store the current time zone.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

LIBC_API
int
settimeofday (
    const struct timeval *NewTime,
    void *UnusedParameter
    );

/*++

Routine Description:

    This routine sets the current time in terms of seconds from the Epoch,
    midnight on January 1, 1970 GMT. The timezone is always GMT. The caller
    must have appropriate privileges to set the system time.

Arguments:

    NewTime - Supplies a pointer where the result will be returned.

    UnusedParameter - Supplies an unused parameter provided for legacy reasons.
        It used to provide the current time zone.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

LIBC_API
int
utimes (
    const char *Path,
    const struct timeval Times[2]
    );

/*++

Routine Description:

    This routine sets the access and modification times of the given file. The
    effective user ID of the process must match the owner of the file, or the
    process must have appropriate privileges.

Arguments:

    Path - Supplies a pointer to the path of the file to change times for.

    Times - Supplies an optional array of time value structures containing the
        access and modification times to set.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

LIBC_API
int
lutimes (
    const char *Path,
    const struct timeval Times[2]
    );

/*++

Routine Description:

    This routine sets the access and modification times of the given file. The
    effective user ID of the process must match the owner of the file, or the
    process must have appropriate privileges. The only difference between this
    function and utimes is that if the path references a symbolic link, the
    times of the link itself will be changed rather than the file to which
    it refers.

Arguments:

    Path - Supplies a pointer to the path of the file to change times for.

    Times - Supplies an optional array of time value structures containing the
        access and modification times to set.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

LIBC_API
int
futimes (
    int File,
    const struct timeval Times[2]
    );

/*++

Routine Description:

    This routine sets the access and modification times of the given file. The
    effective user ID of the process must match the owner of the file, or the
    process must have appropriate privileges.

Arguments:

    File - Supplies the open file descriptor of the file to change the access
        and modification times for.

    Times - Supplies an optional array of time value structures containing the
        access and modification times to set.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

#ifdef __cplusplus

}

#endif
#endif

