/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    wstring.c

Abstract:

    This module implements string and memory manipulation routines for the C
    library.

Author:

    Evan Green 28-Aug-2012

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

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
wchar_t *
wmemchr (
    const wchar_t *Buffer,
    wchar_t Character,
    size_t Size
    )

/*++

Routine Description:

    This routine attempts to locate the first occurrence of the given character
    within the given buffer.

Arguments:

    Buffer - Supplies a pointer to the buffer of wide characters.

    Character - Supplies the wide character to locate.

    Size - Supplies the size of the buffer, in characters.

Return Value:

    Returns a pointer to the first occurrence of the character within the
    buffer on success.

    NULL on failure.

--*/

{

    while (Size != 0) {
        if (*Buffer == Character) {
            return (wchar_t *)Buffer;
        }

        Buffer += 1;
        Size -= 1;
    }

    return NULL;
}

LIBC_API
int
wmemcmp (
    const wchar_t *Left,
    const wchar_t *Right,
    size_t Size
    )

/*++

Routine Description:

    This routine compares two wide strings of memory byte for byte. The null
    wide character is not treated specially here.

Arguments:

    Left - Supplies the first wide string of the comparison.

    Right - Supplies the second wide string of the comparison.

    Size - Supplies the maximum number of characters to compare.

Return Value:

    >0 if Left > Right.

    0 is Left == Right.

    <0 if Left <= Right.

--*/

{

    wchar_t Difference;
    size_t Index;

    for (Index = 0; Index < Size; Index += 1) {
        Difference = *Left - *Right;
        if (Difference != 0) {
            return Difference;
        }

        Left += 1;
        Left += 1;
    }

    return 0;
}

LIBC_API
wchar_t *
wmemcpy (
    wchar_t *Destination,
    const wchar_t *Source,
    size_t CharacterCount
    )

/*++

Routine Description:

    This routine copies characters directly between buffers. The null wide
    character is not treated specially here.

Arguments:

    Destination - Supplies a pointer to the destination of the copy.

    Source - Supplies a pointer to the source data to copy.

    CharacterCount - Supplies the number of characters to copy.

Return Value:

    Returns the destination parameter.

--*/

{

    wchar_t *Result;

    Result = RtlCopyMemory(Destination,
                           (PVOID)Source,
                           CharacterCount * sizeof(wchar_t));

    return Result;
}

LIBC_API
wchar_t *
wmemmove (
    wchar_t *Destination,
    const wchar_t *Source,
    size_t CharacterCount
    )

/*++

Routine Description:

    This routine copies bytes between buffers. Copying takes place as if the
    bytes are first copied into a temporary buffer that does not overlap the
    two buffers, and then are copied to the destination.

Arguments:

    Destination - Supplies a pointer to the destination of the copy.

    Source - Supplies a pointer to the source data to copy.

    CharacterCount - Supplies the number of characters to copy.

Return Value:

    Returns the destination parameter.

--*/

{

    PWCHAR DestinationBytes;
    PWCHAR SourceBytes;

    //
    // Copy the bytes backwards if the source begins before the destination
    // and overlaps.
    //

    if ((Source < Destination) && (Source + CharacterCount > Destination)) {
        DestinationBytes = Destination;
        SourceBytes = (PWCHAR)Source;
        while (CharacterCount != 0) {
            DestinationBytes[CharacterCount - 1] =
                                               SourceBytes[CharacterCount - 1];

            CharacterCount -= 1;
        }

    } else {
        RtlCopyMemory(Destination,
                      (PVOID)Source,
                      CharacterCount * sizeof(wchar_t));
    }

    return Destination;
}

LIBC_API
wchar_t *
wmemset (
    wchar_t *Destination,
    wchar_t Character,
    size_t CharacterCount
    )

/*++

Routine Description:

    This routine copies the given character repeatedly into the given buffer.

Arguments:

    Destination - Supplies a pointer to the destination of the copy.

    Character - Supplies a character (it will be cast down to a character) to
        fill the buffer with.

    CharacterCount - Supplies the number of characters to set.

Return Value:

    Returns the destination parameter.

--*/

{

    size_t Index;

    for (Index = 0; Index < CharacterCount; Index += 1) {
        Destination[Index] = Character;
    }

    return Destination;
}

