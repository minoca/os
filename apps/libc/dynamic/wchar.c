/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

typedef const struct _WC_INTERVAL {
    USHORT First;
    USHORT Last;
} WC_INTERVAL, *PWC_INTERVAL;

//
// ----------------------------------------------- Internal Function Prototypes
//

BOOL
ClpSearchCombiningIntervals (
    wchar_t Character,
    PWC_INTERVAL Table,
    LONG Max
    );

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
// Define the intervals of combining characters.
//

static WC_INTERVAL ClCombiningCharacters[] = {
    {0x0300, 0x034E}, {0x0360, 0x0362}, {0x0483, 0x0486},
    {0x0488, 0x0489}, {0x0591, 0x05A1}, {0x05A3, 0x05B9},
    {0x05BB, 0x05BD}, {0x05BF, 0x05BF}, {0x05C1, 0x05C2},
    {0x05C4, 0x05C4}, {0x064B, 0x0655}, {0x0670, 0x0670},
    {0x06D6, 0x06E4}, {0x06E7, 0x06E8}, {0x06EA, 0x06ED},
    {0x070F, 0x070F}, {0x0711, 0x0711}, {0x0730, 0x074A},
    {0x07A6, 0x07B0}, {0x0901, 0x0902}, {0x093C, 0x093C},
    {0x0941, 0x0948}, {0x094D, 0x094D}, {0x0951, 0x0954},
    {0x0962, 0x0963}, {0x0981, 0x0981}, {0x09BC, 0x09BC},
    {0x09C1, 0x09C4}, {0x09CD, 0x09CD}, {0x09E2, 0x09E3},
    {0x0A02, 0x0A02}, {0x0A3C, 0x0A3C}, {0x0A41, 0x0A42},
    {0x0A47, 0x0A48}, {0x0A4B, 0x0A4D}, {0x0A70, 0x0A71},
    {0x0A81, 0x0A82}, {0x0ABC, 0x0ABC}, {0x0AC1, 0x0AC5},
    {0x0AC7, 0x0AC8}, {0x0ACD, 0x0ACD}, {0x0B01, 0x0B01},
    {0x0B3C, 0x0B3C}, {0x0B3F, 0x0B3F}, {0x0B41, 0x0B43},
    {0x0B4D, 0x0B4D}, {0x0B56, 0x0B56}, {0x0B82, 0x0B82},
    {0x0BC0, 0x0BC0}, {0x0BCD, 0x0BCD}, {0x0C3E, 0x0C40},
    {0x0C46, 0x0C48}, {0x0C4A, 0x0C4D}, {0x0C55, 0x0C56},
    {0x0CBF, 0x0CBF}, {0x0CC6, 0x0CC6}, {0x0CCC, 0x0CCD},
    {0x0D41, 0x0D43}, {0x0D4D, 0x0D4D}, {0x0DCA, 0x0DCA},
    {0x0DD2, 0x0DD4}, {0x0DD6, 0x0DD6}, {0x0E31, 0x0E31},
    {0x0E34, 0x0E3A}, {0x0E47, 0x0E4E}, {0x0EB1, 0x0EB1},
    {0x0EB4, 0x0EB9}, {0x0EBB, 0x0EBC}, {0x0EC8, 0x0ECD},
    {0x0F18, 0x0F19}, {0x0F35, 0x0F35}, {0x0F37, 0x0F37},
    {0x0F39, 0x0F39}, {0x0F71, 0x0F7E}, {0x0F80, 0x0F84},
    {0x0F86, 0x0F87}, {0x0F90, 0x0F97}, {0x0F99, 0x0FBC},
    {0x0FC6, 0x0FC6}, {0x102D, 0x1030}, {0x1032, 0x1032},
    {0x1036, 0x1037}, {0x1039, 0x1039}, {0x1058, 0x1059},
    {0x1160, 0x11FF}, {0x17B7, 0x17BD}, {0x17C6, 0x17C6},
    {0x17C9, 0x17D3}, {0x180B, 0x180E}, {0x18A9, 0x18A9},
    {0x200B, 0x200F}, {0x202A, 0x202E}, {0x206A, 0x206F},
    {0x20D0, 0x20E3}, {0x302A, 0x302F}, {0x3099, 0x309A},
    {0xFB1E, 0xFB1E}, {0xFE20, 0xFE23}, {0xFEFF, 0xFEFF},
    {0xFFF9, 0xFFFB}
};

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
    if ((Result == -1) || (Result > 1)) {
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

    CHARACTER_ENCODING Encoding;

    if (WideCharacter == NULL) {
        RtlResetMultibyteState((PMULTIBYTE_STATE)&ClMultibyteConversionState);

        //
        // This should really get the LC_CTYPE encoding.
        //

        Encoding = RtlGetDefaultCharacterEncoding();
        if (RtlIsCharacterEncodingStateDependent(Encoding, FALSE) != FALSE) {
            return 1;
        }

        return 0;
    }

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

        return ByteCount - Size;
    }

    if (Status == STATUS_BUFFER_TOO_SMALL) {
        return -2;
    }

    errno = ClConvertKstatusToErrorNumber(Status);
    return -1;
}

