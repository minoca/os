/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    wctype.c

Abstract:

    This module implements the standard character class routines.

Author:

    Evan Green 28-Aug-2013

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <ctype.h>
#include <wchar.h>

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
int
iswalnum (
    wint_t Character
    )

/*++

Routine Description:

    This routine returns non-zero if the given wide character is alphanumeric.

Arguments:

    Character - Supplies the character to check.

Return Value:

    Returns non-zero if the given character is alphanumeric.

    0 if the character is not.

--*/

{

    return (iswalpha(Character) || iswdigit(Character));
}

LIBC_API
int
iswalpha (
    wint_t Character
    )

/*++

Routine Description:

    This routine returns non-zero if the given wide character is alphabetic.

Arguments:

    Character - Supplies the character to check.

Return Value:

    Returns non-zero if the given character is alphabetic.

    0 if the character is not.

--*/

{

    return ((iswupper(Character) || iswlower(Character)));
}

LIBC_API
int
iswascii (
    wint_t Character
    )

/*++

Routine Description:

    This routine returns non-zero if the given wide character is part of the
    ASCII character set.

Arguments:

    Character - Supplies the character to check.

Return Value:

    Returns non-zero if the given character is part of the ASCII set.

    0 if the character is not.

--*/

{

    return (((Character) & (~0x7F)) == 0);
}

LIBC_API
int
iswblank (
    wint_t Character
    )

/*++

Routine Description:

    This routine returns non-zero if the given wide character is blank. In the
    default locale, this is just the space and tab characters.

Arguments:

    Character - Supplies the character to check.

Return Value:

    Returns non-zero if the given character is blank.

    0 if the character is not.

--*/

{

    return ((Character == L' ') || (Character == L'\t'));
}

LIBC_API
int
iswcntrl (
    wint_t Character
    )

/*++

Routine Description:

    This routine returns non-zero if the given wide character is a control
    character. For the default locale, this is anything with a value less than
    that of the space character.

Arguments:

    Character - Supplies the character to check.

Return Value:

    Returns non-zero if the given character is alphabetic.

    0 if the character is not.

--*/

{

    return (Character < L' ');
}

LIBC_API
int
iswdigit (
    wint_t Character
    )

/*++

Routine Description:

    This routine returns non-zero if the given wide character is a digit (zero
    through nine).

Arguments:

    Character - Supplies the character to check.

Return Value:

    Returns non-zero if the given character is a digit.

    0 if the character is not.

--*/

{

    return ((Character >= L'0') && (Character <= L'9'));
}

LIBC_API
int
iswgraph (
    wint_t Character
    )

/*++

Routine Description:

    This routine returns non-zero if the given wide character is graphical. In
    the default locale, this represents anything that is alphanumeric or
    punctuation.

Arguments:

    Character - Supplies the character to check.

Return Value:

    Returns non-zero if the given character is graphical.

    0 if the character is not.

--*/

{

    return (iswalnum(Character) || iswpunct(Character));
}

LIBC_API
int
iswlower (
    wint_t Character
    )

/*++

Routine Description:

    This routine returns non-zero if the given wide character is lower case.

Arguments:

    Character - Supplies the character to check.

Return Value:

    Returns non-zero if the given character is lower case.

    0 if the character is not.

--*/

{

    return ((Character >= L'a') && (Character <= L'z'));
}

LIBC_API
int
iswprint (
    wint_t Character
    )

/*++

Routine Description:

    This routine returns non-zero if the given wide character is printable.
    In the default locale, this is anything that is either alphabetic,
    punctuation, or the space character.

Arguments:

    Character - Supplies the character to check.

Return Value:

    Returns non-zero if the given character is printable.

    0 if the character is not.

--*/

{

    if ((iswalnum(Character)) || (iswpunct(Character)) ||
        (Character == L' ')) {

        return 1;
    }

    return 0;
}

LIBC_API
int
iswpunct (
    wint_t Character
    )

/*++

Routine Description:

    This routine returns non-zero if the given wide character is punctuation.
    In the default locale, this is anything that is not alphabetic, a digit,
    a control character, or a space.

Arguments:

    Character - Supplies the character to check.

Return Value:

    Returns non-zero if the given character is punctuation.

    0 if the character is not.

--*/

{

    if ((!(iswalpha(Character))) && (!(iswdigit(Character))) &&
        (!(iswcntrl(Character))) && (Character != L' ')) {

        return 1;
    }

    return 0;
}

LIBC_API
int
iswspace (
    wint_t Character
    )

/*++

Routine Description:

    This routine returns non-zero if the given wide character is white space.
    In the default locale, this is the characters space, tab, newline (\n)
    carriage return (\r), form feed (\f), and vertical tab (\v).

Arguments:

    Character - Supplies the character to check.

Return Value:

    Returns non-zero if the given character is white space.

    0 if the character is not.

--*/

{

    if ((Character == L' ') || (Character == L'\t') ||
        (Character == L'\n') || (Character == L'\r') ||
        (Character == L'\f') || (Character == L'\v')) {

        return 1;
    }

    return 0;
}

LIBC_API
int
iswupper (
    wint_t Character
    )

/*++

Routine Description:

    This routine returns non-zero if the given wide character is upper case.

Arguments:

    Character - Supplies the character to check.

Return Value:

    Returns non-zero if the given character is upper case.

    0 if the character is not.

--*/

{

    return ((Character >= L'A') && (Character <= L'Z'));
}

LIBC_API
int
iswxdigit (
    wint_t Character
    )

/*++

Routine Description:

    This routine returns non-zero if the given wide character is a hexadecimal
    digit, which in the default locale is 0-9, a-f, and A-F.

Arguments:

    Character - Supplies the character to check.

Return Value:

    Returns non-zero if the given character is a hexadecimal digit.

    0 if the character is not.

--*/

{

    if (((Character >= L'0') && (Character <= L'9')) ||
        ((Character >= L'A') && (Character <= L'F')) ||
        ((Character >= L'a') && (Character <= L'f'))) {

        return 1;
    }

    return 0;
}

LIBC_API
wint_t
towascii (
    wint_t Character
    )

/*++

Routine Description:

    This routine converts a wide character into the ASCII wide character set by
    lopping off all but the least significant seven bits.

Arguments:

    Character - Supplies the character to convert.

Return Value:

    Returns the ASCII portion of the character.

--*/

{

    return (Character & 0x7F);
}

LIBC_API
wint_t
towupper (
    wint_t Character
    )

/*++

Routine Description:

    This routine returns converts the given wide character to upper case.

Arguments:

    Character - Supplies the character to convert.

Return Value:

    Returns the upper cased version of the character, or the character itself.

--*/

{

    if (iswlower(Character)) {
        return _toupper(Character);
    }

    return Character;
}

LIBC_API
wint_t
towlower (
    wint_t Character
    )

/*++

Routine Description:

    This routine returns converts the given wide character to lower case.

Arguments:

    Character - Supplies the character to check.

Return Value:

    Returns the lower cased version of the character, or the character itself.

--*/

{

    if (iswupper(Character)) {
        return _tolower(Character);
    }

    return Character;
}

//
// --------------------------------------------------------- Internal Functions
//