LIBC_API
wchar_t *
wcschr (
    const wchar_t *String,
    wchar_t Character
    )

/*++

Routine Description:

    This routine finds the first instance of the given character (converted to
    a char) in the given wide string.

Arguments:

    String - Supplies a pointer to the string to search for the character in.

    Character - Supplies the character to search for.

Return Value:

    Returns a pointer to the first occurrence of the character in the given
    string, or NULL if the character doesn't exist in the string.

--*/

{

    while (TRUE) {
        if (*String == Character) {
            return (wchar_t *)String;
        }

        if (*String == L'\0') {
            break;
        }

        String += 1;
    }

    return NULL;
}

LIBC_API
wchar_t *
wcsrchr (
    const wchar_t *String,
    wchar_t Character
    )

/*++

Routine Description:

    This routine finds the last occurrence of the given character (converted to
    a char) in the given wide string.

Arguments:

    String - Supplies a pointer to the wide string to search for the character
        in.

    Character - Supplies the character to search for.

Return Value:

    Returns a pointer to the last occurrence of the character in the given
    string, or NULL if the character doesn't exist in the string.

--*/

{

    wchar_t *LastOccurrence;

    LastOccurrence = NULL;
    while (TRUE) {
        if (*String == Character) {
            LastOccurrence = (wchar_t *)String;
        }

        if (*String == L'\0') {
            break;
        }

        String += 1;
    }

    return LastOccurrence;
}

LIBC_API
size_t
wcslen (
    const wchar_t *String
    )

/*++

Routine Description:

    This routine computes the length of the given string, not including the
    null terminator.

Arguments:

    String - Supplies a pointer to the string whose length should be computed.

Return Value:

    Returns the length of the string, not including the null terminator.

--*/

{

    return RtlStringLengthWide((PWSTR)String);
}

LIBC_API
int
wcswidth (
    const wchar_t *String,
    size_t Size
    )

/*++

Routine Description:

    This routine computes the display width of the given string.

Arguments:

    String - Supplies a pointer to the string whose display width should be
        computed.

    Size - Supplies the size of the string in characters.

Return Value:

    Returns the number of columns the given string occupies.

    -1 if one of the characters is invalid.

--*/

{

    int Total;
    int Width;

    Total = 0;
    while ((*String != L'\0') && (Size != 0)) {
        Width = wcwidth(*String);
        if (Width < 0) {
            return -1;
        }

        Total += Width;
        String += 1;
        Size -= 1;
    }

    return Total;
}

LIBC_API
wchar_t *
wcscpy (
    wchar_t *DestinationString,
    const wchar_t *SourceString
    )

/*++

Routine Description:

    This routine copies the given source wide string over the given destination
    string. This routine should be avoided if at all possible as it can be the
    cause of buffer overflow problems. Use functions like wcsncpy that place
    explicit bounds on the destination buffer.

Arguments:

    DestinationString - Supplies a pointer where the source string will be
        copied to.

    SourceString - Supplies the string that will be copied.

Return Value:

    Returns the destination string.

--*/

{

    wchar_t *OriginalDestination;

    OriginalDestination = DestinationString;
    while (*SourceString != L'\0') {
        *DestinationString = *SourceString;
        SourceString += 1;
        DestinationString += 1;
    }

    *DestinationString = L'\0';
    return OriginalDestination;
}

LIBC_API
wchar_t *
wcsncpy (
    wchar_t *DestinationString,
    const wchar_t *SourceString,
    size_t CharacterCount
    )

/*++

Routine Description:

    This routine copies the given source string over the given destination
    string.

Arguments:

    DestinationString - Supplies a pointer where the source string will be
        copied to.

    SourceString - Supplies the string that will be copied.

    CharacterCount - Supplies the maximum number of characters to copy. If the
        source string is shorter than this value, then only characters up to
        and including the null terminator will be copied. The remaining
        characters in the destination string will be zeroed out. If the source
        string is longer than this value, then the destination string will not
        be null terminated.

Return Value:

    Returns the destination string.

--*/

