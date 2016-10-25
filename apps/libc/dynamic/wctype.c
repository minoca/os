/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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
#include <wctype.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

typedef
int
(*CL_WIDE_CHARACTER_TEST_ROUTINE) (
    wint_t Character
    );

/*++

Routine Description:

    This routine returns non-zero if the given wide character is the type being
    tested.

Arguments:

    Character - Supplies the character to check.

Return Value:

    Returns non-zero if the given character is of the correct type.

    0 if the character is not.

--*/

/*++

Structure Description:

    This structure defines a wide character type.

Members:

    Name - Stores the type name.

    TestRoutine - Stores a pointer to the routine that tests whether or not the
        character is of the type.

--*/

typedef struct _CL_WIDE_CHARACTER_TYPE {
    PSTR Name;
    CL_WIDE_CHARACTER_TEST_ROUTINE TestRoutine;
} CL_WIDE_CHARATER_TYPE, *PCL_WIDE_CHARACTER_TYPE;

typedef
wint_t
(*CL_WIDE_CHARACTER_MAP_ROUTINE) (
    wint_t Character
    );

/*++

Routine Description:

    This routine returns converts the given wide character.

Arguments:

    Character - Supplies the character to convert.

Return Value:

    Returns the convert version of the character, or the character itself.

--*/

/*++

Structure Description:

    This structure defines a wide character mapping.

Members:

    Name - Stores the mapping name.

    MapRoutine - Stores a pointer to the routine that maps the character.

--*/

typedef struct _CL_WIDE_CHARACTER_MAPPING {
    PSTR Name;
    CL_WIDE_CHARACTER_MAP_ROUTINE MapRoutine;
} CL_WIDE_CHARACTER_MAPPING, *PCL_WIDE_CHARACTER_MAPPING;

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

CL_WIDE_CHARATER_TYPE ClpWideCharacterTypes[] = {
    {"alnum", iswalnum},
    {"alpha", iswalpha},
    {"blank", iswblank},
    {"cntrl", iswcntrl},
    {"digit", iswdigit},
    {"graph", iswgraph},
    {"lower", iswlower},
    {"print", iswprint},
    {"punct", iswpunct},
    {"space", iswspace},
    {"upper", iswupper},
    {"xdigit", iswxdigit}
};

CL_WIDE_CHARACTER_MAPPING ClpWideCharacterMappings[] = {
    {"tolower", towlower},
    {"toupper", towupper}
};

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

    return ((Character < L' ') || (Character == 0x7F));
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

    return ((Character >= L'!') && (Character < 0x7F));
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

    return ((Character >= L' ') && (Character < 0x7F));
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

    if ((iswprint(Character)) &&
        (!iswalnum(Character)) && (Character != L' ')) {

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

    Character - Supplies the character to convert.

Return Value:

    Returns the lower cased version of the character, or the character itself.

--*/

{

    if (iswupper(Character)) {
        return _tolower(Character);
    }

    return Character;
}

LIBC_API
wctrans_t
wctrans (
    const char *CharacterClass
    )

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

{

    int Count;
    int Index;

    Count = sizeof(ClpWideCharacterMappings) /
            sizeof(ClpWideCharacterMappings[0]);

    for (Index = 0; Index < Count; Index += 1) {
        if (strcmp(CharacterClass, ClpWideCharacterMappings[Index].Name) == 0) {
            return Index + 1;
        }
    }

    return 0;
}

LIBC_API
wint_t
towctrans (
    wint_t Character,
    wctrans_t Descriptor
    )

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

{

    int Count;

    if (Descriptor == 0) {
        return Character;
    }

    Count = sizeof(ClpWideCharacterMappings) /
            sizeof(ClpWideCharacterMappings[0]);

    if (Descriptor > Count) {
        return Character;
    }

    return ClpWideCharacterMappings[Descriptor - 1].MapRoutine(Character);
}

LIBC_API
wctype_t
wctype (
    const char *Property
    )

/*++

Routine Description:

    This routine returns the wide character type class for the given property.

Arguments:

    Property - Supplies the name of the character class to look up.

Return Value:

    Returns the type class identifier if the property is valid.

    0 if the property is invalid.

--*/

{

    int Count;
    int Index;

    Count = sizeof(ClpWideCharacterTypes) / sizeof(ClpWideCharacterTypes[0]);
    for (Index = 0; Index < Count; Index += 1) {
        if (strcmp(Property, ClpWideCharacterTypes[Index].Name) == 0) {
            return Index + 1;
        }
    }

    return 0;
}

LIBC_API
int
iswctype (
    wint_t Character,
    wctype_t CharacterClass
    )

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

{

    int Count;

    if (CharacterClass == 0) {
        return 0;
    }

    Count = sizeof(ClpWideCharacterTypes) / sizeof(ClpWideCharacterTypes[0]);
    if (CharacterClass > Count) {
        return 0;
    }

    return ClpWideCharacterTypes[CharacterClass - 1].TestRoutine(Character);
}

//
// --------------------------------------------------------- Internal Functions
//

