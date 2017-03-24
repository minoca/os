/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    string.c

Abstract:

    This module implements string and memory manipulation routines for the C
    library.

Author:

    Evan Green 5-Mar-2013

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

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
// Store the global string tokenizer global.
//

char *ClStringTokenizerContext;

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
void *
memchr (
    const void *Buffer,
    int Character,
    size_t Size
    )

/*++

Routine Description:

    This routine attempts to locate the first occurrence of the given character
    within the given buffer.

Arguments:

    Buffer - Supplies a pointer to the buffer of characters.

    Character - Supplies the character (converted to an unsigned char) to
        locate.

    Size - Supplies the size of the buffer.

Return Value:

    Returns a pointer to the first occurrence of the character within the
    buffer on success.

    NULL on failure.

--*/

{

    PSTR CharacterBuffer;

    CharacterBuffer = (PSTR)Buffer;
    while (Size != 0) {
        if ((unsigned char)*CharacterBuffer == (unsigned char)Character) {
            return CharacterBuffer;
        }

        CharacterBuffer += 1;
        Size -= 1;
    }

    return NULL;
}

LIBC_API
int
memcmp (
    const void *Left,
    const void *Right,
    size_t Size
    )

/*++

Routine Description:

    This routine compares two buffers of memory byte for byte.

Arguments:

    Left - Supplies the first buffer to compare.

    Right - Supplies the second buffer to compare.

    Size - Supplies the number of bytes to compare.

Return Value:

    >0 if Left > Right.

    0 is Left == Right.

    <0 if Left <= Right.

--*/

{

    int Difference;
    size_t Index;
    unsigned char *LeftCharacters;
    unsigned char *RightCharacters;

    LeftCharacters = (unsigned char *)Left;
    RightCharacters = (unsigned char *)Right;
    for (Index = 0; Index < Size; Index += 1) {
        Difference = *LeftCharacters - *RightCharacters;
        if (Difference != 0) {
            return Difference;
        }

        LeftCharacters += 1;
        RightCharacters += 1;
    }

    return 0;
}

LIBC_API
void *
memcpy (
    void *Destination,
    const void *Source,
    size_t ByteCount
    )

/*++

Routine Description:

    This routine copies bytes directly between buffers.

Arguments:

    Destination - Supplies a pointer to the destination of the copy.

    Source - Supplies a pointer to the source data to copy.

    ByteCount - Supplies the number of bytes to copy.

Return Value:

    Returns the destination parameter.

--*/

{

    return RtlCopyMemory(Destination, (PVOID)Source, ByteCount);
}

LIBC_API
void *
memmove (
    void *Destination,
    const void *Source,
    size_t ByteCount
    )

/*++

Routine Description:

    This routine copies bytes between buffers. Copying takes place as if the
    bytes are first copied into a temporary buffer that does not overlap the
    two buffers, and then are copied to the destination.

Arguments:

    Destination - Supplies a pointer to the destination of the copy.

    Source - Supplies a pointer to the source data to copy.

    ByteCount - Supplies the number of bytes to copy.

Return Value:

    Returns the destination parameter.

--*/

{

    PCHAR DestinationBytes;
    PCHAR SourceBytes;

    //
    // Copy the bytes backwards if the source begins before the destination
    // and overlaps.
    //

    if ((Source < Destination) && (Source + ByteCount > Destination)) {
        DestinationBytes = Destination;
        SourceBytes = (PCHAR)Source;
        while (ByteCount != 0) {
            DestinationBytes[ByteCount - 1] = SourceBytes[ByteCount - 1];
            ByteCount -= 1;
        }

    } else {
        RtlCopyMemory(Destination, (PVOID)Source, ByteCount);
    }

    return Destination;
}

LIBC_API
void *
memset (
    void *Destination,
    int Character,
    size_t ByteCount
    )

/*++

Routine Description:

    This routine copies the given character into each of the bytes in the
    destination buffer.

Arguments:

    Destination - Supplies a pointer to the destination of the copy.

    Character - Supplies a character (it will be cast down to a character) to
        fill the buffer with.

    ByteCount - Supplies the number of bytes to set.

Return Value:

    Returns the destination parameter.

--*/

{

    RtlSetMemory(Destination, Character, ByteCount);
    return Destination;
}

LIBC_API
int
bcmp (
    void *Buffer1,
    void *Buffer2,
    size_t Size
    )