{

    size_t ByteIndex;
    wchar_t *OriginalDestination;

    OriginalDestination = DestinationString;
    for (ByteIndex = 0; ByteIndex < CharacterCount; ByteIndex += 1) {
        *DestinationString = *SourceString;
        if (*SourceString == L'\0') {
            break;
        }

        DestinationString += 1;
        SourceString += 1;
    }

    while (ByteIndex < CharacterCount) {
        *DestinationString = L'\0';
        DestinationString += 1;
        ByteIndex += 1;
    }

    return OriginalDestination;
}

LIBC_API
wchar_t *
wcscat (
    wchar_t *DestinationString,
    const wchar_t *SourceString
    )

/*++

Routine Description:

    This routine appends bytes to the end of the given wide string. The
    destination string will always be returned with a null terminator.

Arguments:

    DestinationString - Supplies a pointer containing the string that will be
        appended to.

    SourceString - Supplies a pointer to the string to append.

Return Value:

    Returns the destination string.

--*/

{

    return wcsncat(DestinationString, SourceString, MAX_ULONG);
}

LIBC_API
wchar_t *
wcsncat (
    wchar_t *DestinationString,
    const wchar_t *SourceString,
    size_t CharactersToAppend
    )

/*++

Routine Description:

    This routine appends characters to the end of the given wide string. The
    destination string will always be returned with a wide null terminator.

Arguments:

    DestinationString - Supplies a pointer containing the string that will be
        appended to.

    SourceString - Supplies a pointer to the string to append.

    CharactersToAppend - Supplies the number of bytes of the source string to
        append to the destination, NOT including the null terminator. This
        means that the destination string buffer must be at least large enough
        to take this number plus one bytes on the end of the existing string. If
        the source string is shorter than this value, this routine will stop at
        the terminator.

Return Value:

    Returns the destination string.

--*/

{

    size_t ByteIndex;
    wchar_t *OriginalDestination;

    OriginalDestination = DestinationString;

    //
    // First find the end of the string.
    //

    while (*DestinationString != L'\0') {
        DestinationString += 1;
    }

    //
    // Now copy as many bytes as are requested over.
    //

    for (ByteIndex = 0; ByteIndex < CharactersToAppend; ByteIndex += 1) {

        //
        // Stop if the source ended.
        //

        if (*SourceString == L'\0') {
            break;
        }

        *DestinationString = *SourceString;
        DestinationString += 1;
        SourceString += 1;
    }

    //
    // Always null terminate the destination.
    //

    *DestinationString = L'\0';
    return OriginalDestination;
}

LIBC_API
int
wcscmp (
    const wchar_t *String1,
    const wchar_t *String2
    )

/*++

Routine Description:

    This routine compares two wide strings for equality.

Arguments:

    String1 - Supplies the first wide string to compare.

    String2 - Supplies the second wide string to compare.

Return Value:

    0 if the strings are equal all the way through their null terminators.

    Non-zero if the strings are different. The sign of the return value will be
    determined by the sign of the difference between the values of the first
    pair of bytes (both interpreted as type unsigned char) that differ in the
    strings being compared.

--*/

{

    return wcsncmp(String1, String2, (size_t)-1);
}

LIBC_API
int
wcsicmp (
    const wchar_t *String1,
    const wchar_t *String2
    )

/*++

Routine Description:

    This routine compares two wide strings for equality, ignoring case.

Arguments:

    String1 - Supplies the first wide string to compare.

    String2 - Supplies the second wide string to compare.

Return Value:

    0 if the strings are equal all the way through their null terminators.

    Non-zero if the strings are different. The sign of the return value will be
    determined by the sign of the difference between the values of the first
    pair of bytes (both interpreted as type unsigned char) that differ in the
    strings being compared.

--*/

{

    return wcsncasecmp(String1, String2, (size_t)-1);
}

LIBC_API
int
wcsncmp (
    const wchar_t *String1,
    const wchar_t *String2,
    size_t CharacterCount
    )

/*++

Routine Description:

    This routine compares two wide strings for equality, up to a bounded amount.

Arguments:

    String1 - Supplies the first wide string to compare.

    String2 - Supplies the second wide string to compare.

    CharacterCount - Supplies the maximum number of characters to compare.
        Characters after a null terminator in either string are not compared.

Return Value:

    0 if the strings are equal all the way through their null terminators or
    character count.

    Non-zero if the strings are different. The sign of the return value will be
    determined by the sign of the difference between the values of the first
    pair of bytes (both interpreted as type unsigned char) that differ in the
    strings being compared.

--*/

