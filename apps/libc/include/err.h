/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    err.h

Abstract:

    This header contains definitions for the old err/warn functions.

Author:

    Evan Green 25-Jul-2016

--*/

#ifndef _ERR_H
#define _ERR_H

//
// ------------------------------------------------------------------- Includes
//

#include <libcbase.h>
#include <stdarg.h>
#include <stddef.h>

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
void
err (
    int ExitCode,
    const char *Format,
    ...
    );

/*++

Routine Description:

    This routine prints the program name, the given printf-style formatted
    string, and the string of the current errno, separated by a colon and a
    space. This routine then exits the current program with the given exit
    value.

Arguments:

    ExitCode - Supplies the value to pass to exit.

    Format - Supplies the printf style format string.

    ... - Supplies the remaining arguments to the string.

Return Value:

    This routine does not return, as it calls exit.

--*/

LIBC_API
void
verr (
    int ExitCode,
    const char *Format,
    va_list Arguments
    );

/*++

Routine Description:

    This routine prints the program name, the given printf-style formatted
    string, and the string of the current errno, separated by a colon and a
    space. This routine then exits the current program with the given exit
    value.

Arguments:

    ExitCode - Supplies the value to pass to exit.

    Format - Supplies the printf style format string.

    Arguments - Supplies the remaining arguments to the string.

Return Value:

    This routine does not return, as it calls exit.

--*/

LIBC_API
void
errx (
    int ExitCode,
    const char *Format,
    ...
    );

/*++

Routine Description:

    This routine prints the program name, and the given printf-style formatted
    string, separated by a colon and a space. This routine then exits the
    current program with the given exit value.

Arguments:

    ExitCode - Supplies the value to pass to exit.

    Format - Supplies the printf style format string.

    ... - Supplies the remaining arguments to the string.

Return Value:

    This routine does not return, as it calls exit.

--*/

LIBC_API
void
verrx (
    int ExitCode,
    const char *Format,
    va_list Arguments
    );

/*++

Routine Description:

    This routine prints the program name, and the given printf-style formatted
    string, separated by a colon and a space. This routine then exits the
    current program with the given exit value.

Arguments:

    ExitCode - Supplies the value to pass to exit.

    Format - Supplies the printf style format string.

    Arguments - Supplies the remaining arguments to the string.

Return Value:

    This routine does not return, as it calls exit.

--*/

LIBC_API
void
warn (
    const char *Format,
    ...
    );

/*++

Routine Description:

    This routine prints the program name, the given printf-style formatted
    string, and the string of the current errno, separated by a colon and a
    space.

Arguments:

    Format - Supplies the printf style format string.

    ... - Supplies the remaining arguments to the string.

Return Value:

    None.

--*/

LIBC_API
void
vwarn (
    const char *Format,
    va_list Arguments
    );

/*++

Routine Description:

    This routine prints the program name, the given printf-style formatted
    string, and the string of the current errno, separated by a colon and a
    space.

Arguments:

    Format - Supplies the printf style format string.

    Arguments - Supplies the remaining arguments to the string.

Return Value:

    None.

--*/

LIBC_API
void
warnx (
    const char *Format,
    ...
    );

/*++

Routine Description:

    This routine prints the program name, and the given printf-style formatted
    string, separated by a colon and a space.

Arguments:

    Format - Supplies the printf style format string.

    ... - Supplies the remaining arguments to the string.

Return Value:

    None.

--*/

LIBC_API
void
vwarnx (
    const char *Format,
    va_list Arguments
    );

/*++

Routine Description:

    This routine prints the program name, and the given printf-style formatted
    string, separated by a colon and a space.

Arguments:

    Format - Supplies the printf style format string.

    Arguments - Supplies the remaining arguments to the string.

Return Value:

    None.

--*/

#ifdef __cplusplus

}

#endif
#endif

