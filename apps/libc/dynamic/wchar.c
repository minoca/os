/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    wchar.c

Abstract:

    This module implements support for wide and multibyte characters.

Author:

    Evan Green 23-Aug-2013

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

//
// --------------------------------------------------------------------- Macros
//

//
// This is really a compile-time macro that ensure the mbstate_t structure is
// big enough to contain the MULTIBYTE_STATE structure the runtime library
// defines.
//

#define ASSERT_MBSTATE_SIZE() \
    assert(sizeof(mbstate_t) >= sizeof(MULTIBYTE_STATE))

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
// Define the maximum number of bytes in a multibyte character for the current
// locale.
//

LIBC_API int MB_CUR_MAX = MB_LEN_MAX;

//
// Store the internal character conversion state.
//

mbstate_t ClMultibyteConversionState;

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
int
mbsinit (
    const mbstate_t *State
    )

/*++

Routine Description:

    This routine determines if the given state structure is in its initial
    shift state.

Arguments:

    State - Supplies a pointer to the state to query.

Return Value:

    Returns non-zero if the given state was a NULL pointer or is in its initial
    conversion state.

    0 if the given state is not in its initial conversion state.

--*/

{

    ASSERT_MBSTATE_SIZE();

    if (State == NULL) {
        return 1;
    }

    if (RtlIsMultibyteStateReset((PMULTIBYTE_STATE)State) != FALSE) {
        return 1;
    }

    return 0;
}

LIBC_API
wint_t
btowc (
    int Character
    )

/*++

Routine Description:

    This routine attempts to convert a single byte into a wide character at
    the initial shift state.

Arguments:

    Character - Supplies the character.

Return Value:

    Returns the wide character representation of the character.

    WEOF if the input character is EOF or if the character (cast to an unsigned
    char) does not constitute a valid one byte character in the initial shift
    state.

--*/

{

    size_t Count;
    mbstate_t State;
    wchar_t WideCharacter;

    if (Character == EOF) {
        return WEOF;
    }

    memset(&State, 0, sizeof(mbstate_t));
    Count = mbrtowc(&WideCharacter, (const char *)&Character, 1, &State);
    if ((Count != 0) && (Count != 1)) {
        return WEOF;
    }

    return (wint_t)WideCharacter;
}

LIBC_API
int
wctob (
    wint_t Character
    )

/*++

Routine Description:

    This routine converts the given wide character into its corresponding
    single-byte character if possible, starting at the initial shift state.

Arguments:

    Character - Supplies the wide character to convert to a byte.

Return Value:

    Returns the byte representation of the character.

    EOF if the wide character is invalid or cannot be represented in a single
    byte.

--*/

{

    CHAR MultibyteCharacter[MULTIBYTE_MAX];
    size_t Result;
    mbstate_t State;

    memset(&State, 0, sizeof(mbstate_t));
    Result = wcrtomb(MultibyteCharacter, Character, &State);
    if ((Result < 0) || (Result > 1)) {
        return EOF;
    }

    if (Result == 0) {
        return '\0';
    }

    return MultibyteCharacter[0];
}

LIBC_API
size_t
mbtowc (
    wchar_t *WideCharacter,
    const char *MultibyteCharacter,
    size_t ByteCount
    )

/*++

Routine Description:

    This routine attempts to convert a multibyte character into a wide
    character. This routine is equivalent to calling mbrtowc with a NULL
    state pointer.

Arguments:

    WideCharacter - Supplies an optional pointer wehre the converted wide
        character will be returned on success.

    MultibyteCharacter - Supplies a pointer to the multibyte character to
        convert.

    ByteCount - Supplies the maximum number of bytes to inspect in the
        multibyte character buffer.

Return Value:

    0 if the next character is the null character.

    Returns a positive value on success indicating the number of bytes that
    were used to construct the wide character.

    -2 if the byte count was too small, as the multibyte character could only
    be partially assembled with the given maximum number of bytes.

    -1 if an encoding error occurred.

--*/

{

    return mbrtowc(WideCharacter, MultibyteCharacter, ByteCount, NULL);
}

LIBC_API
size_t
mbrtowc (
    wchar_t *WideCharacter,
    const char *MultibyteCharacter,
    size_t ByteCount,
    mbstate_t *State
    )

/*++

Routine Description:

    This routine attempts to convert a multibyte character into a wide
    character.

Arguments:

    WideCharacter - Supplies an optional pointer wehre the converted wide
        character will be returned on success.

    MultibyteCharacter - Supplies a pointer to the multibyte character to
        convert.

    ByteCount - Supplies the maximum number of bytes to inspect in the
        multibyte character buffer.

    State - Supplies an optional pointer to a multibyte shift state object to
        use. If this value is not supplied, an internal state will be used.
        The downside of using the internal state is that it makes this function
        not thread safe nor reentrant.

Return Value:

    0 if the next character is the null character.

    Returns a positive value on success indicating the number of bytes that
    were used to construct the wide character.

    -2 if the byte count was too small, as the multibyte character could only
    be partially assembled with the given maximum number of bytes.

    -1 if an encoding error occurred.

--*/