{

    while (CharacterCount != 0) {
        if (*String1 != *String2) {
            return *String1 - *String2;
        }

        if (*String1 == L'\0') {
            break;
        }

        String1 += 1;
        String2 += 1;
        CharacterCount -= 1;
    }

    return 0;
}

LIBC_API
int
wcsnicmp (
    const wchar_t *String1,
    const wchar_t *String2,
    size_t CharacterCount
    )

/*++

Routine Description:

    This routine compares two wide strings for equality, ignoring case, up to a
    bounded amount.

Arguments:

    String1 - Supplies the first wide string to compare.

    String2 - Supplies the second wide string to compare.

    CharacterCount - Supplies the maximum number of characters to compare.
        Characters after a null terminator in either string are not compared.

Return Value:

    0 if the strings are equal all the way through their null terminators or
    character count.

    Non-zero if the strings are different. The sign of the return value will be
    determined by the sign of the difference between the values of the first
    pair of bytes (both interpreted as type unsigned char) that differ in the
    strings being compared.

--*/

{

    return wcsncasecmp(String1, String2, CharacterCount);
}

LIBC_API
int
wcscasecmp (
    const wchar_t *String1,
    const wchar_t *String2
    )

/*++

Routine Description:

    This routine compares two wide strings for equality, ignoring case. This
    routine will act for the purposes of comparison like all characters are
    converted to lowercase.

Arguments:

    String1 - Supplies the first wide string to compare.

    String2 - Supplies the second wide string to compare.

Return Value:

    0 if the strings are equal all the way through their null terminators.

    Non-zero if the strings are different. The sign of the return value will be
    determined by the sign of the difference between the values of the first
    pair of bytes (both interpreted as type unsigned char) that differ in the
    strings being compared.

--*/

{

    return wcsncasecmp(String1, String2, -1);
}

LIBC_API
int
wcsncasecmp (
    const wchar_t *String1,
    const wchar_t *String2,
    size_t CharacterCount
    )

/*++

Routine Description:

    This routine compares two wide strings for equality, ignoring case, up to a
    bounded amount. This routine will act for the purposes of comparison like
    all characters are converted to lowercase.

Arguments:

    String1 - Supplies the wide first string to compare.

    String2 - Supplies the wide second string to compare.

    CharacterCount - Supplies the maximum number of characters to compare.
        Characters after a null terminator in either string are not compared.

Return Value:

    0 if the strings are equal all the way through their null terminators or
    character count.

    Non-zero if the strings are different. The sign of the return value will be
    determined by the sign of the difference between the values of the first
    pair of bytes (both interpreted as type unsigned char) that differ in the
    strings being compared.

--*/

{

    wchar_t Character1;
    wchar_t Character2;

    while (CharacterCount != 0) {
        Character1 = towlower(*String1);
        Character2 = towlower(*String2);
        if (Character1 != Character2) {
            return Character1 - Character2;
        }

        if (Character1 == L'\0') {
            break;
        }

        String1 += 1;
        String2 += 1;
        CharacterCount -= 1;
    }

    return 0;
}

LIBC_API
int
wcscoll (
    const wchar_t *String1,
    const wchar_t *String2
    )

/*++

Routine Description:

    This routine compares two wide strings, both interpreted as appropriate to
    the LC_COLLATE category of the current locale.

Arguments:

    String1 - Supplies a pointer to the first wide string.

    String2 - Supplies a pointer to the second wide string.

Return Value:

    >0 if the first string is greater than the second string.

    0 if the first string is equal to the second string.

    <0 if the first string is less than the second string.

--*/

{

    //
    // TODO: Implement collating stuff.
    //

    return wcscmp(String1, String2);
}

LIBC_API
wchar_t *
wcsdup (
    const wchar_t *String
    )

/*++

Routine Description:

    This routine returns a pointer to a newly allocated wide string which is a
    duplicate of the given input wide string. This returned pointer must be
    passed to the free function when the caller is done with it.

Arguments:

    String - Supplies a pointer to the wide string to duplicate.

Return Value:

    Returns a pointer to the newly allocated duplicate wide string on success.

    NULL on failure.

--*/

