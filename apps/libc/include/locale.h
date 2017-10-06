/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    locale.h

Abstract:

    This header contains locale-specific definitions.

Author:

    Evan Green 22-Jul-2013

--*/

#ifndef _LOCALE_H
#define _LOCALE_H

//
// ------------------------------------------------------------------- Includes
//

#include <libcbase.h>
#include <stddef.h>

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// Define the various locale categories.
//

#define LC_ALL 1

//
// The collate category affects the behavior of regular expressions and the
// collation functions.
//

#define LC_COLLATE 2

//
// The ctype category affects the behavior of regular expressions, character
// classification, character conversion functions and wide-character functions.
//

#define LC_CTYPE 3

//
// The messages category affects what strings are expected and given by
// commands and utilities as affirmative or negative responses, and the content
// of messages.
//

#define LC_MESSAGES 4

//
// The monetary category affects the behavior of functions that handle
// monetary values.
//

#define LC_MONETARY 5

//
// The numeric category affects the radix character for the formatted input/
// output functions and the string conversion functions.
//

#define LC_NUMERIC 6

//
// The time category affects the behavior of time conversion functions.
//

#define LC_TIME 7

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores numeric conventions for the current locale.

Members:

    currency_symbol - Stores a pointer to a string containing the locale's
        currency symbol.

    decimal_point - Stores the radix character used to format non-monetary
        quantities.

    frac_digits - Stores the number of fractional digits (those after the
        decimal point) to be displayed in a formatted monetary quantity.

    grouping - Stores a pointer to a string whose elements taken as one-byte
        integer values indicate the size fo each group of digits in
        formatted non-monetary quantities.

    int_curr_symbol - Stores a pointer to a string store the international
        currency symbol applicable to the current locale.

    int_frac_digits - Stores the number of fractional digits (those after the
        decimal point) to be displayed in an internationally formatted
        monetary quantity.

    int_n_cs_precedes - Stores a 1 or 0 if the int_curr_symbol respectively
        precedes or succeeds the value for a negative internationally
        formatted monetary quantity.

    int_n_sep_by_space - Stores a value indicating the separation of
        int_curr_symbol, the sign string, and the value for a negative
        internationally formatted monetary quantity.

    int_n_sign_posn - Stores a value indicating the positioning of the
        negative_sign for a negative internationally formatted monetary
        quantity.

    int_p_cs_precedes - Stores a 1 or 0 if the int_curr_symbol respectively
        precedes or succeeds the value for a non-negative internally formatted
        monetary quantity.

    int_p_sep_by_space - Stores a value indicating the separation of the
        int_curr_symbol, the sign string, and the value for a non-negative
        internationally formatted monetary quantity.

    int_p_sign_posn - Stores a value indicating the position of the
        positive_sign for a non-negative internationally formatted monetary
        quantity.

    mon_decimal_point - Stores the radix character used to format monetary
        quantities.

    mon_grouping - Stores a pointer to a string whose elements taken as one-
        byte integers indicate the size of each group of digits in formatted
        monetary quantities.

    mon_thousands_sep - Stores the separator for groups of digits before the
        decimal point in formatted monetary quantities.

    negative_sign - Stores a pointer to the string used to indicate a negative
        valued formatted monetary quantity.

    n_cs_precedes - Stores a 1 if the currency_symbol precedes the value for a
        negative formatted monetary quantity. Set to 0 if the symbol succeeds
        the value.

    n_sep_by_space - Stores a value indicating the separation of the
        currency_symbol, the sign string, and the value for a negative
        formatted monetary quantity.

    n_sign_posn - Stores a value indicating the positioning of the
        negative_sign for a negative formatted monetary quantity.

    positive_sign - Stores a pointer to a string used to indicate a
        non-negative valued formatted monetary quantity.

    p_cs_precedes - Stores a 1 if the currency_symbol precedes the value for a
        non-negative formatted monetary quantity. Set to 0 if the symbol
        succeeds the value.

    p_sep_by_space - Stores a value indicating the separation of the
        currency_symbol, the sign string, and the value for a non-negative
        formatted monetary quantity.

    p_sign_posn - Stores a value indicating the positioning of the
        positive_sign for a non-negative formatted monetary quantity.

    thousands_sep - Stores a pointer to a string containing the character used
        to separate groups of digits before the decimal point character in
        formatted non-monetary quantities.

--*/

struct lconv {
    char *currency_symbol;
    char *decimal_point;
    char frac_digits;
    char *grouping;
    char *int_curr_symbol;
    char int_frac_digits;
    char int_n_cs_precedes;
    char int_n_sep_by_space;
    char int_n_sign_posn;
    char int_p_cs_precedes;
    char int_p_sep_by_space;
    char int_p_sign_posn;
    char *mon_decimal_point;
    char *mon_grouping;
    char *mon_thousands_sep;
    char *negative_sign;
    char n_cs_precedes;
    char n_sep_by_space;
    char n_sign_posn;
    char *positive_sign;
    char p_cs_precedes;
    char p_sep_by_space;
    char p_sign_posn;
    char *thousands_sep;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
struct lconv *
localeconv (
    void
    );

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

LIBC_API
char *
setlocale (
    int Category,
    const char *Locale
    );

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

#ifdef __cplusplus

}

#endif
#endif

