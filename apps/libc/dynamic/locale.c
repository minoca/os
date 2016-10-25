/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    locale.c

Abstract:

    This module implements locale functionality for the C library.

Author:

    Evan Green 22-Jul-2013

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <assert.h>
#include <limits.h>
#include <locale.h>

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
// Store the current locale information, which is initialized to the "C"
// locale.
//

struct lconv ClLocaleInformation = {
    "",
    ".",
    CHAR_MAX,
    "",
    "",
    CHAR_MAX,
    CHAR_MAX,
    CHAR_MAX,
    CHAR_MAX,
    CHAR_MAX,
    CHAR_MAX,
    CHAR_MAX,
    "",
    "",
    "",
    "",
    CHAR_MAX,
    CHAR_MAX,
    CHAR_MAX,
    "",
    CHAR_MAX,
    CHAR_MAX,
    CHAR_MAX,
    ""
};

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
struct lconv *
localeconv (
    void
    )

/*++

Routine Description:

    This routine returns a pointer to a structure containing the numeric and
    monetary customs of the current locale. String members of the structure
    may point to "" to indicate that the value is not available in the current
    locale or is of zero length. Character members are non-negative numbers,
    any of which can be CHAR_MAX to indicate that the value is not available
    in the current locale.

Arguments:

    None.

Return Value:

    Returns a pointer to the filled in structure. The caller must not attempt
    to modify or free the returned structure. The structure returned or
    storage areas pointed to by the structure may be overwritten or invalidated
    by subsequent calls to setlocale or uselocale which affect the categories
    LC_ALL, LC_MONETARY, or LC_NUMERIC.

--*/

{

    return &ClLocaleInformation;
}

LIBC_API
char *
setlocale (
    int Category,
    const char *Locale
    )

/*++

Routine Description:

    This routine sets or returns the appropriate piece of the program's
    locale.

Arguments:

    Category - Supplies the category of the locale to set. See LC_* definitions.

    Locale - Supplies an optional pointer to a string containing the locale to
        set. If NULL is supplied, then the current locale will be returned and
        left unchanged.

Return Value:

    Returns a pointer to a string containing the name of the original locale
    for the specified category. This string must not be modified, and may be
    overwritten by subsequent calls to this routine.

    NULL on failure.

--*/

{

    //
    // At some point, consider implementing some support for locales.
    //

    if (Locale != NULL) {
        if ((strcmp(Locale, "C") != 0) &&
            (strcmp(Locale, "POSIX") != 0) &&
            (strcmp(Locale, "") != 0)) {

            return NULL;
        }
    }

    return "C";
}

//
// --------------------------------------------------------- Internal Functions
//