/*++

Routine Description:

    This routine copares two regions of memory.

Arguments:

    Buffer1 - Supplies a pointer to the first buffer.

    Buffer2 - Supplies a pointer to the second buffer.

    Size - Supplies the size in bytes to compare.

Return Value:

    0 if the regions are identical.

    Non-zero if the regions are different.

--*/

{

    return memcmp(Buffer1, Buffer2, Size);
}

LIBC_API
void
bcopy (
    void *Source,
    void *Destination,
    size_t Size
    )

/*++

Routine Description:

    This routine copies a region of memory.

Arguments:

    Source - Supplies the source buffer.

    Destination - Supplies the destination buffer.

    Size - Supplies the size in bytes to copy.

Return Value:

    None.

--*/

{

    memmove(Destination, Source, Size);
    return;
}

LIBC_API
void
bzero (
    void *Buffer,
    size_t Size
    )

/*++

Routine Description:

    This routine zeroes a region of memory.

Arguments:

    Buffer - Supplies a pointer to the buffer to zero.

    Size - Supplies the size in bytes to zero.

Return Value:

    None.

--*/

{

    memset(Buffer, 0, Size);
    return;
}

LIBC_API
char *
index (
    const char *String,
    int Character
    )

/*++

Routine Description:

    This routine finds the first instance of the given character (converted to
    a char) in the given string.

Arguments:

    String - Supplies a pointer to the string to search for the character in.

    Character - Supplies the character to search for.

Return Value:

    Returns a pointer to the first occurrence of the character in the given
    string, or NULL if the character doesn't exist in the string.

--*/

{

    return strchr(String, Character);
}

LIBC_API
char *
rindex (
    const char *String,
    int Character
    )

/*++

Routine Description:

    This routine finds the last occurrence of the given character (converted to
    a char) in the given string.

Arguments:

    String - Supplies a pointer to the string to search for the character in.

    Character - Supplies the character to search for.

Return Value:

    Returns a pointer to the last occurrence of the character in the given
    string, or NULL if the character doesn't exist in the string.

--*/

{

    return strrchr(String, Character);
}

LIBC_API
char *
strchr (
    const char *String,
    int Character
    )

/*++

Routine Description:

    This routine finds the first instance of the given character (converted to
    a char) in the given string.

Arguments:

    String - Supplies a pointer to the string to search for the character in.

    Character - Supplies the character to search for.

Return Value:

    Returns a pointer to the first occurrence of the character in the given
    string, or NULL if the character doesn't exist in the string.

--*/

{

    while (TRUE) {
        if (*String == (char)Character) {
            return (char *)String;
        }

        if (*String == '\0') {
            break;
        }

        String += 1;
    }

    return NULL;
}

LIBC_API
char *
strrchr (
    const char *String,
    int Character
    )

/*++

Routine Description:

    This routine finds the last occurrence of the given character (converted to
    a char) in the given string.

Arguments:

    String - Supplies a pointer to the string to search for the character in.

    Character - Supplies the character to search for.

Return Value:

    Returns a pointer to the last occurrence of the character in the given
    string, or NULL if the character doesn't exist in the string.

--*/

{

    char *LastOccurrence;

    LastOccurrence = NULL;
    while (TRUE) {
        if (*String == (char)Character) {
            LastOccurrence = (char *)String;
        }

        if (*String == '\0') {
            break;
        }

        String += 1;
    }

    return LastOccurrence;
}

LIBC_API
size_t
strlen (
    const char *String
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

    return RtlStringLength((PSTR)String);
}

LIBC_API
size_t
strnlen (
    const char *String,
    size_t MaxLength
    )

/*++

Routine Description:

    This routine computes the length of the given string, not including the
    null terminator, but will only examine up to the given maximum number of
    bytes.

Arguments:

    String - Supplies a pointer to the string whose length should be computed.

    MaxLength - Supplies the maximum number of bytes to examine.

Return Value:

    Returns the length of the string, not including the null terminator, or
    the maximum length provided if no null terminator was found.

--*/

{

    size_t Size;

    Size = 0;
    while ((Size < MaxLength) && (String[Size] != '\0')) {
        Size += 1;
    }

    return Size;
}

LIBC_API
char *
strcpy (
    char *DestinationString,
    const char *SourceString
    )