{

    size_t Length;
    wchar_t *NewString;

    if (String == NULL) {
        Length = 1;

    } else {
        Length = wcslen(String) + 1;
    }

    NewString = malloc(Length * sizeof(wchar_t));
    if (NewString == NULL) {
        return NULL;
    }

    wcscpy(NewString, String);
    return NewString;
}

LIBC_API
wchar_t *
wcspbrk (
    const wchar_t *String,
    const wchar_t *Characters
    )

/*++

Routine Description:

    This routine locates the first occurrence in the given wide string of any
    character from the given character set.

Arguments:

    String - Supplies a pointer to the wide string to search.

    Characters - Supplies a pointer to a null terminated wide string containing
        the acceptable set of characters.

Return Value:

    Returns a pointer within the given string to the first character in the
    requested set.

    NULL if no bytes from the set occur in the given string.

--*/

{

    const wchar_t *CurrentCharacter;

    while (*String != L'\0') {
        CurrentCharacter = Characters;
        while (*CurrentCharacter != L'\0') {
            if (*String == *CurrentCharacter) {
                return (wchar_t *)String;
            }
        }

        String += 1;
    }

    return NULL;
}

LIBC_API
size_t
wcscspn (
    const wchar_t *Input,
    const wchar_t *Characters
    )

/*++

Routine Description:

    This routine computes the length in bytes of the initial portion of the
    given input that's made up only of characters not in the given set. For
    example, an input of "abc123" and a set of "0123456789" would return a value
    of 3.

Arguments:

    Input - Supplies a pointer to a null terminated wide string containing the
        string to query.

    Characters - Supplies a pointer to a null terminated wide string containing
        the set of characters.

Return Value:

    Returns the count of initial characters in the string not in the given
    set.

--*/

{

    size_t Count;
    const wchar_t *CurrentCharacter;

    Count = 0;
    while (*Input != L'\0') {
        CurrentCharacter = Characters;
        while (*CurrentCharacter != L'\0') {
            if (*Input == *CurrentCharacter) {
                break;
            }

            CurrentCharacter += 1;
        }

        if (*CurrentCharacter != L'\0') {
            break;
        }

        Count += 1;
        Input += 1;
    }

    return Count;
}

LIBC_API
size_t
wcsspn (
    const wchar_t *Input,
    const wchar_t *Characters
    )

/*++

Routine Description:

    This routine computes the length in bytes of the initial portion of the
    given input that's made up only of characters from the given set. For
    example, an input of "129th" and a set of "0123456789" would return a value
    of 3.

Arguments:

    Input - Supplies a pointer to a null terminated wide string containing the
        string to query.

    Characters - Supplies a pointer to a null terminated wide string containing
        the acceptable set of characters.

Return Value:

    Returns the count of initial characters in the string in the given set.

--*/

{

    size_t Count;
    const wchar_t *CurrentCharacter;

    Count = 0;
    while (*Input != L'\0') {
        CurrentCharacter = Characters;
        while (*CurrentCharacter != L'\0') {
            if (*Input == *CurrentCharacter) {
                break;
            }

            CurrentCharacter += 1;
        }

        if (*CurrentCharacter == L'\0') {
            break;
        }

        Count += 1;
        Input += 1;
    }

    return Count;
}

LIBC_API
wchar_t *
wcsstr (
    const wchar_t *InputString,
    const wchar_t *QueryString
    )

/*++

Routine Description:

    This routine attempts to find the first occurrence of the wide query string
    in the given wide input string.

Arguments:

    InputString - Supplies a pointer to the wide input string to search.

    QueryString - Supplies a pointer to the wide query string to search for.

Return Value:

    Returns a pointer within the input string to the first instance of the
    query string.

    NULL if no instances of the query string were found in the input string.

--*/

{

    size_t QueryIndex;

    if ((QueryString == NULL) || (InputString == NULL)) {
        return NULL;
    }

    while (*InputString != L'\0') {

        //
        // Loop as long as the query string hasn't ended and it seems to be
        // matching the current input.
        //

        QueryIndex = 0;
        while ((QueryString[QueryIndex] != L'\0') &&
               (QueryString[QueryIndex] == InputString[QueryIndex])) {

            QueryIndex += 1;
        }

        if (QueryString[QueryIndex] == L'\0') {
            return (wchar_t *)InputString;
        }

        InputString += 1;
    }

    return NULL;
}

