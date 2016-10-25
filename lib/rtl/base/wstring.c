/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    wstring.c

Abstract:

    This module implements common string manipulation functions used by the
    kernel.

Author:

    Evan Green 27-Aug-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "rtlp.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

RTL_API
ULONG
RtlStringCopyWide (
    PWSTR Destination,
    PWSTR Source,
    ULONG BufferSize
    )

/*++

Routine Description:

    This routine copies a wide string from one buffer to another, including the
    NULL terminator.

Arguments:

    Destination - Supplies a pointer to the buffer where the wide string will be
        copied to.

    Source - Supplies a pointer to the wide string to copy.

    BufferSize - Supplies the size of the destination buffer.

Return Value:

    Returns the number of characters copied, including the NULL terminator. If
    the source string is longer than the destination buffer, the string will be
    truncated but still NULL terminated.

--*/

{

    ULONG Index;

    ASSERT(BufferSize != 0);

    for (Index = 0; Index < BufferSize; Index += 1) {
        Destination[Index] = Source[Index];
        if (Source[Index] == WIDE_STRING_TERMINATOR) {
            break;
        }
    }

    if (Index == BufferSize) {
        Index -= 1;
    }

    //
    // Terminate the string in case the source was too long.
    //

    Destination[Index] = WIDE_STRING_TERMINATOR;
    return Index + 1;
}

RTL_API
VOID
RtlStringReverseWide (
    PWSTR String,
    PWSTR StringEnd
    )

/*++

Routine Description:

    This routine reverses the contents of a wide string. For example, the string
    L"abcd" would get reversed to L"dcba".

Arguments:

    String - Supplies a pointer to the beginning of the wide string to reverse.

    StringEnd - Supplies a pointer to one beyond the end of the string. That is,
        this pointer points to the first byte *not* in the string.

Return Value:

    None.

--*/

{

    ULONG Length;
    ULONG Position;
    WCHAR SwapSpace;

    Length = StringEnd - String;

    //
    // Work from the left towards the middle, swapping characters with their
    // positions on the other extreme. The truncation of Length / 2 is okay
    // because odd length strings do not need their middle byte swapped.
    //

    for (Position = 0; Position < (Length / 2); Position += 1) {
        SwapSpace = String[Position];
        String[Position] = String[Length - Position - 1];
        String[Length - Position - 1] = SwapSpace;
    }

    return;
}

RTL_API
ULONG
RtlStringLengthWide (
    PWSTR String
    )

/*++

Routine Description:

    This routine determines the length of the given wide string, not including
    its NULL terminator.

Arguments:

    String - Supplies a pointer to the beginning of the string.

Return Value:

    Returns the length of the string, not including the NULL terminator.

--*/

{

    ULONG Length;

    Length = 0;
    while (*String != WIDE_STRING_TERMINATOR) {
        Length += 1;
        String += 1;
    }

    return Length;
}

RTL_API
BOOL
RtlAreStringsEqualWide (
    PWSTR String1,
    PWSTR String2,
    ULONG MaxLength
    )

/*++

Routine Description:

    This routine determines if the contents of two wide strings are equal, up
    to a maximum number of characters.

Arguments:

    String1 - Supplies a pointer to the first wide string to compare.

    String2 - Supplies a pointer to the second wide string to compare.

    MaxLength - Supplies the minimum of either string's buffer size.

Return Value:

    TRUE if the strings are equal up to the maximum length.

    FALSE if the strings differ in some way.

--*/

{

    ASSERT((String1 != NULL) && (String2 != NULL));

    while ((*String1 != WIDE_STRING_TERMINATOR) &&
           (*String2 != WIDE_STRING_TERMINATOR) &&
           (MaxLength != 0)) {

        if (*String1 != *String2) {
            return FALSE;
        }

        String1 += 1;
        String2 += 1;
        MaxLength -= 1;
    }

    if ((MaxLength != 0) && (*String1 != *String2)) {
        return FALSE;
    }

    return TRUE;
}

RTL_API
BOOL
RtlAreStringsEqualIgnoringCaseWide (
    PWSTR String1,
    PWSTR String2,
    ULONG MaxLength
    )

/*++

Routine Description:

    This routine determines if the contents of two wide strings are equal, up
    to a maximum number of characters. This routine is case insensitive.

Arguments:

    String1 - Supplies a pointer to the first wide string to compare.

    String2 - Supplies a pointer to the second wide string to compare.

    MaxLength - Supplies the minimum of either string's buffer size, in
        characters.

Return Value:

    TRUE if the strings are equal up to the maximum length.

    FALSE if the strings differ in some way.

--*/

{

    WCHAR Character1;
    WCHAR Character2;

    ASSERT((String1 != NULL) && (String2 != NULL));

    while ((*String1 != WIDE_STRING_TERMINATOR) &&
           (*String2 != WIDE_STRING_TERMINATOR) &&
           (MaxLength != 0)) {

        Character1 = *String1;
        Character2 = *String2;
        if ((Character1 >= L'a') && (Character1 <= L'z')) {
            Character1 = Character1 - L'a' + L'A';
        }

        if ((Character2 >= L'a') && (Character2 <= L'z')) {
            Character2 = Character2 - L'a' + L'A';
        }

        if (Character1 != Character2) {
            return FALSE;
        }

        String1 += 1;
        String2 += 1;
        MaxLength -= 1;
    }

    if ((MaxLength != 0) && (*String1 != *String2)) {
        return FALSE;
    }

    return TRUE;
}

RTL_API
PWSTR
RtlStringFindCharacterWide (
    PWSTR String,
    WCHAR Character,
    ULONG StringLength
    )

/*++

Routine Description:

    This routine searches a wide string for the first instance of the given
    character, scanning from the left.

Arguments:

    String - Supplies a pointer to the wide string to search.

    Character - Supplies a pointer to the wide character to search for within
        the string.

    StringLength - Supplies the length of the string, in characters, including
        the NULL terminator.

Return Value:

    Returns a pointer to the first instance of the character on success.

    NULL if the character could not be found in the string.

--*/

{

    //
    // Search the string for the character as long as the end of the string
    // is not reached according to a NULL terminator or the string length.
    //

    while ((StringLength != 0) && (*String != WIDE_STRING_TERMINATOR)) {
        if (*String == Character) {
            return String;
        }

        String += 1;
        StringLength -= 1;
    }

    return NULL;
}

RTL_API
PWSTR
RtlStringFindCharacterRightWide (
    PWSTR String,
    WCHAR Character,
    ULONG StringLength
    )

/*++

Routine Description:

    This routine searches a wide string for the first instance of the given
    character, scanning from the right backwards. The function will search
    starting at the NULL terminator or string length, whichever comes first.

Arguments:

    String - Supplies a pointer to the wide string to search.

    Character - Supplies a pointer to the character to search for within the
        wide string.

    StringLength - Supplies the length of the string, in characters, including
        the NULL terminator.

Return Value:

    Returns a pointer to the first instance of the character on success.

    NULL if the character could not be found in the string.

--*/

{

    PWSTR Current;

    if ((String == NULL) || (StringLength == 0)) {
        return NULL;
    }

    //
    // Find the end of the string.
    //

    Current = String;
    while ((*Current != WIDE_STRING_TERMINATOR) &&
           ((UINTN)Current - (UINTN)String < StringLength)) {

        Current += 1;
    }

    //
    // Now walk backwards looking for the character.
    //

    while (Current != String) {
        if (*Current == Character) {
            return Current;
        }

        Current -= 1;
    }

    if (*Current == Character) {
        return String;
    }

    return NULL;
}

//
// --------------------------------------------------------- Internal Functions
//