/*++

Routine Description:

    This routine copies the given source string over the given destination
    string. This routine should be avoided if at all possible as it can be the
    cause of buffer overflow problems. Use functions like strncpy that place
    explicit bounds on the destination buffer.

Arguments:

    DestinationString - Supplies a pointer where the source string will be
        copied to.

    SourceString - Supplies the string that will be copied.

Return Value:

    Returns the destination string.

--*/

{

    char *Current;

    Current = DestinationString;
    while (*SourceString != '\0') {
        *Current = *SourceString;
        Current += 1;
        SourceString += 1;
    }

    *Current = '\0';
    return DestinationString;
}

LIBC_API
char *
stpcpy (
    char *DestinationString,
    const char *SourceString
    )

/*++

Routine Description:

    This routine copies the given source string over the given destination
    string. This routine should be avoided if at all possible as it can be the
    cause of buffer overflow problems. Use functions like stpncpy that place
    explicit bounds on the destination buffer.

Arguments:

    DestinationString - Supplies a pointer where the source string will be
        copied to.

    SourceString - Supplies the string that will be copied.

Return Value:

    Returns a pointer to the end of the destination string.

--*/

{

    while (TRUE) {
        *DestinationString = *SourceString;
        if (*DestinationString == '\0') {
            break;
        }

        DestinationString += 1;
        SourceString += 1;
    }

    return DestinationString;
}

LIBC_API
char *
strncpy (
    char *DestinationString,
    const char *SourceString,
    size_t BytesToCopy
    )

/*++

Routine Description:

    This routine copies the given source string over the given destination
    string.

Arguments:

    DestinationString - Supplies a pointer where the source string will be
        copied to.

    SourceString - Supplies the string that will be copied.

    BytesToCopy - Supplies the maximum number of bytes to copy. If the source
        string is shorter than this value, then only bytes up to and including
        the null terminator will be copied. The remaining bytes will be zeroed.
        If the source string is longer than this value, then the destination
        string will not be null terminated.

Return Value:

    Returns the destination string.

--*/

{

    size_t ByteIndex;

    for (ByteIndex = 0; ByteIndex < BytesToCopy; ByteIndex += 1) {
        DestinationString[ByteIndex] = SourceString[ByteIndex];
        if (SourceString[ByteIndex] == '\0') {
            break;
        }
    }

    while (ByteIndex < BytesToCopy) {
        DestinationString[ByteIndex] = '\0';
        ByteIndex += 1;
    }

    return DestinationString;
}

LIBC_API
char *
stpncpy (
    char *DestinationString,
    const char *SourceString,
    size_t BytesToCopy
    )

/*++

Routine Description:

    This routine copies the given source string over the given destination
    string. It fills any remaining space with zeroes.

Arguments:

    DestinationString - Supplies a pointer where the source string will be
        copied to.

    SourceString - Supplies the string that will be copied.

    BytesToCopy - Supplies the maximum number of bytes to copy. If the source
        string is shorter than this value, then only bytes up to and including
        the null terminator will be copied. If the source string is longer
        than this value, then the destination string will not be null
        terminated.

Return Value:

    Returns a pointer to the end of the destination string.

--*/

{

    size_t ByteIndex;
    size_t EndIndex;

    for (ByteIndex = 0; ByteIndex < BytesToCopy; ByteIndex += 1) {
        DestinationString[ByteIndex] = SourceString[ByteIndex];
        if (SourceString[ByteIndex] == '\0') {
            break;
        }
    }

    EndIndex = ByteIndex;
    while (ByteIndex < BytesToCopy) {
        DestinationString[ByteIndex] = '\0';
        ByteIndex += 1;
    }

    return DestinationString + EndIndex;
}

LIBC_API
char *
strcat (
    char *DestinationString,
    const char *SourceString
    )

/*++

Routine Description:

    This routine appends bytes to the end of the given string. The destination
    string will always be returned with a null terminator.

Arguments:

    DestinationString - Supplies a pointer containing the string that will be
        appended to.

    SourceString - Supplies a pointer to the string to append.

Return Value:

    Returns the destination string.

--*/

{

    return strncat(DestinationString, SourceString, MAX_ULONG);
}

LIBC_API
char *
strncat (
    char *DestinationString,
    const char *SourceString,
    size_t BytesToAppend
    )