{

    WCHAR LocalWideCharacter;
    PMULTIBYTE_STATE MultibyteState;
    ULONG Size;
    KSTATUS Status;

    ASSERT_MBSTATE_SIZE();

    if (State == NULL) {
        State = &ClMultibyteConversionState;
    }

    if (MultibyteCharacter == NULL) {
        if (ByteCount != 0) {
            errno = EILSEQ;
            return -1;
        }

        memset(State, 0, sizeof(mbstate_t));
        return 0;
    }

    Size = ByteCount;
    MultibyteState = (PMULTIBYTE_STATE)State;
    Status = RtlConvertMultibyteCharacterToWide((PCHAR *)&MultibyteCharacter,
                                                &Size,
                                                &LocalWideCharacter,
                                                MultibyteState);

    if (KSUCCESS(Status)) {
        if (WideCharacter != NULL) {
            *WideCharacter = LocalWideCharacter;
        }

        if (LocalWideCharacter == L'\0') {
            return 0;
        }

        return Size;
    }

    if (Status == STATUS_BUFFER_TOO_SMALL) {
        return -2;
    }

    errno = ClConvertKstatusToErrorNumber(Status);
    return -1;
}

LIBC_API
size_t
wcrtomb (
    char *MultibyteCharacter,
    wchar_t WideCharacter,
    mbstate_t *State
    )

/*++

Routine Description:

    This routine attempts to convert a single wide character into a multibyte
    character.

Arguments:

    MultibyteCharacter - Supplies an optional pointer to the buffer where the
        multibyte character will be returned. This buffer is assumed to be at
        least MB_CUR_MAX bytes large. If this is NULL, then functionality will
        be equivalent to wcrtomb(Buffer, L'\0', State), where Buffer is an
        internal buffer.

    WideCharacter - Supplies a pointer to the wide character to convert. If this
        is a null terminator, then the shift state will be reset to its initial
        shift state.

    State - Supplies an optional pointer to a multibyte shift state object to
        use. If this value is not supplied, an internal state will be used.
        The downside of using the internal state is that it makes this function
        not thread safe nor reentrant.

Return Value:

    Returns the number of bytes stored in the multibyte array.

    -1 if an encoding error occurred, and errno may be set to EILSEQ.

--*/

{

    PMULTIBYTE_STATE MultibyteState;
    ULONG Size;
    KSTATUS Status;

    ASSERT_MBSTATE_SIZE();

    if (State == NULL) {
        State = &ClMultibyteConversionState;
    }

    MultibyteState = (PMULTIBYTE_STATE)State;
    Size = MULTIBYTE_MAX;
    Status = RtlConvertWideCharacterToMultibyte(WideCharacter,
                                                MultibyteCharacter,
                                                &Size,
                                                MultibyteState);

    if (KSUCCESS(Status)) {
        return Size;
    }

    errno = ClConvertKstatusToErrorNumber(Status);
    return -1;
}

LIBC_API
size_t
mbstowcs (
    wchar_t *WideString,
    const char *MultibyteString,
    size_t WideStringSize
    )

/*++

Routine Description:

    This routine converts a null-terminated sequence of multi-byte characters
    beginning in the inital shift state to a string of wide characters.

Arguments:

    WideString - Supplies an optional pointer where the wide character string
        will be returned.

    MultibyteString - Supplies a pointer to the null-terminated multibyte
        string. No characters are examined after a null terminator is found.

    WideStringSize - Supplies the maximum number of elements to place in the
        wide string.

Return Value:

    Returns the number of wide character array elements modified (or required
    if the wide string is NULL).

    -1 if an invalid character is encountered. The errno variable may be set
    to provide more information.

--*/

{

    mbstate_t State;

    memset(&State, 0, sizeof(mbstate_t));
    return mbsrtowcs(WideString, MultibyteString, WideStringSize, &State);
}

LIBC_API
size_t
mbsrtowcs (
    wchar_t *WideString,
    const char *MultibyteString,
    size_t WideStringSize,
    mbstate_t *State
    )

/*++

Routine Description:

    This routine converts a null-terminated sequence of multi-byte characters
    at the given shift state to a string of wide characters.

Arguments:

    WideString - Supplies an optional pointer where the wide character string
        will be returned.

    MultibyteString - Supplies a pointer to the null-terminated multibyte
        string. No characters are examined after a null terminator is found.

    WideStringSize - Supplies the maximum number of elements to place in the
        wide string.

    State - Supplies an optional pointer to a multibyte shift state object to
        use. If this value is not supplied, an internal state will be used.
        The downside of using the internal state is that it makes this function
        not thread safe nor reentrant.

Return Value:

    Returns the number of wide character array elements modified (or required
    if the wide string is NULL).

    -1 if an invalid character is encountered. The errno variable may be set
    to provide more information.

--*/

