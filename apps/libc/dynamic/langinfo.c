/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    langinfo.c

Abstract:

    This module implements support for getting locale-specific messages.

Author:

    Evan Green 21-Jan-2015

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <langinfo.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the maximum valid langinfo item value.
//

#define LANGINFO_MAX D_MD_ORDER

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

PSTR ClPosixLangInfo[LANGINFO_MAX + 1] = {
    "UTF-8",
    "%b %a %d %k:%M%S %Z %Y",
    "%b %a %d",
    "%H:%M",
    "%I:%M:%S %p",
    "AM",
    "PM",
    "Sunday",
    "Monday",
    "Tuesday",
    "Wednesday",
    "Thursday",
    "Friday",
    "Saturday",
    "Sun",
    "Mon",
    "Tue",
    "Wed",
    "Thu",
    "Fri",
    "Sat",
    "January",
    "February",
    "March",
    "April",
    "May",
    "June",
    "July",
    "August",
    "September",
    "October",
    "November",
    "December",
    "Jan",
    "Feb",
    "Mar",
    "Apr",
    "May",
    "Jun",
    "Jul",
    "Aug",
    "Sep",
    "Oct",
    "Nov",
    "Dec",
    "",
    "",
    "",
    "",
    "",
    ".",
    "",
    "^[yY]",
    "^[nN]",
    "$",
    "md"
};

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
char *
nl_langinfo (
    nl_item Item
    )

/*++

Routine Description:

    This routine returns a pointer to a string containing the relevant message
    for the given item.

Arguments:

    Item - Supplies the item to get a string for. See definitions in
        langinfo.h.

Return Value:

    Returns a pointer to a string for the given item. If item is invalid,
    returns a pointer to an empty string. If language information is not
    defined, returns the string from the POSIX locale. The memory returned here
    may be overwritten by subsequent calls.

--*/

{

    if (Item > LANGINFO_MAX) {
        return NULL;
    }

    return ClPosixLangInfo[Item];
}

//
// --------------------------------------------------------- Internal Functions
//

