/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    langinfo.h

Abstract:

    This header contains definitions for retrieving specific language
    information strings.

Author:

    Evan Green 21-Jan-2015

--*/

#ifndef _LANGINFO_H
#define _LANGINFO_H

//
// ------------------------------------------------------------------- Includes
//

#include <sys/types.h>

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// Current codeset name
//

#define CODESET 0

//
// Date and time formatting string
//

#define D_T_FMT 1

//
// Date formatting string
//

#define D_FMT 2

//
// Time formatting string
//

#define T_FMT 3

//
// AM/PM time formatting string
//

#define T_FMT_AMPM 4

//
// Ante Meridian affix
//

#define AM_STR 5

//
// Post Meridian affix
//

#define PM_STR 6

//
// Weekday names
//

#define DAY_1 7
#define DAY_2 8
#define DAY_3 9
#define DAY_4 10
#define DAY_5 11
#define DAY_6 12
#define DAY_7 13

//
// Abbreviated weekday names
//

#define ABDAY_1 14
#define ABDAY_2 15
#define ABDAY_3 16
#define ABDAY_4 17
#define ABDAY_5 18
#define ABDAY_6 19
#define ABDAY_7 20

//
// Month names
//

#define MON_1 21
#define MON_2 22
#define MON_3 23
#define MON_4 24
#define MON_5 25
#define MON_6 26
#define MON_7 27
#define MON_8 28
#define MON_9 29
#define MON_10 30
#define MON_11 31
#define MON_12 32

//
// Abbreviated month names
//

#define ABMON_1 33
#define ABMON_2 34
#define ABMON_3 35
#define ABMON_4 36
#define ABMON_5 37
#define ABMON_6 38
#define ABMON_7 39
#define ABMON_8 40
#define ABMON_9 41
#define ABMON_10 42
#define ABMON_11 43
#define ABMON_12 44

//
// Era description segments
//

#define ERA 45

//
// Era date format string
//

#define ERA_D_FMT 46

//
// Era date and time format string
//

#define ERA_D_T_FMT 47

//
// Era time format string
//

#define ERA_T_FMT 48

//
// Alternative digit symbols
//

#define ALT_DIGITS 49

//
// Radix character
//

#define RADIXCHAR 50

//
// Thousands separator
//

#define THOUSEP 51

//
// Affirmative response expression
//

#define YESEXPR 52

//
// Negative response expression
//

#define NOEXPR 53

//
// Affirmative response for yes/no queries
//

#define YESSTR 54

//
// Negative response for yes/no queries
//

#define NOSTR 55

//
// Currency symbol
//

#define CRNCYSTR 56

//
// Month/day order
//

#define D_MD_ORDER 57

//
// ------------------------------------------------------ Data Type Definitions
//

typedef int nl_item;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
char *
nl_langinfo (
    nl_item Item
    );

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

#ifdef __cplusplus

}

#endif
#endif

