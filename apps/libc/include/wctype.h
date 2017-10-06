/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    wctype.h

Abstract:

    This header contains definitions for the wide character type classification
    functions.

Author:

    Evan Green 28-Aug-2013

--*/

#ifndef _WCTYPE_H
#define _WCTYPE_H

//
// ------------------------------------------------------------------- Includes
//

#include <wchar.h>

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// ------------------------------------------------------ Data Type Definitions
//

typedef unsigned long wctrans_t;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
int
iswalnum (
    wint_t Character
    );

/*++

Routine Description:

    This routine returns non-zero if the given wide character is alphanumeric.

Arguments:

    Character - Supplies the character to check.

Return Value:

    Returns non-zero if the given character is alphanumeric.

    0 if the character is not.

--*/

LIBC_API
int
iswalpha (
    wint_t Character
    );

/*++

Routine Description:

    This routine returns non-zero if the given wide character is alphabetic.

Arguments:

    Character - Supplies the character to check.

Return Value:

    Returns non-zero if the given character is alphabetic.

    0 if the character is not.

--*/

LIBC_API
int
iswascii (
    wint_t Character
    );

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

LIBC_API
int
iswblank (
    wint_t Character
    );

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

LIBC_API
int
iswcntrl (
    wint_t Character
    );

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

LIBC_API
int
iswdigit (
    wint_t Character
    );

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

LIBC_API
int
iswgraph (
    wint_t Character
    );

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

LIBC_API
int
iswlower (
    wint_t Character
    );

/*++

Routine Description:

    This routine returns non-zero if the given wide character is lower case.

Arguments:

    Character - Supplies the character to check.

Return Value:

    Returns non-zero if the given character is lower case.

    0 if the character is not.

--*/

LIBC_API
int
iswprint (
    wint_t Character
    );

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

LIBC_API
int
iswpunct (
    wint_t Character
    );

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

LIBC_API
int
iswspace (
    wint_t Character
    );

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

LIBC_API
int
iswupper (
    wint_t Character
    );

/*++

Routine Description:

    This routine returns non-zero if the given wide character is upper case.

Arguments:

    Character - Supplies the character to check.

Return Value:

    Returns non-zero if the given character is upper case.

    0 if the character is not.

--*/

LIBC_API
int
iswxdigit (
    wint_t Character
    );

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

LIBC_API
wint_t
towascii (
    wint_t Character
    );

/*++

Routine Description:

    This routine converts a wide character into the ASCII wide character set by
    lopping off all but the least significant seven bits.

Arguments:

    Character - Supplies the character to convert.

Return Value:

    Returns the ASCII portion of the character.

--*/

LIBC_API
wint_t
towupper (
    wint_t Character
    );

/*++

Routine Description:

    This routine returns converts the given wide character to upper case.

Arguments:

    Character - Supplies the character to convert.

Return Value:

    Returns the upper cased version of the character, or the character itself.

--*/

LIBC_API
wint_t
towlower (
    wint_t Character
    );

/*++

Routine Description:

    This routine returns converts the given wide character to lower case.

Arguments:

    Character - Supplies the character to convert.

Return Value:

    Returns the lower cased version of the character, or the character itself.

--*/

LIBC_API
wctrans_t
wctrans (
    const char *CharacterClass
    );

/*++

Routine Description:

    This routine returns the wide character mapping descriptor for the given
    character class.

Arguments:

    CharacterClass - Supplies the name of the character class to look up.

Return Value:

    Returns the mapping descriptor if the character class if valid.

    0 if the character class is invalid.

--*/

LIBC_API
wint_t
towctrans (
    wint_t Character,
    wctrans_t Descriptor
    );

/*++

Routine Description:

    This routine returns converts the given wide character using the mapping
    class identified by the descriptor.

Arguments:

    Character - Supplies the character to convert.

    Descriptor - Supplies the descriptor for the mapping class to use.

Return Value:

    Returns the converted version of the character, or the character itself.

--*/

LIBC_API
wctype_t
wctype (
    const char *Property
    );

/*++

Routine Description:

    This routine returns the wide character type class for the given property.

Arguments:

    Property - Supplies the name of the character class to look up.

Return Value:

    Returns the type class identifier if the property is valid.

    0 if the property is invalid.

--*/

LIBC_API
int
iswctype (
    wint_t Character,
    wctype_t Type
    );

/*++

Routine Description:

    This routine tests whether or not the given character belongs to the given
    class.

Arguments:

    Character - Supplies the character to check.

    CharacterClass - Supplies the identifier of the class to check against.

Return Value:

    Returns non-zero if the given character belongs to the class.

    0 if the character does not.

--*/

#ifdef __cplusplus

}

#endif
#endif
