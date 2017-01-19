/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    err.c

Abstract:

    This module implements support for the old err/warn functions.

Author:

    Evan Green 25-Jul-2016

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

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

LIBC_API
void
err (
    int ExitCode,
    const char *Format,
    ...
    )

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

{

    va_list Arguments;

    va_start(Arguments, Format);
    verr(ExitCode, Format, Arguments);
    va_end(Arguments);
    return;
}

LIBC_API
void
verr (
    int ExitCode,
    const char *Format,
    va_list Arguments
    )

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

{

    PPROCESS_ENVIRONMENT Environment;

    Environment = OsGetCurrentEnvironment();
    fprintf(stderr, "%s: ", Environment->Arguments[0]);
    if (Format != NULL) {
        vfprintf(stderr, Format, Arguments);
        fprintf(stderr, ": ");
    }

    fprintf(stderr, "%s\n", strerror(errno));
    exit(ExitCode);
    return;
}

LIBC_API
void
errx (
    int ExitCode,
    const char *Format,
    ...
    )

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

{

    va_list Arguments;

    va_start(Arguments, Format);
    verrx(ExitCode, Format, Arguments);
    va_end(Arguments);
    return;
}

LIBC_API
void
verrx (
    int ExitCode,
    const char *Format,
    va_list Arguments
    )

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

{

    PPROCESS_ENVIRONMENT Environment;

    Environment = OsGetCurrentEnvironment();
    fprintf(stderr, "%s: ", Environment->Arguments[0]);
    if (Format != NULL) {
        vfprintf(stderr, Format, Arguments);
        fprintf(stderr, ": ");
    }

    fprintf(stderr, "\n");
    exit(ExitCode);
    return;
}

LIBC_API
void
warn (
    const char *Format,
    ...
    )

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

{

    va_list Arguments;

    va_start(Arguments, Format);
    vwarn(Format, Arguments);
    va_end(Arguments);
    return;
}

LIBC_API
void
vwarn (
    const char *Format,
    va_list Arguments
    )

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

{

    PPROCESS_ENVIRONMENT Environment;

    Environment = OsGetCurrentEnvironment();
    fprintf(stderr, "%s: ", Environment->Arguments[0]);
    if (Format != NULL) {
        vfprintf(stderr, Format, Arguments);
        fprintf(stderr, ": ");
    }

    fprintf(stderr, "%s\n", strerror(errno));
    return;
}

LIBC_API
void
warnx (
    const char *Format,
    ...
    )

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

{

    va_list Arguments;

    va_start(Arguments, Format);
    vwarnx(Format, Arguments);
    va_end(Arguments);
    return;
}

LIBC_API
void
vwarnx (
    const char *Format,
    va_list Arguments
    )

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

{

    PPROCESS_ENVIRONMENT Environment;

    Environment = OsGetCurrentEnvironment();
    fprintf(stderr, "%s: ", Environment->Arguments[0]);
    if (Format != NULL) {
        vfprintf(stderr, Format, Arguments);
        fprintf(stderr, ": ");
    }

    fprintf(stderr, "\n");
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

