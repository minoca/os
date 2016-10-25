/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    wchar.c

Abstract:

    This module implements support for wide character conversion functions.

Author:

    Evan Green 27-Aug-2013

Environment:

    Any

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "rtlp.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
RtlpConvertAsciiMultibyteCharacterToWide (
    PCHAR *MultibyteCharacter,
    PULONG MultibyteBufferSize,
    PWCHAR WideCharacter,
    PMULTIBYTE_STATE State
    );

KSTATUS
RtlpConvertAsciiWideCharacterToMultibyte (
    WCHAR WideCharacter,
    PCHAR MultibyteCharacter,
    PULONG Size,
    PMULTIBYTE_STATE State
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define the default multibyte encoding scheme.
//

ULONG RtlDefaultEncoding = CharacterEncodingAscii;

//
// ------------------------------------------------------------------ Functions
//

RTL_API
VOID
RtlInitializeMultibyteState (
    PMULTIBYTE_STATE State,
    CHARACTER_ENCODING Encoding
    )

/*++

Routine Description:

    This routine initializes a multibyte state structure.

Arguments:

    State - Supplies the a pointer to the state to reset.

    Encoding - Supplies the encoding to use for multibyte sequences. If the
        default value is supplied here, then the current default system
        encoding will be used.

Return Value:

    None.

--*/

{

    if (Encoding == CharacterEncodingDefault) {
        Encoding = RtlDefaultEncoding;
    }

    RtlZeroMemory(State, sizeof(MULTIBYTE_STATE));
    State->Encoding = Encoding;
    return;
}

RTL_API
CHARACTER_ENCODING
RtlGetDefaultCharacterEncoding (
    VOID
    )

/*++

Routine Description:

    This routine returns the system default character encoding.

Arguments:

    None.

Return Value:

    Returns the current system default character encoding.

--*/

{

    return RtlDefaultEncoding;
}

RTL_API
KSTATUS
RtlSetDefaultCharacterEncoding (
    CHARACTER_ENCODING NewEncoding,
    PCHARACTER_ENCODING OriginalEncoding
    )

/*++

Routine Description:

    This routine sets the system default character encoding.

Arguments:

    NewEncoding - Supplies the new encoding to use.

    OriginalEncoding - Supplies an optional pointer where the previous
        character encoding will be returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NOT_SUPPORTED if the given character encoding is not supported on
    this system.

--*/

{

    ULONG PreviousValue;

    if (RtlIsCharacterEncodingSupported(NewEncoding) == FALSE) {
        return STATUS_NOT_SUPPORTED;
    }

    PreviousValue = RtlAtomicExchange32(&RtlDefaultEncoding, NewEncoding);
    if (OriginalEncoding != NULL) {
        *OriginalEncoding = PreviousValue;
    }

    return STATUS_SUCCESS;
}

RTL_API
BOOL
RtlIsCharacterEncodingSupported (
    CHARACTER_ENCODING Encoding
    )

/*++

Routine Description:

    This routine determines if the system supports a given character encoding.

Arguments:

    Encoding - Supplies the encoding to query.

Return Value:

    TRUE if the parameter is a valid encoding.

    FALSE if the system does not recognize the given encoding.

--*/

{

    if ((Encoding > CharacterEncodingDefault) &&
        (Encoding < CharacterEncodingMax)) {

        return TRUE;
    }

    return FALSE;
}

RTL_API
BOOL
RtlIsCharacterEncodingStateDependent (
    CHARACTER_ENCODING Encoding,
    BOOL ToMultibyte
    )

/*++

Routine Description:

    This routine determines if the given character encoding is state-dependent
    when converting between multibyte sequences and wide characters.

Arguments:

    Encoding - Supplies the encoding to query.

    ToMultibyte - Supplies a boolean indicating the direction of the character
        encoding. State-dependence can vary between converting to multibyte and
        converting to wide character.

Return Value:

    TRUE if the given encoding is valid and state-dependent.

    FALSE if the given encoding is invalid or not state-dependent.

--*/

{

    BOOL Result;

    if (Encoding == CharacterEncodingDefault) {
        Encoding = RtlDefaultEncoding;
    }

    switch (Encoding) {
    case CharacterEncodingAscii:
        Result = FALSE;
        break;

    default:
        Result = FALSE;
        break;
    }

    return Result;
}

RTL_API
VOID
RtlResetMultibyteState (
    PMULTIBYTE_STATE State
    )

/*++

Routine Description:

    This routine resets the given multibyte state back to its initial state,
    without clearing the character encoding.

Arguments:

    State - Supplies a pointer to the state to reset.

Return Value:

    None.

--*/

{

    RtlInitializeMultibyteState(State, State->Encoding);
    return;
}

RTL_API
BOOL
RtlIsMultibyteStateReset (
    PMULTIBYTE_STATE State
    )

/*++

Routine Description:

    This routine determines if the given multibyte state is in its initial
    reset state.

Arguments:

    State - Supplies a pointer to the state to query.

Return Value:

    TRUE if the state is in the initial shift state.

    FALSE if the state is not in the initial shift state.

--*/

{

    return TRUE;
}

RTL_API
KSTATUS
RtlConvertMultibyteCharacterToWide (
    PCHAR *MultibyteCharacter,
    PULONG MultibyteBufferSize,
    PWCHAR WideCharacter,
    PMULTIBYTE_STATE State
    )

/*++

Routine Description:

    This routine converts a multibyte sequence into a wide character.

Arguments:

    MultibyteCharacter - Supplies a pointer that on input contains a pointer
        to the multibyte character sequence. On successful output, this pointer
        will be advanced beyond the character.

    MultibyteBufferSize - Supplies a pointer that on input contains the size of
        the multibyte buffer in bytes. This value will be updated if the
        returned multibyte character buffer is advanced.

    WideCharacter - Supplies an optional pointer where the wide character will
        be returned on success.

    State - Supplies a pointer to the state to use.

Return Value:

    STATUS_SUCCESS if a wide character was successfully converted.

    STATUS_INVALID_PARAMETER if the multibyte state is invalid.

    STATUS_BUFFER_TOO_SMALL if only a portion of a character could be
    constructed given the number of bytes in the buffer. The buffer will not
    be advanced in this case.

    STATUS_MALFORMED_DATA_STREAM if the byte sequence is not valid.

--*/

{

    KSTATUS Status;

    if (State->Encoding == CharacterEncodingDefault) {
        State->Encoding = RtlDefaultEncoding;
    }

    switch (State->Encoding) {
    case CharacterEncodingAscii:
        Status = RtlpConvertAsciiMultibyteCharacterToWide(MultibyteCharacter,
                                                          MultibyteBufferSize,
                                                          WideCharacter,
                                                          State);

        break;

    default:
        Status = STATUS_INVALID_PARAMETER;
        break;
    }

    return Status;
}

RTL_API
KSTATUS
RtlConvertWideCharacterToMultibyte (
    WCHAR WideCharacter,
    PCHAR MultibyteCharacter,
    PULONG Size,
    PMULTIBYTE_STATE State
    )

/*++

Routine Description:

    This routine converts a wide character into a multibyte sequence.

Arguments:

    WideCharacter - Supplies the wide character to convert to a multibyte
        sequence.

    MultibyteCharacter - Supplies a pointer to the multibyte sequence.

    Size - Supplies a pointer that on input contains the size of the buffer.
        On output, it will return the number of bytes in the multibyte
        character, even if the buffer provided was too small.

    State - Supplies a pointer to the state to use.

Return Value:

    STATUS_SUCCESS if a wide character was successfully converted.

    STATUS_INVALID_PARAMETER if the multibyte state is invalid.

    STATUS_BUFFER_TOO_SMALL if only a portion of a character could be
    constructed given the remaining space in the buffer. The buffer will not
    be advanced in this case.

    STATUS_MALFORMED_DATA_STREAM if the byte sequence is not valid.

--*/

{

    KSTATUS Status;

    if (State->Encoding == CharacterEncodingDefault) {
        State->Encoding = RtlDefaultEncoding;
    }

    switch (State->Encoding) {
    case CharacterEncodingAscii:
        Status = RtlpConvertAsciiWideCharacterToMultibyte(WideCharacter,
                                                          MultibyteCharacter,
                                                          Size,
                                                          State);

        break;

    default:
        Status = STATUS_INVALID_PARAMETER;
        break;
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
RtlpConvertAsciiMultibyteCharacterToWide (
    PCHAR *MultibyteCharacter,
    PULONG MultibyteBufferSize,
    PWCHAR WideCharacter,
    PMULTIBYTE_STATE State
    )

/*++

Routine Description:

    This routine converts an ASCII character into a wide character.

Arguments:

    MultibyteCharacter - Supplies a pointer that on input contains a pointer
        to the multibyte character sequence. On successful output, this pointer
        will be advanced beyond the character.

    MultibyteBufferSize - Supplies a pointer that on input contains the size of
        the multibyte buffer in bytes. This value will be updated if the
        returned multibyte character buffer is advanced.

    WideCharacter - Supplies an optional pointer where the wide character will
        be returned on success.

    State - Supplies a pointer to the state to use.

Return Value:

    STATUS_SUCCESS if a wide character was successfully converted.

    STATUS_BUFFER_TOO_SMALL if only a portion of a character could be
    constructed given the number of bytes in the buffer. The buffer will not
    be advanced in this case.

    STATUS_MALFORMED_DATA_STREAM if the byte sequence is not valid.

--*/

{

    PCHAR Buffer;
    CHAR Character;
    ULONG Size;
    KSTATUS Status;

    Buffer = *MultibyteCharacter;
    Size = *MultibyteBufferSize;
    if (Size == 0) {
        Status = STATUS_BUFFER_TOO_SMALL;
        goto ConvertAsciiMultibyteCharacterToWideEnd;
    }

    Character = *Buffer;
    Buffer += 1;
    Size -= 1;
    if (RtlIsCharacterAscii(Character) == FALSE) {
        Status = STATUS_MALFORMED_DATA_STREAM;
        goto ConvertAsciiMultibyteCharacterToWideEnd;
    }

    if (WideCharacter != NULL) {
        *WideCharacter = Character;
    }

    Status = STATUS_SUCCESS;

ConvertAsciiMultibyteCharacterToWideEnd:
    if (KSUCCESS(Status)) {
        *MultibyteCharacter = Buffer;
        *MultibyteBufferSize = Size;
    }

    return Status;
}

KSTATUS
RtlpConvertAsciiWideCharacterToMultibyte (
    WCHAR WideCharacter,
    PCHAR MultibyteCharacter,
    PULONG Size,
    PMULTIBYTE_STATE State
    )

/*++

Routine Description:

    This routine converts a wide character into a multibyte sequence.

Arguments:

    WideCharacter - Supplies the wide character to convert to a multibyte
        sequence.

    MultibyteCharacter - Supplies a pointer to the multibyte sequence.

    Size - Supplies a pointer that on input contains the size of the buffer.
        On output, it will return the number of bytes in the multibyte
        character, even if the buffer provided was too small.

    State - Supplies a pointer to the state to use.

Return Value:

    STATUS_SUCCESS if a wide character was successfully converted.

    STATUS_INVALID_PARAMETER if the multibyte state is invalid.

    STATUS_BUFFER_TOO_SMALL if only a portion of a character could be
    constructed given the remaining space in the buffer. The buffer will not
    be advanced in this case.

    STATUS_MALFORMED_DATA_STREAM if the byte sequence is not valid.

--*/

{

    KSTATUS Status;

    if (*Size < 1) {
        Status = STATUS_BUFFER_TOO_SMALL;
        goto ConvertAsciiWideCharacterToMultibyteEnd;
    }

    if (RtlIsCharacterAsciiWide(WideCharacter) == FALSE) {
        Status = STATUS_MALFORMED_DATA_STREAM;
        goto ConvertAsciiWideCharacterToMultibyteEnd;
    }

    if (MultibyteCharacter != NULL) {
        *MultibyteCharacter = (CHAR)WideCharacter;
    }

    Status = STATUS_SUCCESS;

ConvertAsciiWideCharacterToMultibyteEnd:
    *Size = 1;
    return Status;
}