/*++

Routine Description:

    This routine appends bytes to the end of the given string. The destination
    string will always be returned with a null terminator.

Arguments:

    DestinationString - Supplies a pointer containing the string that will be
        appended to.

    SourceString - Supplies a pointer to the string to append.

    BytesToAppend - Supplies the number of bytes of the source string to
        append to the destination, NOT including the null terminator. This
        means that the destination string buffer must be at least large enough
        to take this number plus one bytes on the end of the existing string. If
        the source string is shorter than this value, this routine will stop at
        the terminator.

Return Value:

    Returns the destination string.

--*/

{

    int ByteIndex;
    char *OriginalDestination;

    OriginalDestination = DestinationString;

    //
    // First find the end of the string.
    //

    while (*DestinationString != '\0') {
        DestinationString += 1;
    }

    //
    // Now copy as many bytes as are requested over.
    //

    for (ByteIndex = 0; ByteIndex < BytesToAppend; ByteIndex += 1) {

        //
        // Stop if the source ended.
        //

        if (*SourceString == '\0') {
            break;
        }

        *DestinationString = *SourceString;
        DestinationString += 1;
        SourceString += 1;
    }

    //
    // Always null terminate the destination.
    //

    *DestinationString = '\0';
    return OriginalDestination;
}

LIBC_API
int
strcmp (
    const char *String1,
    const char *String2
    )

/*++

Routine Description:

    This routine compares two strings for equality.

Arguments:

    String1 - Supplies the first string to compare.

    String2 - Supplies the second string to compare.

Return Value:

    0 if the strings are equal all the way through their null terminators.

    Non-zero if the strings are different. The sign of the return value will be
    determined by the sign of the difference between the values of the first
    pair of bytes (both interpreted as type unsigned char) that differ in the
    strings being compared.

--*/

{

    return strncmp(String1, String2, (size_t)-1);
}

LIBC_API
int
stricmp (
    const char *String1,
    const char *String2
    )

/*++

Routine Description:

    This routine compares two strings for equality, ignoring case.

Arguments:

    String1 - Supplies the first string to compare.

    String2 - Supplies the second string to compare.

Return Value:

    0 if the strings are equal all the way through their null terminators.

    Non-zero if the strings are different. The sign of the return value will be
    determined by the sign of the difference between the values of the first
    pair of bytes (both interpreted as type unsigned char) that differ in the
    strings being compared.

--*/

{

    return strncasecmp(String1, String2, (size_t)-1);
}

LIBC_API
int
strncmp (
    const char *String1,
    const char *String2,
    size_t CharacterCount
    )

/*++

Routine Description:

    This routine compares two strings for equality, up to a bounded amount.

Arguments:

    String1 - Supplies the first string to compare.

    String2 - Supplies the second string to compare.

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
            return (unsigned char)*String1 - (unsigned char)*String2;
        }

        if (*String1 == '\0') {
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
strnicmp (
    const char *String1,
    const char *String2,
    size_t CharacterCount
    )

/*++

Routine Description:

    This routine compares two strings for equality, ignoring case, up to a
    bounded amount.

Arguments:

    String1 - Supplies the first string to compare.

    String2 - Supplies the second string to compare.

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

    return strncasecmp(String1, String2, CharacterCount);
}

LIBC_API
int
strcasecmp (
    const char *String1,
    const char *String2
    )

/*++

Routine Description:

    This routine compares two strings for equality, ignoring case. This routine
    will act for the purposes of comparison like all characters are converted
    to lowercase.

Arguments:

    String1 - Supplies the first string to compare.

    String2 - Supplies the second string to compare.

Return Value:

    0 if the strings are equal all the way through their null terminators.

    Non-zero if the strings are different. The sign of the return value will be
    determined by the sign of the difference between the values of the first
    pair of bytes (both interpreted as type unsigned char) that differ in the
    strings being compared.

--*/

{

    return strncasecmp(String1, String2, -1);
}

LIBC_API
int
strncasecmp (
    const char *String1,
    const char *String2,
    size_t CharacterCount
    )