LIBC_API
wchar_t *
wcswcs (
    const wchar_t *InputString,
    const wchar_t *QueryString
    )

/*++

Routine Description:

    This routine attempts to find the first occurrence of the wide query string
    in the given wide input string.

Arguments:

    InputString - Supplies a pointer to the wide input string to search.

    QueryString - Supplies a pointer to the wide query string to search for.

Return Value:

    Returns a pointer within the input string to the first instance of the
    query string.

    NULL if no instances of the query string were found in the input string.

--*/

{

    return wcsstr(InputString, QueryString);
}

LIBC_API
wchar_t *
wcstok (
    wchar_t *InputString,
    const wchar_t *Separators,
    wchar_t **LastToken
    )

/*++

Routine Description:

    This routine breaks a wide string into a series of tokens delimited by any
    character from the given separator set. The first call passes an input
    string in. This routine scans looking for a non-separator character, which
    marks the first token. It then scans looking for a separator character, and
    sets that byte to the null terminator to delimit the first character.
    Subsequent calls should pass NULL as the input string, and the context
    pointer will be updated so that successive calls return the next tokens.
    This routine is thread safe and re-entrant so long as the same context
    pointer is not used by multiple threads.

Arguments:

    InputString - Supplies a pointer to the wide input string to tokenize. If
        supplied, this will reset the value returned in the last token context
        pointer.

    Separators - Supplies a pointer to a null terminated wide string containing
        the set of characters that delimit tokens. This may vary from call to
        call of this routine with the same context pointer.

    LastToken - Supplies a pointer where a context pointer will be stored
        allowing this routine to keep its place and return successive tokens.

Return Value:

    Returns a pointer to the next token on success.

    NULL if there are no more tokens.

--*/

{

    size_t Count;
    PWSTR Token;

    Token = InputString;
    if (Token == NULL) {
        Token = *LastToken;
    }

    if ((Token == NULL) || (*Token == L'\0')) {
        *LastToken = NULL;
        return NULL;
    }

    //
    // Advance past any separators.
    //

    Token += wcsspn(Token, Separators);

    //
    // If this is the end of the string, then there is no token.
    //

    if (*Token == L'\0') {
        *LastToken = NULL;
        return NULL;
    }

    //
    // Get the count of characters not in the set.
    //

    Count = wcscspn(Token, Separators);

    assert(Count != 0);

    //
    // If at the end of the string, return this last token and null out the
    // context pointer.
    //

    if (Token[Count] == L'\0') {
        *LastToken = NULL;

    //
    // Otherwise, null terminate the token and save the subsequent character
    // for next time.
    //

    } else {
        Token[Count] = L'\0';
        *LastToken = Token + Count + 1;
    }

    return Token;
}

LIBC_API
size_t
wcsxfrm (
    wchar_t *Result,
    const wchar_t *Input,
    size_t ResultSize
    )

/*++

Routine Description:

    This routine transforms the given input string in such a way that using
    strcmp on two transformed strings will return the same value as strcoll
    would return on the untransformed strings. The transformed string is not
    necessarily readable. It is used primarily if a string is going to be
    compared repeatedly, as it explicitly performs the transformation process
    once rather than on each strcoll comparison.

Arguments:

    Result - Supplies an optional pointer where the transformed string will be
        returned. This can be NULL to just get the size of the transformed
        string.

    Input - Supplies a pointer to the string to transform according to the
        current value of LC_COLLATE.

    ResultSize - Supplies the size of the result buffer in bytes. This routine
        will not write more than this number of bytes to the result buffer.

Return Value:

    Returns the size of the complete transform (even if a buffer is not
    supplied or is too small) not including the null terminator byte.

--*/

{

    size_t Length;

    //
    // TODO: Implement collating stuff.
    //

    Length = wcslen(Input);
    if ((Result != NULL) && (ResultSize != 0)) {
        wcsncpy(Result, Input, ResultSize);
    }

    return Length;
}

//
// --------------------------------------------------------- Internal Functions
//