LIBC_API
int
wctomb (
    char *MultibyteCharacter,
    wchar_t WideCharacter
    )

/*++

Routine Description:

    This routine attempts to convert a single wide character into a multibyte
    character.

Arguments:

    MultibyteCharacter - Supplies an optional pointer to the buffer where the
        multibyte character will be returned. This buffer is assumed to be at
        least MB_CUR_MAX bytes large. If this is NULL, then this function will
        determine whether or not the given character has state-dependent
        encodings.

    WideCharacter - Supplies a pointer to the wide character to convert. If this
        is a null terminator, then the shift state will be reset to its initial
        shift state.

Return Value:

    0 if the multibyte character is NULL and the character does not have state
    dependent encodings.

    Returns the number of bytes stored in the multibyte array, or that would
    be stored in the array were it non-NULL.

    -1 if an encoding error occurred, and errno may be set to EILSEQ.

--*/

{

    CHARACTER_ENCODING Encoding;

    if (MultibyteCharacter == NULL) {
        RtlResetMultibyteState((PMULTIBYTE_STATE)&ClMultibyteConversionState);

        //
        // This should really get the LC_CTYPE encoding.
        //

        Encoding = RtlGetDefaultCharacterEncoding();
        if (RtlIsCharacterEncodingStateDependent(Encoding, TRUE) != FALSE) {
            return 1;
        }

        return 0;
    }

    return wcrtomb(MultibyteCharacter, WideCharacter, NULL);
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

    if (MultibyteCharacter == NULL) {
        WideCharacter = L'\0';
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
    wchar_t *Destination,
    const char *Source,
    size_t DestinationSize
    )

/*++

Routine Description:

    This routine converts a null-terminated sequence of multi-byte characters
    beginning in the inital shift state to a string of wide characters, up to
    and including a null terminator.

Arguments:

    Destination - Supplies an optional pointer where the wide character string
        will be returned.

    Source - Supplies a pointer to the null-terminated multibyte string. No
        characters are examined after a null terminator is found.

    DestinationSize - Supplies the maximum number of elements to place in the
        wide string.

Return Value:

    Returns the number of wide character array elements modified (or required
    if the wide string is NULL), not including the terminating NULL.

    -1 if an invalid character is encountered. The errno variable may be set
    to provide more information.

--*/

{

    mbstate_t State;

    memset(&State, 0, sizeof(mbstate_t));
    return mbsrtowcs(Destination, &Source, DestinationSize, &State);
}

LIBC_API
size_t
mbsrtowcs (
    wchar_t *Destination,
    const char **Source,
    size_t DestinationSize,
    mbstate_t *State
    )

/*++

Routine Description:

    This routine converts a null-terminated sequence of multi-byte characters
    beginning in the inital shift state to a string of wide characters, up to
    and including a null terminator.

Arguments:

    Destination - Supplies an optional pointer where the wide character string
        will be returned.

    Source - Supplies a pointer that upon input contains a pointer to the null
        terminated multibyte string to convert. On output, this will contain
        one of two values. If the null terminator was encountered in the
        multibyte string, then the value returned here will be NULL. If the
        conversion stopped because it would exceed the wide string size, then
        the value returned here will be a pointer to the character one after
        the last character successfully converted. If the wide string is NULL,
        the pointer will remained unchanged on output.

    DestinationSize - Supplies the maximum number of elements to place in the
        wide string.

    State - Supplies an optional pointer to a multibyte shift state object to
        use. If this value is not supplied, an internal state will be used.
        The downside of using the internal state is that it makes this function
        not thread safe nor reentrant.

Return Value:

    Returns the number of wide character array elements modified (or required
    if the wide string is NULL), not including the terminating NULL.

    -1 if an invalid character is encountered. The errno variable may be set
    to provide more information.

--*/

{

    size_t ElementsConverted;
    const char *MultibyteString;
    size_t Result;
    wchar_t WideCharacter;

    ElementsConverted = 0;
    MultibyteString = *Source;
    while ((Destination == NULL) || (DestinationSize > 0)) {
        Result = mbrtowc(&WideCharacter, MultibyteString, MB_LEN_MAX, State);
        if (Result == -1) {
            return -1;
        }

        if (Destination != NULL) {
            *Destination = WideCharacter;
            Destination += 1;
            DestinationSize -= 1;
        }

        if (Result == 0) {
            break;
        }

        if (WideCharacter == L'\0') {
            MultibyteString = NULL;
            break;
        }

        MultibyteString += Result;
        ElementsConverted += 1;
    }

    if (Destination != NULL) {
        *Source = MultibyteString;
    }

    return ElementsConverted;
}

LIBC_API
size_t
wcstombs (
    char *Destination,
    const wchar_t *Source,
    size_t DestinationSize
    )

/*++

Routine Description:

    This routine converts a string of wide characters into a multibyte string,
    up to and including a wide null terminator.

Arguments:

    Destination - Supplies an optional pointer to a destination where the
        multibyte characters will be returned.

    Source - Supplies a pointer to the null terminated wide character string to
        convert.

    DestinationSize - Supplies the number of bytes in the destination buffer
        (or the theoretical destination buffer if one was not supplied).

Return Value:

    Returns the number of bytes in the resulting character sequence, not
    including the null terminator (if any).

    -1 if an invalid wide character is encountered. The errno variable may be
    set to provide more information.

--*/

{

    mbstate_t State;

    memset(&State, 0, sizeof(State));
    return wcsrtombs(Destination, &Source, DestinationSize, &State);
}

LIBC_API
size_t
wcsrtombs (
    char *Destination,
    const wchar_t **Source,
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
        after the last character successfully converted. If the destination
        is NULL, the pointer will remained unchanged on ouput.

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
    const wchar_t *WideString;

    if (State == NULL) {
        State = &ClMultibyteConversionState;
    }

    Result = 0;
    TotalWritten = 0;
    WideString = *Source;
    while ((Destination == NULL) || (DestinationSize > 0)) {
        PreviousState = *State;
        Result = wcrtomb(HoldingBuffer, *WideString, State);
        if (Result == -1) {
            errno = EILSEQ;
            break;

        } else if (Destination != NULL) {

            //
            // Copy the holding buffer to the destination if there's enough
            // room.
            //

            if (Result <= DestinationSize) {
                memcpy(Destination, HoldingBuffer, Result);
                Destination += Result;
                DestinationSize -= Result;

            //
            // The remaining size is not big enough to hold the character. Back
            // out the state advancement.
            //

            } else {
                *State = PreviousState;
                break;
            }
        }

        //
        // If this was a null terminator, stop.
        //

        if (*WideString == L'\0') {
            WideString = NULL;
            break;
        }

        //
        // Update the total bytes written. This never includes the null
        // terminator.
        //

        TotalWritten += Result;

        //
        // Advance the source string and continue.
        //

        WideString += 1;
    }

    //
    // Return the source string.
    //

    if (Destination != NULL) {
        *Source = WideString;
    }

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

LIBC_API
int
wcwidth (
    wchar_t Character
    )

/*++

Routine Description:

    This routine returns the number of display column positions the given wide
    character occupies.

Arguments:

    Character - Supplies the character to examine.

Return Value:

    0 for the null character.

    -1 if the character is not printable.

    Otherwise, returns the number of columns the given character takes up.

--*/

{

    LONG Max;

    //
    // This function is based on Markus Kuhn's function at
    // https://www.cl.cam.ac.uk/~mgk25/ucs/wcwidth.c, which was placed in the
    // public domain.
    //

    if (Character == 0) {
        return 0;
    }

    if ((Character < 0x20) || ((Character >= 0x7F) && (Character < 0xA0))) {
        return -1;
    }

    //
    // Search the non-spacing characters.
    //

    Max = sizeof(ClCombiningCharacters) / sizeof(ClCombiningCharacters[0]) - 1;
    if (ClpSearchCombiningIntervals(Character, ClCombiningCharacters, Max) !=
        FALSE) {

        return 0;
    }

    if (Character >= 0x1100) {
        if ((Character <= 0x115F) ||
            (Character == 0x2329) ||
            (Character == 0x232A) ||
            ((Character >= 0x2E80) && (Character <= 0xA4CF) &&
             (Character != 0x303F)) ||
            ((Character >= 0xAC00) && (Character <= 0xD7A3)) ||
            ((Character >= 0xF900) && (Character <= 0xFAFF)) ||
            ((Character >= 0xFE10) && (Character <= 0xFE19)) ||
            ((Character >= 0xFE30) && (Character <= 0xFE6F)) ||
            ((Character >= 0xFF00) && (Character <= 0xFF60)) ||
            ((Character >= 0xFFE0) && (Character <= 0xFFE6)) ||
            ((Character >= 0x20000) && (Character <= 0x2FFFD)) ||
            ((Character >= 0x30000) && (Character <= 0x3FFFD))) {

            return 2;
        }
    }

    return 1;
}

//
// --------------------------------------------------------- Internal Functions
//

BOOL
ClpSearchCombiningIntervals (
    wchar_t Character,
    PWC_INTERVAL Table,
    LONG Max
    )

/*++

Routine Description:

    This routine performs a binary search to determine if the given character
    is listed in the given table.

Arguments:

    Character - Supplies the character to examine.

    Table - Supplies a pointer to the table to search in.

    Max - Supplies the maximum index of the table, inclusive.

Return Value:

    TRUE if the character is in the table.

    FALSE if the character is not in the table.

--*/

{

    LONG Mid;
    LONG Min;

    if ((Character < Table[0].First) || (Character > Table[Max].Last)) {
        return FALSE;
    }

    Min = 0;
    while (Max >= Min) {
        Mid = (Min + Max) / 2;
        if (Character > Table[Mid].Last) {
            Min = Mid + 1;

        } else if (Character < Table[Mid].First) {
            Max = Mid - 1;

        } else {
            return TRUE;
        }
    }

    return FALSE;
}