/*++

Routine Description:

    This routine compares two strings for equality, ignoring case, up to a
    bounded amount. This routine will act for the purposes of comparison like
    all characters are converted to lowercase.

Arguments:

    String1 - Supplies the first string to compare.

    String2 - Supplies the second string to compare.

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

    unsigned char Character1;
    unsigned char Character2;

    while (CharacterCount != 0) {
        Character1 = tolower(*String1);
        Character2 = tolower(*String2);
        if (Character1 != Character2) {
            return Character1 - Character2;
        }

        if (Character1 == '\0') {
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
strcoll (
    const char *String1,
    const char *String2
    )

/*++

Routine Description:

    This routine compares two strings, both interpreted as appropriate to the
    LC_COLLATE category of the current locale.

Arguments:

    String1 - Supplies a pointer to the first string.

    String2 - Supplies a pointer to the second string.

Return Value:

    >0 if the first string is greater than the second string.

    0 if the first string is equal to the second string.

    <0 if the first string is less than the second string.

--*/

{

    //
    // TODO: Implement collating stuff.
    //

    return strcmp(String1, String2);
}

LIBC_API
char *
strdup (
    const char *String
    )

/*++

Routine Description:

    This routine returns a pointer to a newly allocated string which is a
    duplicate of the given input string. This returned pointer must be passed
    to the free function when the caller is done with it.

Arguments:

    String - Supplies a pointer to the string to duplicate.

Return Value:

    Returns a pointer to the newly allocated duplicate string on success.

    NULL on failure.

--*/

{

    size_t Length;
    char *NewString;

    if (String == NULL) {
        Length = 1;

    } else {
        Length = strlen(String) + 1;
    }

    NewString = malloc(Length);
    if (NewString == NULL) {
        return NULL;
    }

    strcpy(NewString, String);
    return NewString;
}

LIBC_API
char *
strndup (
    const char *String,
    size_t Size
    )

/*++

Routine Description:

    This routine returns a pointer to a newly allocated string which is a
    duplicate of the given input string. This returned pointer must be passed
    to the free function when the caller is done with it.

Arguments:

    String - Supplies a pointer to the string to duplicate.

    Size - Supplies the maximum number of bytes to copy before terminating the
        string.

Return Value:

    Returns a pointer to the newly allocated duplicate string on success.

    NULL on failure.

--*/

{

    size_t Length;
    char *NewString;

    Length = 0;
    if (String != NULL) {
        Length = strlen(String);
    }

    if (Length > Size) {
        Length = Size;
    }

    NewString = malloc(Length + 1);
    if (NewString == NULL) {
        return NULL;
    }

    memcpy(NewString, String, Length);
    NewString[Length] = '\0';
    return NewString;
}

LIBC_API
char *
strpbrk (
    const char *String,
    const char *Characters
    )

/*++

Routine Description:

    This routine locates the first occurrence in the given string of any byte
    from the given character set.

Arguments:

    String - Supplies a pointer to the string to search.

    Characters - Supplies a pointer to a null terminated string containing the
        acceptable set of characters.

Return Value:

    Returns a pointer within the given string to the first character in the
    requested set.

    NULL if no bytes from the set occur in the given string.

--*/

{

    const char *CurrentCharacter;

    while (*String != '\0') {
        CurrentCharacter = Characters;
        while (*CurrentCharacter != '\0') {
            if (*String == *CurrentCharacter) {
                return (char *)String;
            }

            CurrentCharacter += 1;
        }

        String += 1;
    }

    return NULL;
}

LIBC_API
size_t
strcspn (
    const char *Input,
    const char *Characters
    )

/*++

Routine Description:

    This routine computes the length in bytes of the initial portion of the
    given input that's made up only of characters not in the given set. For
    example, an input of "abc123" and a set of "0123456789" would return a value
    of 3.

Arguments:

    Input - Supplies a pointer to a null terminated string containing the
        string to query.

    Characters - Supplies a pointer to a null terminated string containing the
        set of characters.

Return Value:

    Returns the count of initial characters in the string not in the given
    set.

--*/

{

    size_t Count;
    const char *CurrentCharacter;

    Count = 0;
    while (*Input != '\0') {
        CurrentCharacter = Characters;
        while (*CurrentCharacter != '\0') {
            if (*Input == *CurrentCharacter) {
                break;
            }

            CurrentCharacter += 1;
        }

        if (*CurrentCharacter != '\0') {
            break;
        }

        Count += 1;
        Input += 1;
    }

    return Count;
}

LIBC_API
size_t
strspn (
    const char *Input,
    const char *Characters
    )