{

    size_t ElementsConverted;
    size_t Result;

    ElementsConverted = 0;
    while (TRUE) {
        Result = mbrtowc(WideString, MultibyteString, MB_LEN_MAX, State);
        if (Result < 0) {
            return -1;
        }

        if (Result == 0) {
            break;
        }

        MultibyteString += Result;
        if (WideString != NULL) {
            WideString += 1;
        }

        ElementsConverted += 1;
    }

    return ElementsConverted;
}

LIBC_API
size_t
wcsrtombs (
    char *Destination,
    wchar_t **Source,
    size_t DestinationSize,
    mbstate_t *State
    )

/*++

Routine Description:

    This routine converts a string of wide characters into a multibyte string,
    up to and including a wide null terminator.

Arguments:

    Destination - Supplies an optional pointer to a destination where the
        multibyte characters will be returned.

    Source - Supplies a pointer that upon input contains a pointer to the
        null terminated wide character string to convert. On output, this will
        contain one of two values. If the null terminator was encountered in
        the source string, then the value returned here will be NULL. If the
        conversion stopped because it would exceed the destination size,
        then the value returned here will be a pointer to the character one
        after the last character successfully converted.

    DestinationSize - Supplies the number of bytes in the destination buffer
        (or the theoretical destination buffer if one was not supplied).

    State - Supplies an optional pointer to a multibyte shift state object to
        use. If this value is not supplied, an internal state will be used.
        The downside of using the internal state is that it makes this function
        not thread safe nor reentrant.

Return Value:

    Returns the number of bytes in the resulting character sequence, not
    including the null terminator (if any).

    -1 if an invalid wide character is encountered. The errno variable may be
    set to provide more information.

--*/

{

    char HoldingBuffer[MB_LEN_MAX];
    mbstate_t PreviousState;
    size_t Result;
    size_t TotalWritten;
    wchar_t *WideString;

    if (State == NULL) {
        State = &ClMultibyteConversionState;
    }

    Result = 0;
    TotalWritten = 0;
    WideString = *Source;
    while (DestinationSize > 0) {
        PreviousState = *State;
        Result = wcrtomb(HoldingBuffer, *WideString, State);
        if (Result == -1) {
            errno = EILSEQ;
            break;

        //
        // Copy the holding buffer to the destination if there's enough room.
        //

        } else if (Result <= DestinationSize) {
            if (Destination != NULL) {
                memcpy(Destination, HoldingBuffer, Result);
            }

            Destination += Result;
            DestinationSize -= Result;
            TotalWritten += Result;

        //
        // The remaining size is not big enough to hold the character. Back
        // out the state advancement.
        //

        } else {
            *State = PreviousState;
            break;
        }

        //
        // If this was a null terminator, stop.
        //

        if (*WideString == L'\0') {
            WideString = NULL;
            break;
        }

        //
        // Advance the source string and continue.
        //

        WideString += 1;
    }

    //
    // Return the source string.
    //

    *Source = WideString;
    if (Result == -1) {
        return -1;
    }

    return TotalWritten;
}

LIBC_API
int
mblen (
    const char *MultibyteCharacter,
    size_t Size
    )

/*++

Routine Description:

    This routine returns the number of bytes constituting the given multibyte
    character. It shall be equivalent to:
    mbtowc(NULL, MultibyteCharacter, Size);

    except that the builtin state of mbtowc is not affected.

Arguments:

    MultibyteCharacter - Supplies an optional pointer to the multibyte
        character to get the length of.

    Size - Supplies the size of the multibyte character buffer.

Return Value:

    0 if the next character corresponds to the null wide character.

    Returns the positive number of bytes constituting the next character on
    success.

    -2 if the size of the buffer is too small, such that only a partial wide
    character could be constructed using the given bytes.

    -1 on error, and errno will be set to contain more information.

--*/

{

    mbstate_t State;

    memset(&State, 0, sizeof(mbstate_t));
    return (int)mbrtowc(NULL, MultibyteCharacter, Size, &State);
}

LIBC_API
size_t
mbrlen (
    const char *MultibyteCharacter,
    size_t Size,
    mbstate_t *State
    )

/*++

Routine Description:

    This routine returns the number of bytes constituting the given multibyte
    character. It shall be equivalent to:
    mbrtowc(NULL, MultibyteCharacter, Size, State);.

Arguments:

    MultibyteCharacter - Supplies an optional pointer to the multibyte
        character to get the length of.

    Size - Supplies the size of the multibyte character buffer.

    State - Supplies an optional pointer to an initialized multibyte conversion
        state buffer. If this is not supplied, an internal state buffer will
        be used, however using the internal one makes this function neither
        safe nor reentrant.

Return Value:

    0 if the next character corresponds to the null wide character.

    Returns the positive number of bytes constituting the next character on
    success.

    -2 if the size of the buffer is too small, such that only a partial wide
    character could be constructed using the given bytes.

    -1 on error, and errno will be set to contain more information.

--*/

{

    return mbrtowc(NULL, MultibyteCharacter, Size, State);
}

//
// --------------------------------------------------------- Internal Functions
//

