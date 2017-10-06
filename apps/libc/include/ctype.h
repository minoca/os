/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    ctype.h

Abstract:

    This header contains character type definitions.

Author:

    Evan Green 18-Jun-2013

--*/

#ifndef _CTYPE_H
#define _CTYPE_H

//
// ------------------------------------------------------------------- Includes
//

#include <libcbase.h>

//
// --------------------------------------------------------------------- Macros
//

//
// This macro converts the given character into a lower case character.
//

#define _tolower(_Character) ((_Character) | 0x20)

//
// This macro converts the given character into an upper case character.
//

#define _toupper(_Character) ((_Character) & (~0x20))

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
isalnum (
    int Character
    );

/*++

Routine Description:

    This routine returns non-zero if the given character is alphanumeric.

Arguments:

    Character - Supplies the character to check.

Return Value:

    Returns non-zero if the given character is alphanumeric.

    0 if the character is not.

--*/

LIBC_API
int
isalpha (
    int Character
    );

/*++

Routine Description:

    This routine returns non-zero if the given character is alphabetic.

Arguments:

    Character - Supplies the character to check.

Return Value:

    Returns non-zero if the given character is alphabetic.

    0 if the character is not.

--*/

LIBC_API
int
isascii (
    int Character
    );

/*++

Routine Description:

    This routine returns non-zero if the given character is part of the ASCII
    character set.

Arguments:

    Character - Supplies the character to check.

Return Value:

    Returns non-zero if the given character is part of the ASCII set.

    0 if the character is not.

--*/

LIBC_API
int
isblank (
    int Character
    );

/*++

Routine Description:

    This routine returns non-zero if the given character is blank. In the
    default locale, this is just the space and tab characters.

Arguments:

    Character - Supplies the character to check.

Return Value:

    Returns non-zero if the given character is blank.

    0 if the character is not.

--*/

LIBC_API
int
iscntrl (
    int Character
    );

/*++

Routine Description:

    This routine returns non-zero if the given character is a control character.
    For the default locale, this is anything with a value less than that of
    the space character.

Arguments:

    Character - Supplies the character to check.

Return Value:

    Returns non-zero if the given character is alphabetic.

    0 if the character is not.

--*/

LIBC_API
int
isdigit (
    int Character
    );

/*++

Routine Description:

    This routine returns non-zero if the given character is a digit (zero
    through nine).

Arguments:

    Character - Supplies the character to check.

Return Value:

    Returns non-zero if the given character is a digit.

    0 if the character is not.

--*/

LIBC_API
int
isgraph (
    int Character
    );

/*++

Routine Description:

    This routine returns non-zero if the given character is graphical. In the
    default locale, this represents anything that is alphanumeric or
    punctuation.

Arguments:

    Character - Supplies the character to check.

Return Value:

    Returns non-zero if the given character is graphical.

    0 if the character is not.

--*/

LIBC_API
int
islower (
    int Character
    );

/*++

Routine Description:

    This routine returns non-zero if the given character is lower case.

Arguments:

    Character - Supplies the character to check.

Return Value:

    Returns non-zero if the given character is lower case.

    0 if the character is not.

--*/

LIBC_API
int
isprint (
    int Character
    );

/*++

Routine Description:

    This routine returns non-zero if the given character is printable. In the
    default locale, this is anything that is either alphabetic, punctuation, or
    the space character.

Arguments:

    Character - Supplies the character to check.

Return Value:

    Returns non-zero if the given character is printable.

    0 if the character is not.

--*/

LIBC_API
int
ispunct (
    int Character
    );

/*++

Routine Description:

    This routine returns non-zero if the given character is punctuation. In
    the default locale, this is anything that is not alphabetic, a digit,
    a control character, or a space.

Arguments:

    Character - Supplies the character to check.

Return Value:

    Returns non-zero if the given character is punctuation.

    0 if the character is not.

--*/

LIBC_API
int
isspace (
    int Character
    );

/*++

Routine Description:

    This routine returns non-zero if the given character is white space. In the
    default locale, this is the characters space, tab, newline (\n) carriage
    return (\r), form feed (\f), and vertical tab (\v).

Arguments:

    Character - Supplies the character to check.

Return Value:

    Returns non-zero if the given character is white space.

    0 if the character is not.

--*/

LIBC_API
int
isupper (
    int Character
    );

/*++

Routine Description:

    This routine returns non-zero if the given character is upper case.

Arguments:

    Character - Supplies the character to check.

Return Value:

    Returns non-zero if the given character is upper case.

    0 if the character is not.

--*/

LIBC_API
int
isxdigit (
    int Character
    );

/*++

Routine Description:

    This routine returns non-zero if the given character is a hexadecimal
    digit, which in the default locale is 0-9, a-f, and A-F.

Arguments:

    Character - Supplies the character to check.

Return Value:

    Returns non-zero if the given character is a hexadecimal digit.

    0 if the character is not.

--*/

LIBC_API
int
toascii (
    int Character
    );

/*++

Routine Description:

    This routine converts a character into the ASCII character set by lopping
    off all but the least significant seven bits.

Arguments:

    Character - Supplies the character to convert.

Return Value:

    Returns the ASCII portion of the character.

--*/

LIBC_API
int
toupper (
    int Character
    );

/*++

Routine Description:

    This routine returns converts the given character to upper case.

Arguments:

    Character - Supplies the character to convert.

Return Value:

    Returns the upper cased version of the character, or the character itself.

--*/

LIBC_API
int
tolower (
    int Character
    );

/*++

Routine Description:

    This routine returns converts the given character to lower case.

Arguments:

    Character - Supplies the character to check.

Return Value:

    Returns the lower cased version of the character, or the character itself.

--*/

#ifdef __cplusplus

}

#endif
#endif