/*++

Routine Description:

    This routine computes the length in bytes of the initial portion of the
    given input that's made up only of characters from the given set. For
    example, an input of "129th" and a set of "0123456789" would return a value
    of 3.

Arguments:

    Input - Supplies a pointer to a null terminated string containing the
        string to query.

    Characters - Supplies a pointer to a null terminated string containing the
        acceptable set of characters.

Return Value:

    Returns the count of initial characters in the string in the given set.

--*/

{

    size_t Count;
    const char *CurrentCharacter;

    Count = 0;
    while (*Input != '\0') {
        CurrentCharacter = Characters;
        while (*CurrentCharacter != '\0') {
            if (*Input == *CurrentCharacter) {
                break;
            }

            CurrentCharacter += 1;
        }

        if (*CurrentCharacter == '\0') {
            break;
        }

        Count += 1;
        Input += 1;
    }

    return Count;
}

LIBC_API
char *
strstr (
    const char *InputString,
    const char *QueryString
    )

/*++

Routine Description:

    This routine attempts to find the first occurrence of the query string in
    the given input string.

Arguments:

    InputString - Supplies a pointer to the input string to search.

    QueryString - Supplies a pointer to the query string to search for.

Return Value:

    Returns a pointer within the input string to the first instance of the
    query string.

    NULL if no instances of the query string were found in the input string.

--*/

{

    size_t InputStringLength;
    size_t QueryStringLength;
    char *Result;

    if ((QueryString == NULL) || (InputString == NULL)) {
        return NULL;
    }

    InputStringLength = strlen(InputString) + 1;
    QueryStringLength = strlen(QueryString) + 1;
    Result =  RtlStringSearch((PSTR)InputString,
                              InputStringLength,
                              (PSTR)QueryString,
                              QueryStringLength);

    return Result;
}

LIBC_API
char *
strcasestr (
    const char *InputString,
    const char *QueryString
    )

/*++

Routine Description:

    This routine attempts to find the first occurrence of the query string in
    the given input string. This routine is case insensitive.

Arguments:

    InputString - Supplies a pointer to the input string to search.

    QueryString - Supplies a pointer to the query string to search for.

Return Value:

    Returns a pointer within the input string to the first instance of the
    query string.

    NULL if no instances of the query string were found in the input string.

--*/

{

    size_t InputStringLength;
    size_t QueryStringLength;
    char *Result;

    if ((QueryString == NULL) || (InputString == NULL)) {
        return NULL;
    }

    InputStringLength = strlen(InputString) + 1;
    QueryStringLength = strlen(QueryString) + 1;
    Result =  RtlStringSearchIgnoringCase((PSTR)InputString,
                                          InputStringLength,
                                          (PSTR)QueryString,
                                          QueryStringLength);

    return Result;
}

LIBC_API
char *
strtok (
    char *InputString,
    const char *Separators
    )

/*++

Routine Description:

    This routine breaks a string into a series of tokens delimited by any
    character from the given separator set. The first call passes an input
    string in. This routine scans looking for a non-separator character, which
    marks the first token. It then scans looking for a separator character, and
    sets that byte to the null terminator to delimit the first character.
    Subsequent calls should pass NULL as the input string, and the context
    pointer will be updated so that successive calls return the next tokens.
    This routine is neither thread safe nor reentrant.

Arguments:

    InputString - Supplies a pointer to the input string to tokenize. If
        supplied, this will reset the tokenizer function.

    Separators - Supplies a pointer to a null terminated string containing the
        set of characters that delimit tokens. This may vary from call to call
        of this routine with the same context pointer.

Return Value:

    Returns a pointer to the next token on success.

    NULL if there are no more tokens.

--*/

{

    return strtok_r(InputString, Separators, &ClStringTokenizerContext);
}

LIBC_API
char *
strtok_r (
    char *InputString,
    const char *Separators,
    char **LastToken
    )

/*++

Routine Description:

    This routine breaks a string into a series of tokens delimited by any
    character from the given separator set. The first call passes an input
    string in. This routine scans looking for a non-separator character, which
    marks the first token. It then scans looking for a separator character, and
    sets that byte to the null terminator to delimit the first character.
    Subsequent calls should pass NULL as the input string, and the context
    pointer will be updated so that successive calls return the next tokens.
    This routine is thread safe and re-entrant so long as the same context
    pointer is not used by multiple threads.

Arguments:

    InputString - Supplies a pointer to the input string to tokenize. If
        supplied, this will reset the value returned in the last token context
        pointer.

    Separators - Supplies a pointer to a null terminated string containing the
        set of characters that delimit tokens. This may vary from call to call
        of this routine with the same context pointer.

    LastToken - Supplies a pointer where a context pointer will be stored
        allowing this routine to keep its place and return successive tokens.

Return Value:

    Returns a pointer to the next token on success.

    NULL if there are no more tokens.

--*/

{

    size_t Count;
    PSTR Token;

    Token = InputString;
    if (Token == NULL) {
        Token = *LastToken;
    }

    if ((Token == NULL) || (*Token == '\0')) {
        *LastToken = NULL;
        return NULL;
    }

    //
    // Advance past any separators.
    //

    Token += strspn(Token, Separators);

    //
    // If this is the end of the string, then there is no token.
    //

    if (*Token == '\0') {
        *LastToken = NULL;
        return NULL;
    }

    //
    // Get the count of characters not in the set.
    //

    Count = strcspn(Token, Separators);

    assert(Count != 0);

    //
    // If at the end of the string, return this last token and null out the
    // context pointer.
    //

    if (Token[Count] == '\0') {
        *LastToken = NULL;

    //
    // Otherwise, null terminate the token and save the subsequent character
    // for next time.
    //

    } else {
        Token[Count] = '\0';
        *LastToken = Token + Count + 1;
    }

    return Token;
}

LIBC_API
char *
strsep (
    char **InputString,
    const char *Delimiters
    )

/*++

Routine Description:

    This routine breaks a string into a series of tokens delimited by any
    character from the given delimiter set. It scans looking for a delimiter
    character and sets that byte to the null terminator to delimit the first
    token. This may result in an empty field where the returned token is made
    up of just the null terminator. This routine is thread safe and re-entrant
    so long as the input string is not used by multiple threads.

Arguments:

    InputString - Supplies a pointer to a pointer to the input string to
        tokenize. On output, this will point to the character after the
        modified delimiter or NULL if the end of the string was reached without
        finding a delimiter.

    Delimiters - Supplies a pointer to a null terminated string containing the
        set of characters that delimit tokens.

Return Value:

    Returns a pointer to the the original input string (now delimited).

    NULL if there are no more tokens or no string was supplied.

--*/

{

    size_t Count;
    PSTR OriginalString;
    PSTR Token;

    if ((InputString == NULL) || (*InputString == NULL)) {
        return NULL;
    }

    //
    // The original string is always returned. Save it.
    //

    OriginalString = *InputString;
    Token = OriginalString;

    //
    // Get the count of characters not in the set. This may be 0, indicating an
    // empty field. The original string is still returned and the character is
    // still converted to the null terminator, unless of course it is already
    // the null terminator.
    //

    Count = strcspn(Token, Delimiters);

    //
    // If this is the end of the string, then there are no more tokens. The
    // input string is set to NULL so the next call fails.
    //

    if (Token[Count] == '\0') {
        *InputString = NULL;

    //
    // Otherwise, null terminate the token and save the subsequent character
    // for the next time.
    //

    } else {
        Token[Count] = '\0';
        *InputString = Token + Count + 1;
    }

    return OriginalString;
}

LIBC_API
size_t
strxfrm (
    char *Result,
    const char *Input,
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

    Length = strlen(Input);
    if ((Result != NULL) && (ResultSize != 0)) {
        strncpy(Result, Input, ResultSize);
    }

    return Length;
}

LIBC_API
void
swab (
    const void *Source,
    void *Destination,
    ssize_t ByteCount
    )

/*++

Routine Description:

    This routine copies bytes from a source buffer to a destination, exchanging
    adjacent bytes. The source and destination buffers should not overlap.

Arguments:

    Source - Supplies the source buffer.

    Destination - Supplies the destination buffer (which should not overlap
        with the source buffer).

    ByteCount - Supplies the number of bytes to copy. This should be even. If
        it is odd, the byte count will be truncated down to an even boundary,
        so the last odd byte will not be copied.

Return Value:

    None.

--*/

{

    char *DestinationBytes;
    ssize_t Index;
    const char *SourceBytes;

    DestinationBytes = Destination;
    SourceBytes = Source;

    //
    // Truncate down to an even boundary.
    //

    ByteCount &= ~0x1;
    for (Index = 0; Index < ByteCount; Index += 2) {
        DestinationBytes[Index] = SourceBytes[Index + 1];
        DestinationBytes[Index + 1] = SourceBytes[Index];
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

