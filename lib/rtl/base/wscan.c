/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    wscan.c

Abstract:

    This module implements scanning wide strings into various other forms, such
    as integers.

Author:

    Evan Green 27-Aug-2013

Environment:

    Kernel and User Modes

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "rtlp.h"

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

#define INFINITY_STRING L"infinity"
#define INFINITY_STRING_LENGTH 8
#define NAN_STRING L"nan"
#define NAN_STRING_LENGTH 3

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
RtlpScanIntegerWide (
    PSCAN_INPUT Input,
    ULONG Base,
    ULONG FieldSize,
    BOOL Signed,
    PLONGLONG Integer,
    PULONG CharactersConsumed
    );

__NOINLINE
KSTATUS
RtlpScanDoubleWide (
    PSCAN_INPUT Input,
    ULONG FieldSize,
    double *Double,
    PULONG CharactersConsumed
    );

VOID
RtlpScannerUnputWide (
    PSCAN_INPUT Input,
    WCHAR Character
    );

BOOL
RtlpScannerGetInputWide (
    PSCAN_INPUT Input,
    PWCHAR Character
    );

BOOL
RtlpStringScannerGetInputWide (
    PSCAN_INPUT Input,
    PWCHAR Character
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

RTL_API
KSTATUS
RtlStringScanWide (
    PCWSTR Input,
    ULONG InputSize,
    PCWSTR Format,
    ULONG FormatSize,
    CHARACTER_ENCODING Encoding,
    PULONG ItemsScanned,
    ...
    )

/*++

Routine Description:

    This routine scans in a wide string and converts it to a number of arguments
    based on a format string.

Arguments:

    Input - Supplies a pointer to the input wide string to scan.

    InputSize - Supplies the size of the string in characters including the
        null terminator.

    Format - Supplies the format string that specifies how to convert the input
        to the arguments.

    FormatSize - Supplies the size of the format string in characters,
        including the null terminator.

    Encoding - Supplies the character encoding to use when scanning non-wide
        items.

    ItemsScanned - Supplies a pointer where the number of items scanned will
        be returned (not counting any %n specifiers).

    ... - Supplies the remaining pointer arguments where the scanned data will
        be returned.

Return Value:

    STATUS_SUCCESS if the input was successfully scanned according to the
    format.

    STATUS_INVALID_SEQUENCE if the input did not match the format or the
    format was invalid.

    STATUS_ARGUMENT_EXPECTED if not enough arguments were supplied for the
        format.

    STATUS_END_OF_FILE if the input ended before any arguments were converted
    or any matching failures occurred.

--*/

{

    va_list ArgumentList;
    KSTATUS Status;

    va_start(ArgumentList, ItemsScanned);
    Status = RtlStringScanVaListWide(Input,
                                     InputSize,
                                     Format,
                                     FormatSize,
                                     Encoding,
                                     ItemsScanned,
                                     ArgumentList);

    va_end(ArgumentList);
    return Status;
}

RTL_API
KSTATUS
RtlStringScanVaListWide (
    PCWSTR Input,
    ULONG InputSize,
    PCWSTR Format,
    ULONG FormatSize,
    CHARACTER_ENCODING Encoding,
    PULONG ItemsScanned,
    va_list Arguments
    )

/*++

Routine Description:

    This routine scans in a wide string and converts it to a number of arguments
    based on a format string.

Arguments:

    Input - Supplies a pointer to the wide input string to scan.

    InputSize - Supplies the size of the string in characters including the
        null terminator.

    Format - Supplies the wide format string that specifies how to convert the
        input to the arguments.

    FormatSize - Supplies the size of the format string in characters,
        including the null terminator.

    Encoding - Supplies the character encoding to use when scanning non-wide
        items.

    ItemsScanned - Supplies a pointer where the number of items scanned will
        be returned (not counting any %n specifiers).

    Arguments - Supplies the initialized arguments list where various pieces
        of the formatted string will be returned.

Return Value:

    STATUS_SUCCESS if the input was successfully scanned according to the
    format.

    STATUS_INVALID_SEQUENCE if the input did not match the format or the
    format was invalid.

    STATUS_ARGUMENT_EXPECTED if not enough arguments were supplied for the
        format.

    STATUS_END_OF_FILE if the input ended before any arguments were converted
    or any matching failures occurred.

--*/

{

    SCAN_INPUT InputParameters;
    KSTATUS Status;

    if (Input == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    InputParameters.ReadU.GetInputWide = RtlpStringScannerGetInputWide;
    InputParameters.DataU.WideString = Input;
    InputParameters.StringSize = InputSize;
    InputParameters.ValidUnputCharacters = 0;
    InputParameters.CharactersRead = 0;
    RtlInitializeMultibyteState(&(InputParameters.State), Encoding);
    Status = RtlScanWide(&InputParameters,
                         Format,
                         FormatSize,
                         ItemsScanned,
                         Arguments);

    return Status;
}

RTL_API
KSTATUS
RtlStringScanIntegerWide (
    PCWSTR *String,
    PULONG StringSize,
    ULONG Base,
    BOOL Signed,
    PLONGLONG Integer
    )

/*++

Routine Description:

    This routine converts a wide string into an integer. It scans past leading
    whitespace.

Arguments:

    String - Supplies a pointer that on input contains a pointer to the string
        to scan. On output, the string advanced past the scanned value (if any)
        will be returned. If the entire string is whitespace or starts with an
        invalid character, this parameter will not be modified.

    StringSize - Supplies a pointer that on input contains the size of the
        string, in characters , including the null terminator. On output, this
        will contain the size of the string minus the number of bytes scanned by
        this routine. Said differently, it will return the size of the output
        to the string parameter in bytes including the null terminator.

    Base - Supplies the base of the integer to scan. Valid values are zero and
        two through thirty six. If zero is supplied, this routine will attempt
        to automatically detect what the base is out of bases 8, 10, and 16.

    Signed - Supplies a boolean indicating whether the integer to scan is
        signed or not.

    STATUS_INTEGER_OVERFLOW if the result overflowed. In this case the integer
    returned will be MAX_LONGLONG, MIN_LONGLONG, or MAX_ULONGLONG depending on
    the signedness and value.

    Integer - Supplies a pointer where the resulting integer is returned on
        success.

Return Value:

    STATUS_SUCCESS if an integer was successfully scanned.

    STATUS_INVALID_SEQUENCE if a valid integer could not be scanned.

    STATUS_END_OF_FILE if the input ended before the value was converted
    or a matching failure occurred.

--*/

{

    ULONG CharactersConsumed;
    SCAN_INPUT Input;
    KSTATUS Status;

    if (String == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    Input.ReadU.GetInputWide = RtlpStringScannerGetInputWide;
    Input.DataU.WideString = *String;
    Input.StringSize = *StringSize;
    Input.ValidUnputCharacters = 0;
    Input.CharactersRead = 0;
    RtlInitializeMultibyteState(&(Input.State), CharacterEncodingDefault);
    Status = RtlpScanIntegerWide(&Input,
                                 Base,
                                 *StringSize,
                                 Signed,
                                 Integer,
                                 &CharactersConsumed);

    *StringSize -= CharactersConsumed;
    *String += CharactersConsumed;
    return Status;
}

RTL_API
KSTATUS
RtlStringScanDoubleWide (
    PCWSTR *String,
    PULONG StringSize,
    double *Double
    )

/*++

Routine Description:

    This routine converts a wide string into a floating point double. It scans
    past leading whitespace.

Arguments:

    String - Supplies a pointer that on input contains a pointer to the wide
        string to scan. On output, the string advanced past the scanned value
        (if any) will be returned. If the entire string is whitespace or starts
        with an invalid character, this parameter will not be modified.

    StringSize - Supplies a pointer that on input contains the size of the
        string, in characters, including the null terminator. On output, this
        will contain the size of the string minus the number of bytes scanned
        by this routine. Said differently, it will return the size of the output
        to the string parameter in bytes including the null terminator.

    Double - Supplies a pointer where the resulting double is returned on
        success.

Return Value:

    STATUS_SUCCESS if an integer was successfully scanned.

    STATUS_INVALID_SEQUENCE if a valid integer could not be scanned.

    STATUS_END_OF_FILE if the input ended before the value was converted
    or a matching failure occurred.

--*/

{

    ULONG CharactersConsumed;
    SCAN_INPUT Input;
    KSTATUS Status;

    if (String == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    Input.ReadU.GetInputWide = RtlpStringScannerGetInputWide;
    Input.DataU.WideString = *String;
    Input.StringSize = *StringSize;
    Input.ValidUnputCharacters = 0;
    Input.CharactersRead = 0;
    RtlInitializeMultibyteState(&(Input.State), CharacterEncodingDefault);
    Status = RtlpScanDoubleWide(&Input,
                                *StringSize,
                                Double,
                                &CharactersConsumed);

    *StringSize -= CharactersConsumed;
    *String += CharactersConsumed;
    return Status;
}

RTL_API
KSTATUS
RtlScanWide (
    PSCAN_INPUT Input,
    PCWSTR Format,
    ULONG FormatLength,
    PULONG ItemsScanned,
    va_list ArgumentList
    )

/*++

Routine Description:

    This routine scans from an input and converts the input to various
    parameters according to a specified format.

Arguments:

    Input - Supplies a pointer to the filled out scan input structure which
        will be used to retrieve more input.

    Format - Supplies the wide format string which specifies how to convert the
        input to the argument list.

    FormatLength - Supplies the size of the format length string in characters,
        including the null terminator.

    ItemsScanned - Supplies a pointer where the number of parameters filled in
        (not counting %n) will be returned.

    ArgumentList - Supplies the list of arguments that will get filled out
        based on the input and format string.

Return Value:

    STATUS_SUCCESS if the input was successfully scanned according to the
    format.

    STATUS_INVALID_SEQUENCE if the input did not match the format or the
    format was invalid.

    STATUS_ARGUMENT_EXPECTED if not enough arguments were supplied for the
    format.

    STATUS_END_OF_FILE if the input ended before any arguments were converted
    or any matching failures occurred.

--*/

{

    BOOL AdvanceFormat;
    PWCHAR Argument;
    ULONG ArgumentsWritten;
    BOOL AssignmentSuppression;
    ULONG Base;
    PCHAR ByteArgument;
    ULONG ByteArgumentSize;
    WCHAR Character;
    ULONG CharactersConsumed;
    double Double;
    ULONG FieldWidth;
    WCHAR InputCharacter;
    BOOL InScanSet;
    LONGLONG Integer;
    ULONG LengthModifier;
    BOOL LongSpecified;
    ULONG Position;
    BOOL Result;
    PCWSTR ScanSetBegin;
    BOOL ScanSetGotSomething;
    ULONG ScanSetIndex;
    ULONG ScanSetLength;
    BOOL ScanSetNegated;
    KSTATUS ScanStatus;
    BOOL Signed;
    KSTATUS Status;

    ArgumentsWritten = 0;
    *ItemsScanned = 0;
    Result = FALSE;

    //
    // Loop getting characters.
    //

    while (FormatLength != 0) {
        AssignmentSuppression = FALSE;
        FieldWidth = -1;
        LengthModifier = 0;
        Position = -1;
        Character = *Format;

        //
        // Any whitespace in the format blasts through all whitespace in the
        // input.
        //

        if (RtlIsCharacterSpaceWide(Character) != FALSE) {
            do {
                Result = RtlpScannerGetInputWide(Input, &InputCharacter);
                if (Result == FALSE) {
                    break;
                }

            } while (RtlIsCharacterSpaceWide(InputCharacter) != FALSE);

            //
            // This went one too far, put the non-whitespace character back.
            //

            if (Result != FALSE) {
                RtlpScannerUnputWide(Input, InputCharacter);
            }

        //
        // If it's a terminator, stop scanning.
        //

        } else if (Character == L'\0') {
            break;

        //
        // If it's not a percent, then it's just a regular character, match it
        // up.
        //

        } else if (Character != L'%') {
            Result = RtlpScannerGetInputWide(Input, &InputCharacter);
            if (Result == FALSE) {
                Status = STATUS_END_OF_FILE;
                goto ScanWideEnd;
            }

            if (InputCharacter != Character) {
                Status = STATUS_INVALID_SEQUENCE;
                goto ScanWideEnd;
            }

        //
        // Big boy land, it's a format specifier (percent sign).
        //

        } else {

            ASSERT(Character == L'%');

            Format += 1;
            FormatLength -= 1;
            if ((FormatLength == 0) || (*Format == L'\0')) {
                Status = STATUS_INVALID_SEQUENCE;
                goto ScanWideEnd;
            }

            Character = *Format;

            //
            // Potentially get a positional argument (or field length, it's
            // unclear yet).
            //

            if ((Character >= L'0') && (Character <= L'9')) {
                ScanStatus = RtlStringScanIntegerWide(&Format,
                                                      &FormatLength,
                                                      10,
                                                      FALSE,
                                                      &Integer);

                if (!KSUCCESS(ScanStatus)) {
                    Status = ScanStatus;
                    goto ScanWideEnd;
                }

                if ((FormatLength == 0) || (*Format == L'\0')) {
                    Status = STATUS_END_OF_FILE;
                    goto ScanWideEnd;
                }

                if (Integer <= 0) {
                    Status = STATUS_INVALID_SEQUENCE;
                    goto ScanWideEnd;
                }

                //
                // A dollar sign means it was a positional argument, none means
                // it was a field width.
                //

                Character = *Format;
                if (Character == L'$') {
                    Position = (ULONG)Integer;
                    Format += 1;
                    FormatLength -= 1;
                    if (FormatLength == 0) {
                        Status = STATUS_INVALID_SEQUENCE;
                        goto ScanWideEnd;
                    }

                    Character = *Format;
                    if (Character == L'\0') {
                        Status = STATUS_INVALID_SEQUENCE;
                        goto ScanWideEnd;
                    }

                } else {
                    FieldWidth = (ULONG)Integer;
                }
            }

            //
            // Watch out for assignment suppression.
            //

            if (Character == L'*') {
                AssignmentSuppression = TRUE;
                Format += 1;
                FormatLength -= 1;
                if (FormatLength == 0) {
                    Status = STATUS_INVALID_SEQUENCE;
                    goto ScanWideEnd;
                }

                Character = *Format;
            }

            //
            // If not already found, try again to scan a field width, as it
            // could have been after the asterisk.
            //

            if ((FieldWidth == -1) &&
                (Character >= L'0') && (Character <= L'9')) {

                ScanStatus = RtlStringScanIntegerWide(&Format,
                                                      &FormatLength,
                                                      10,
                                                      FALSE,
                                                      &Integer);

                if (!KSUCCESS(ScanStatus)) {
                    Status = ScanStatus;
                    goto ScanWideEnd;
                }

                if ((FormatLength == 0) || (*Format == L'\0')) {
                    Status = STATUS_END_OF_FILE;
                    goto ScanWideEnd;
                }

                if (Integer <= 0) {
                    Status = STATUS_INVALID_SEQUENCE;
                    goto ScanWideEnd;
                }

                FieldWidth = (ULONG)Integer;
                Character = *Format;
            }

            //
            // Look for a length modifier. There are two-character wide
            // length modifiers hh for char and ll for long long.
            //

            AdvanceFormat = FALSE;
            LongSpecified = FALSE;

            //
            // If the character is 'h', then the parameter points to a short.
            // Advance manually here to look for 'hh' as well, which makes the
            // parameter a char.
            //

            if (Character == L'h') {
                LengthModifier = sizeof(SHORT);
                Format += 1;
                FormatLength -= 1;
                if ((FormatLength == 0) || (*Format == L'\0')) {
                    Status = STATUS_INVALID_SEQUENCE;
                    goto ScanWideEnd;
                }

                Character = *Format;
                if (Character == L'h') {
                    LengthModifier = sizeof(CHAR);
                    AdvanceFormat = TRUE;
                }

            //
            // If the character is 'l', then the parameter points to a long.
            // Advance manually here as well to look for the 'll', which is
            // long long.
            //

            } else if (Character == L'l') {
                LongSpecified = TRUE;
                LengthModifier = sizeof(LONG);
                Format += 1;
                FormatLength -= 1;
                if ((FormatLength == 0) || (*Format == L'\0')) {
                    Status = STATUS_INVALID_SEQUENCE;
                    goto ScanWideEnd;
                }

                Character = *Format;
                if (Character == L'l') {
                    LongSpecified = FALSE;
                    LengthModifier = sizeof(LONGLONG);
                    AdvanceFormat = TRUE;
                }

            //
            // The 'j' override specifies an intmax_t type.
            //

            } else if (Character == L'j') {
                LengthModifier = sizeof(intmax_t);
                AdvanceFormat = TRUE;

            //
            // The 'z' override specifies a size_t type.
            //

            } else if (Character == L'z') {
                LengthModifier = sizeof(size_t);
                AdvanceFormat = TRUE;

            //
            // The 't' override specifies a ptrdiff_t type.
            //

            } else if (Character == L't') {
                LengthModifier = sizeof(ptrdiff_t);
                AdvanceFormat = TRUE;

            //
            // The 'L' override specifies a long double.
            //

            } else if (Character == L'L') {
                LengthModifier = sizeof(LONGLONG);
                AdvanceFormat = TRUE;
            }

            if (AdvanceFormat != FALSE) {
                Format += 1;
                FormatLength -= 1;
                if ((FormatLength == 0) || (*Format == L'\0')) {
                    Status = STATUS_INVALID_SEQUENCE;
                    goto ScanWideEnd;
                }

                Character = *Format;
            }

            //
            // Get the argument unless the assignment is suppressed.
            //

            Argument = NULL;
            if ((AssignmentSuppression == FALSE) && (Character != L'%')) {

                //
                // TODO: Handle positional arguments. Not sure how to do
                // that with just va_arg. If they're all pointer types
                // perhaps pointer arithmetic is fine if there were a way
                // to get the original address of the first argument.
                //

                if (Position != -1) {

                    ASSERT(FALSE);

                    Argument = NULL;

                } else {
                    Argument = va_arg(ArgumentList, PWCHAR);
                }

                if (Argument == NULL) {
                    Status = STATUS_ARGUMENT_EXPECTED;
                    goto ScanWideEnd;
                }
            }

            //
            // Convert lc to C and ls to S for if statement convenience.
            //

            if (LongSpecified != FALSE) {
                if (Character == L'c') {
                    Character = L'C';

                } else if (Character == L's') {
                    Character = L'S';
                }
            }

            //
            // All the wiggly stuff is out of the way, get down to the real
            // format specifier. First check for an integer.
            //

            if ((Character == L'd') || (Character == L'i') ||
                (Character == L'o') || (Character == L'u') ||
                (Character == L'x') || (Character == L'X')) {

                if (LengthModifier == 0) {
                    LengthModifier = sizeof(INT);
                }

                Signed = TRUE;
                if (Character == L'd') {
                    Base = 10;

                } else if (Character == L'i') {
                    Base = 0;

                } else if (Character == L'o') {
                    Base = 8;

                } else if (Character == L'u') {
                    Base = 10;
                    Signed = FALSE;

                } else {
                    Base = 16;
                }

                ScanStatus = RtlpScanIntegerWide(Input,
                                                 Base,
                                                 FieldWidth,
                                                 Signed,
                                                 &Integer,
                                                 &CharactersConsumed);

                if (!KSUCCESS(ScanStatus)) {
                    Status = ScanStatus;
                    goto ScanWideEnd;
                }

                if (AssignmentSuppression == FALSE) {

                    //
                    // Write the argument.
                    //

                    switch (LengthModifier) {
                    case sizeof(CHAR):
                        *((PCHAR)Argument) = (CHAR)Integer;
                        break;

                    case sizeof(SHORT):
                        *((PSHORT)Argument) = (SHORT)Integer;
                        break;

                    case sizeof(LONG):
                        *((PLONG)Argument) = (LONG)Integer;
                        break;

                    case sizeof(LONGLONG):
                        *((PLONGLONG)Argument) = (LONGLONG)Integer;
                        break;

                    default:

                        ASSERT(FALSE);

                        Status = STATUS_INVALID_SEQUENCE;
                        goto ScanWideEnd;
                    }

                    ArgumentsWritten += 1;
                }

            //
            // Handle floats.
            //

            } else if ((Character == L'a') || (Character == L'A') ||
                       (Character == L'e') || (Character == L'E') ||
                       (Character == L'f') || (Character == L'F') ||
                       (Character == L'g') || (Character == L'G')) {

                ScanStatus = RtlpScanDoubleWide(Input,
                                                FieldWidth,
                                                &Double,
                                                &CharactersConsumed);

                if (!KSUCCESS(ScanStatus)) {
                    Status = ScanStatus;
                    goto ScanWideEnd;
                }

                if (AssignmentSuppression == FALSE) {
                    switch (LengthModifier) {
                    case sizeof(LONG):
                        *((double *)Argument) = Double;
                        break;

                    case sizeof(LONGLONG):
                        *((long double *)Argument) = Double;
                        break;

                    default:
                        *((float *)Argument) = (float)Double;
                        break;
                    }

                    ArgumentsWritten += 1;
                }

            //
            // Handle string copies.
            //

            } else if ((Character == L's') || (Character == L'S')) {
                if (Character == L's') {
                    RtlResetMultibyteState(&(Input->State));
                }

                //
                // First get past any whitespace.
                //

                do {
                    Result = RtlpScannerGetInputWide(Input, &InputCharacter);
                    if ((Result == FALSE) || (InputCharacter == L'\0')) {
                        Status = STATUS_END_OF_FILE;
                        goto ScanWideEnd;
                    }

                } while (RtlIsCharacterSpace(InputCharacter) != FALSE);

                //
                // Now loop putting non-whitespace characters into the
                // argument. Note how the destination argument buffer is
                // unbounded? Very dangerous to use without a field width.
                //

                ByteArgument = (PSTR)Argument;
                do {
                    if (AssignmentSuppression == FALSE) {
                        if ((Character == L'S') || (LongSpecified != FALSE)) {
                            *Argument = InputCharacter;
                            Argument += 1;

                        } else {

                            //
                            // Convert the wide character into bytes as it's
                            // slammed into the argument.
                            //

                            ByteArgumentSize = MULTIBYTE_MAX;
                            Status = RtlConvertWideCharacterToMultibyte(
                                                             InputCharacter,
                                                             ByteArgument,
                                                             &ByteArgumentSize,
                                                             &(Input->State));

                            if (!KSUCCESS(Status)) {
                                goto ScanWideEnd;
                            }

                            ByteArgument += ByteArgumentSize;
                        }
                    }

                    FieldWidth -= 1;
                    Result = RtlpScannerGetInputWide(Input, &InputCharacter);
                    if ((Result == FALSE) || (InputCharacter == L'\0')) {
                        Status = STATUS_END_OF_FILE;
                        break;
                    }

                } while ((FieldWidth != 0) &&
                         (RtlIsCharacterSpaceWide(InputCharacter) == FALSE));

                //
                // Put the last character back.
                //

                if (Result != FALSE) {
                    RtlpScannerUnputWide(Input, InputCharacter);
                }

                //
                // Null terminate the destination string.
                //

                if (AssignmentSuppression == FALSE) {
                    if ((Character == L'S') || (LongSpecified != FALSE)) {
                        *Argument = L'\0';

                    } else {
                        *ByteArgument = '\0';
                    }

                    ArgumentsWritten += 1;
                }

            //
            // Handle a character (or a bunch of them).
            //

            } else if ((Character == L'c') || (Character == L'C')) {
                if (Character == L'c') {
                    RtlResetMultibyteState(&(Input->State));
                }

                if (FieldWidth == -1) {
                    FieldWidth = 1;
                }

                Result = RtlpScannerGetInputWide(Input, &InputCharacter);
                if ((Result == FALSE) || (InputCharacter == L'\0')) {
                    Status = STATUS_END_OF_FILE;
                    goto ScanWideEnd;
                }

                ByteArgument = (PSTR)Argument;
                while (TRUE) {
                    if (AssignmentSuppression == FALSE) {
                        if ((Character == L'C') ||
                            (LongSpecified != FALSE)) {

                            *Argument = InputCharacter;
                            Argument += 1;

                        } else {

                            //
                            // Convert the wide character into bytes as it's
                            // slammed into the argument.
                            //

                            ByteArgumentSize = MULTIBYTE_MAX;
                            Status = RtlConvertWideCharacterToMultibyte(
                                                             InputCharacter,
                                                             ByteArgument,
                                                             &ByteArgumentSize,
                                                             &(Input->State));

                            if (!KSUCCESS(Status)) {
                                goto ScanWideEnd;
                            }

                            ByteArgument += ByteArgumentSize;
                        }
                    }

                    FieldWidth -= 1;
                    if (FieldWidth == 0) {
                        break;
                    }

                    Result = RtlpScannerGetInputWide(Input, &InputCharacter);
                    if ((Result == FALSE) || (InputCharacter == L'\0')) {
                        Status = STATUS_END_OF_FILE;
                        break;
                    }
                }

                if (AssignmentSuppression == FALSE) {
                    ArgumentsWritten += 1;
                }

            //
            // Handle a scanset.
            //

            } else if (Character == L'[') {
                Format += 1;
                FormatLength -= 1;
                if (FormatLength == 0) {
                    Status = STATUS_INVALID_SEQUENCE;
                    goto ScanWideEnd;
                }

                //
                // The circumflex (^) negates the scanset.
                //

                ScanSetNegated = FALSE;
                if (*Format == L'^') {
                    ScanSetNegated = TRUE;
                    Format += 1;
                    FormatLength -= 1;
                    if (FormatLength == 0) {
                        break;
                    }
                }

                //
                // Find the end of the scanset. If the scanset starts with
                // [] or [^] then the left bracket is considered to be part of
                // the scanset. Annoyingly, there is no way to specify a
                // sequence of just ^, which seems like a glaring hole to this
                // programmer.
                //

                ScanSetBegin = Format;
                ScanSetLength = 0;
                while ((FormatLength != 0) && (*Format != L'\0')) {
                    if ((*Format == L']') && (ScanSetLength != 0)) {
                        break;
                    }

                    ScanSetLength += 1;
                    Format += 1;
                    FormatLength -= 1;
                }

                if ((FormatLength == 0) || (*Format == L'\0')) {
                    Status = STATUS_INVALID_SEQUENCE;
                    goto ScanWideEnd;
                }

                if (LongSpecified != FALSE) {
                    RtlResetMultibyteState(&(Input->State));
                }

                //
                // Now grab bytes that are either in the scanset or not in
                // the scanset.
                //

                ByteArgument = (PSTR)Argument;
                ScanSetGotSomething = FALSE;
                do {
                    Result = RtlpScannerGetInputWide(Input, &InputCharacter);
                    if ((Result == FALSE) || (InputCharacter == L'\0')) {
                        break;
                    }

                    InScanSet = FALSE;
                    for (ScanSetIndex = 0;
                         ScanSetIndex < ScanSetLength;
                         ScanSetIndex += 1) {

                        if (InputCharacter == ScanSetBegin[ScanSetIndex]) {
                            InScanSet = TRUE;
                            break;
                        }
                    }

                    //
                    // Break out if it's not negated and it's not in the
                    // scanset, or it is negated and it is in the scanset.
                    // Write it out the long way and the simplification will
                    // be more obvious.
                    //

                    if (ScanSetNegated == InScanSet) {
                        break;
                    }

                    if (AssignmentSuppression == FALSE) {
                        if (LongSpecified != FALSE) {
                            *Argument = InputCharacter;
                            Argument += 1;

                        } else {

                            //
                            // Convert the wide character into bytes as it's
                            // slammed into the argument.
                            //

                            ByteArgumentSize = MULTIBYTE_MAX;
                            Status = RtlConvertWideCharacterToMultibyte(
                                                             InputCharacter,
                                                             ByteArgument,
                                                             &ByteArgumentSize,
                                                             &(Input->State));

                            if (!KSUCCESS(Status)) {
                                goto ScanWideEnd;
                            }

                            ByteArgument += ByteArgumentSize;
                        }
                    }

                    FieldWidth -= 1;
                    ScanSetGotSomething = TRUE;

                } while (FieldWidth != 0);

                if (ScanSetGotSomething == FALSE) {
                    Status = STATUS_INVALID_SEQUENCE;
                    goto ScanWideEnd;
                }

                //
                // Put the last character back.
                //

                if ((Result != FALSE) && (FieldWidth != 0)) {
                    RtlpScannerUnputWide(Input, InputCharacter);
                }

                //
                // Null terminate the destination string.
                //

                if (AssignmentSuppression == FALSE) {
                    if (LongSpecified != FALSE) {
                        *Argument = L'\0';

                    } else {
                        *ByteArgument = '\0';
                    }

                    ArgumentsWritten += 1;
                }

            //
            // Handle a little old percent. Double percents are just the
            // percent sign literal.
            //

            } else if (Character == L'%') {
                Result = RtlpScannerGetInputWide(Input, &InputCharacter);
                if (Result == FALSE) {
                    Status = STATUS_END_OF_FILE;
                    goto ScanWideEnd;
                }

                if (InputCharacter != Character) {
                    Status = STATUS_INVALID_SEQUENCE;
                    goto ScanWideEnd;
                }

            //
            // Return the number of bytes read from the input to get to this
            // point. This doesn't count in the number of arguments written.
            //

            } else if (Character == L'n') {
                if (AssignmentSuppression == FALSE) {
                    *((PINT)Argument) = Input->CharactersRead -
                                        Input->ValidUnputCharacters;
                }

            //
            // This is an unknown format specifier.
            //

            } else {
                Status = STATUS_NOT_SUPPORTED;
                goto ScanWideEnd;
            }
        }

        //
        // Advance to the next character in the format string.
        //

        Format += 1;
        FormatLength -= 1;
    }

    Status = STATUS_SUCCESS;

ScanWideEnd:
    if ((Status == STATUS_INVALID_SEQUENCE) && (Result != FALSE)) {
        RtlpScannerUnputWide(Input, InputCharacter);
    }

    if ((Status == STATUS_END_OF_FILE) && (ArgumentsWritten != 0)) {
        Status = STATUS_SUCCESS;
    }

    *ItemsScanned = ArgumentsWritten;
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
RtlpScanIntegerWide (
    PSCAN_INPUT Input,
    ULONG Base,
    ULONG FieldSize,
    BOOL Signed,
    PLONGLONG Integer,
    PULONG CharactersConsumed
    )

/*++

Routine Description:

    This routine converts a string into an integer. It scans past leading
    whitespace.

Arguments:

    Input - Supplies a pointer to the filled out scan input structure which
        will be used to retrieve more input.

    Base - Supplies the base of the integer to scan. Valid values are zero and
        two through thirty six. If zero is supplied, this routine will attempt
        to automatically detect what the base is out of bases 8, 10, and 16.

    FieldSize - Supplies the maximum number of characters to scan for the
        integer.

    Signed - Supplies a boolean indicating if the integer is signed or not.

    Integer - Supplies a pointer where the resulting integer is returned on
        success.

    CharactersConsumed - Supplies a pointer where the number characters consumed
        will be stored.

Return Value:

    STATUS_SUCCESS if an integer was successfully scanned.

    STATUS_INVALID_SEQUENCE if a valid integer could not be scanned.

    STATUS_INTEGER_OVERFLOW if the result overflowed. In this case the integer
    returned will be MAX_LONGLONG, MIN_LONGLONG, or MAX_ULONGLONG depending on
    the signedness and value.

    STATUS_END_OF_FILE if the end of the file was reached before any
    non-whitespace could be retrieved.

--*/

{

    WCHAR Character;
    ULONG CharacterCount;
    BOOL Negative;
    ULONGLONG NewValue;
    BOOL Result;
    KSTATUS Status;
    BOOL ValidCharacterFound;
    ULONGLONG Value;

    *CharactersConsumed = 0;
    CharacterCount = 0;
    *Integer = 0;
    Negative = FALSE;
    Result = RtlpScannerGetInputWide(Input, &Character);
    if ((Result == FALSE) || (Character == L'\0')) {
        return STATUS_END_OF_FILE;
    }

    //
    // Scan past any whitespace.
    //

    while (RtlIsCharacterSpaceWide(Character) != FALSE) {
        CharacterCount += 1;
        Result = RtlpScannerGetInputWide(Input, &Character);
        if ((Result == FALSE) || (Character == L'\0')) {
            return STATUS_END_OF_FILE;
        }
    }

    //
    // Get past any optional plus or minus.
    //

    if ((Character == L'+') || (Character == L'-')) {
        if (Character == L'-') {
            Negative = TRUE;
        }

        if (FieldSize == 0) {
            Status = STATUS_INVALID_SEQUENCE;
            goto ScanIntegerWideEnd;
        }

        CharacterCount += 1;
        FieldSize -= 1;
        Result = RtlpScannerGetInputWide(Input, &Character);
        if ((Result == FALSE) || (Character == L'\0') || (FieldSize == 0)) {
            Status = STATUS_INVALID_SEQUENCE;
            goto ScanIntegerWideEnd;
        }
    }

    //
    // Get past an optional 0x or 0X for base 16 mode.
    //

    ValidCharacterFound = FALSE;
    if (((Base == 0) || (Base == 16)) && (Character == L'0')) {

        //
        // Seeing a leading zero is an indication of octal mode, so start with
        // that in case the x coming up isn't there.
        //

        if (Base == 0) {
            Base = 8;
        }

        if (FieldSize == 0) {
            Status = STATUS_INVALID_SEQUENCE;
            goto ScanIntegerWideEnd;
        }

        CharacterCount += 1;
        FieldSize -= 1;
        ValidCharacterFound = TRUE;
        Result = RtlpScannerGetInputWide(Input, &Character);
        if ((Result == FALSE) || (Character == L'\0') || (FieldSize == 0)) {
            *CharactersConsumed = CharacterCount;
            Status = STATUS_SUCCESS;
            goto ScanIntegerWideEnd;
        }

        //
        // Swallow an x. 0x by itself is allowed, and counts as just the zero.
        //

        if ((Character == L'x') || (Character == L'X')) {
            Base = 16;
            Result = RtlpScannerGetInputWide(Input, &Character);
            if ((Result == FALSE) ||
                (RtlIsCharacterHexDigitWide(Character) == FALSE) ||
                (FieldSize == 0)) {

                *CharactersConsumed = CharacterCount;
                Status = STATUS_SUCCESS;
                goto ScanIntegerWideEnd;
            }

            CharacterCount += 1;
            FieldSize -= 1;
        }
    }

    //
    // If the base is undecided, take a look at the first digit to figure it
    // out.
    //

    if (Base == 0) {

        ASSERT(Character != L'0');

        if ((Character >= L'1') && (Character <= L'9')) {
            Base = 10;

        } else {
            Status = STATUS_INVALID_SEQUENCE;
            goto ScanIntegerWideEnd;
        }
    }

    Status = STATUS_SUCCESS;
    Value = 0;

    //
    // Loop through every digit.
    //

    while (TRUE) {

        //
        // Potentially add the next digit.
        //

        if ((Character >= L'0') && (Character <= L'9')) {
            if (Character > L'0' + Base - 1) {
                break;
            }

            Character -= L'0';

        //
        // It could also be a letter digit.
        //

        } else if ((Character >= L'A') && (Character <= L'Z')) {
            if (Character > L'A' + Base - 0xA - 1) {
                break;
            }

            Character -= L'A' - 0xA;

        } else if ((Character >= L'a') && (Character <= L'z')) {
            if (Character > L'a' + Base - 0xA - 1) {
                break;
            }

            Character -= L'a' - 0xA;

        //
        // Or it could be something entirely different, in which case the
        // number is over.
        //

        } else {
            break;
        }

        //
        // Check for overflow by dividing back out.
        //

        NewValue = (Value * Base) + Character;
        if (((NewValue - Character) / Base) != Value) {
            Status = STATUS_INTEGER_OVERFLOW;
        }

        Value = NewValue;
        ValidCharacterFound = TRUE;
        CharacterCount += 1;
        FieldSize -= 1;
        if (FieldSize == 0) {
            break;
        }

        Result = RtlpScannerGetInputWide(Input, &Character);
        if ((Result == FALSE) || (Character == L'\0')) {
            break;
        }
    }

    //
    // If the loop broke without ever finding a valid character, fail.
    //

    if (ValidCharacterFound == FALSE) {
        Status = STATUS_INVALID_SEQUENCE;
        goto ScanIntegerWideEnd;
    }

    //
    // If the character that caused the loop to break wasn't an integer, put
    // the candle back.
    //

    if ((FieldSize != 0) && (Result != FALSE)) {
        RtlpScannerUnputWide(Input, Character);
    }

    *CharactersConsumed = CharacterCount;

    //
    // Handle overflow.
    //

    if (Status == STATUS_INTEGER_OVERFLOW) {
        if (Signed != FALSE) {
            if (Negative != FALSE) {
                *Integer = MIN_LONGLONG;

            } else {
                *Integer = MAX_LONGLONG;
            }

        } else {
            *(PULONGLONG)Integer = MAX_ULONGLONG;
        }

    } else {
        if (Negative != FALSE) {
            Value = -Value;
        }

        *Integer = Value;
    }

ScanIntegerWideEnd:
    if ((!KSUCCESS(Status)) && (Result != FALSE)) {
        RtlpScannerUnputWide(Input, Character);
    }

    return Status;
}

//
// This function cannot be inlined because doing so runs the risk of adding
// floating point register prologue/epilogus code in common paths used by the
// kernel.
//

__NOINLINE
KSTATUS
RtlpScanDoubleWide (
    PSCAN_INPUT Input,
    ULONG FieldSize,
    double *Double,
    PULONG CharactersConsumed
    )

/*++

Routine Description:

    This routine converts a string into a floating point double. It scans past
    leading whitespace.

Arguments:

    Input - Supplies a pointer to the filled out scan input structure which
        will be used to retrieve more input.

    FieldSize - Supplies the maximum number of characters to scan for the
        integer.

    Double - Supplies a pointer where the resulting double is returned on
        success.

    CharactersConsumed - Supplies a pointer where the number characters consumed
        will be stored.

Return Value:

    STATUS_SUCCESS if an integer was successfully scanned.

    STATUS_INVALID_SEQUENCE if a valid double could not be scanned.

    STATUS_OUT_OF_BOUNDS if the exponent was out of range.

    STATUS_END_OF_FILE if the end of the file was reached before any
    non-whitespace could be retrieved.

--*/

{

    LONG Base;
    WCHAR Character;
    ULONG CharacterCount;
    WCHAR DecasedCharacter;
    double Digit;
    LONG Exponent;
    WCHAR ExponentCharacter;
    double ExponentMultiplier;
    WCHAR ExponentSign;
    double ExponentValue;
    BOOL Negative;
    double NegativeExponent;
    double OneOverBase;
    ULONG PowerIndex;
    BOOL Result;
    BOOL SeenDecimal;
    KSTATUS Status;
    WCHAR String[DOUBLE_SCAN_STRING_SIZE];
    ULONG StringCharacterCount;
    BOOL ValidCharacterFound;
    double Value;

    Base = 10;
    OneOverBase = 1.0E-1;
    *CharactersConsumed = 0;
    CharacterCount = 0;
    *Double = 0.0;
    Negative = FALSE;
    Value = 0.0;
    Result = RtlpScannerGetInputWide(Input, &Character);
    if ((Result == FALSE) || (Character == L'\0')) {
        return STATUS_END_OF_FILE;
    }

    //
    // Scan past any whitespace.
    //

    while (RtlIsCharacterSpaceWide(Character) != FALSE) {
        CharacterCount += 1;
        Result = RtlpScannerGetInputWide(Input, &Character);
        if ((Result == FALSE) || (Character == L'\0')) {
            return STATUS_END_OF_FILE;
        }
    }

    Status = STATUS_SUCCESS;

    //
    // Get past any optional plus or minus.
    //

    if ((Character == L'+') || (Character == L'-')) {
        if (Character == L'-') {
            Negative = TRUE;
        }

        if (FieldSize == 0) {
            Status = STATUS_INVALID_SEQUENCE;
            goto ScanDoubleWideEnd;
        }

        CharacterCount += 1;
        FieldSize -= 1;
        Result = RtlpScannerGetInputWide(Input, &Character);
        if ((Result == FALSE) || (Character == L'\0') || (FieldSize == 0)) {
            return STATUS_INVALID_SEQUENCE;
        }
    }

    //
    // Look for inf and infinity, ignoring case.
    //

    StringCharacterCount = 0;
    DecasedCharacter = Character;
    if ((Character >= L'A') && (Character <= L'Z')) {
        DecasedCharacter = Character + L'a' - L'A';
    }

    while ((StringCharacterCount < INFINITY_STRING_LENGTH) &&
           (DecasedCharacter == INFINITY_STRING[StringCharacterCount])) {

        String[StringCharacterCount] = Character;
        StringCharacterCount += 1;
        CharacterCount += 1;
        FieldSize -= 1;
        if (FieldSize == 0) {
            break;
        }

        Result = RtlpScannerGetInputWide(Input, &Character);
        if ((Result == FALSE) || (Character == L'\0')) {
            break;
        }

        DecasedCharacter = Character;
        if ((Character >= L'A') && (Character <= L'Z')) {
            DecasedCharacter = Character + L'a' - L'A';
        }
    }

    if (StringCharacterCount >= 3) {

        //
        // If it didn't match the full infinity (but has matched inf), then
        // put back anything between inf and the failed infinity.
        //

        if (StringCharacterCount != INFINITY_STRING_LENGTH) {
            RtlpScannerUnputWide(Input, Character);
            while (StringCharacterCount > 3) {
                RtlpScannerUnputWide(Input, String[StringCharacterCount]);
                StringCharacterCount -= 1;
                CharacterCount -= 1;
            }
        }

        Value = DOUBLE_INFINITY;
        goto ScanDoubleWideEnd;

    //
    // Unput all the characters looked at.
    //

    } else if (StringCharacterCount != 0) {
        RtlpScannerUnputWide(Input, Character);
        StringCharacterCount -= 1;
        CharacterCount -= 1;
        while (StringCharacterCount != 0) {
            RtlpScannerUnputWide(Input, String[StringCharacterCount]);
            StringCharacterCount -= 1;
            CharacterCount -= 1;
        }

        Character = String[0];
    }

    //
    // Also look for NaN.
    //

    DecasedCharacter = Character;
    if ((Character >= L'A') && (Character <= L'Z')) {
        DecasedCharacter = Character + L'a' - L'A';
    }

    while ((StringCharacterCount < NAN_STRING_LENGTH) &&
           (DecasedCharacter == NAN_STRING[StringCharacterCount])) {

        String[StringCharacterCount] = Character;
        StringCharacterCount += 1;
        CharacterCount += 1;
        FieldSize -= 1;
        if (FieldSize == 0) {
            break;
        }

        Result = RtlpScannerGetInputWide(Input, &Character);
        if ((Result == FALSE) || (Character == L'\0')) {
            break;
        }

        DecasedCharacter = Character;
        if ((Character >= L'A') && (Character <= L'Z')) {
            DecasedCharacter = Character + L'a' - L'A';
        }
    }

    if (StringCharacterCount == NAN_STRING_LENGTH) {

        //
        // Also check for a () or (0) on the end.
        //

        if (Character == L'(') {
            Result = RtlpScannerGetInputWide(Input, &Character);
            if (Result != FALSE) {
                if (Character == L'0') {
                    Result = RtlpScannerGetInputWide(Input, &Character);
                    if (Result != FALSE) {
                        if (Character == L')') {
                            CharacterCount += 3;

                        } else {
                            RtlpScannerUnputWide(Input, Character);
                            RtlpScannerUnputWide(Input, L'0');
                            RtlpScannerUnputWide(Input, L'(');
                        }

                    } else {
                        RtlpScannerUnputWide(Input, Character);
                        RtlpScannerUnputWide(Input, L'(');
                    }

                } else if (Character == L')') {
                    CharacterCount += 2;

                } else {
                    RtlpScannerUnputWide(Input, Character);
                    RtlpScannerUnputWide(Input, L'(');
                }

            } else {
                RtlpScannerUnputWide(Input, Character);
            }

        } else {
            RtlpScannerUnputWide(Input, Character);
        }

        Value = DOUBLE_NAN;
        Negative = FALSE;
        goto ScanDoubleWideEnd;

    //
    // Unput all the characters looked at.
    //

    } else if (StringCharacterCount != 0) {
        RtlpScannerUnputWide(Input, Character);
        StringCharacterCount -= 1;
        CharacterCount -= 1;
        while (StringCharacterCount != 0) {
            RtlpScannerUnputWide(Input, String[StringCharacterCount]);
            StringCharacterCount -= 1;
            CharacterCount -= 1;
        }

        Character = String[0];
    }

    //
    // Get past an optional 0x or 0X for base 16 mode.
    //

    ValidCharacterFound = FALSE;
    if (Character == L'0') {
        ValidCharacterFound = TRUE;
        if (FieldSize == 0) {
            Status = STATUS_INVALID_SEQUENCE;
            goto ScanDoubleWideEnd;
        }

        CharacterCount += 1;
        FieldSize -= 1;
        Result = RtlpScannerGetInputWide(Input, &Character);
        if (Result != FALSE) {

            //
            // If it was only a lonely zero, then handle that case specifically.
            //

            if ((FieldSize == 0) ||
                (Character == L'\0') ||
                (RtlIsCharacterSpaceWide(Character) != FALSE)) {

                *CharactersConsumed = CharacterCount;
                goto ScanDoubleWideEnd;
            }

            if ((Character == L'x') || (Character == L'X')) {
                Base = 16;
                OneOverBase = 0.0625;
                Result = RtlpScannerGetInputWide(Input, &Character);

                //
                // If it was just an 0x, then actually it was just a 0.
                //

                if ((Result == FALSE) ||
                    (RtlIsCharacterHexDigitWide(Character) == FALSE)) {

                    RtlpScannerUnputWide(Input, Character);
                    goto ScanDoubleWideEnd;
                }

                CharacterCount += 1;
                FieldSize -= 1;
                if (FieldSize == 0) {
                    Status = STATUS_INVALID_SEQUENCE;
                    goto ScanDoubleWideEnd;
                }
            }
        }
    }

    Digit = 0.0;
    NegativeExponent = OneOverBase;

    //
    // Loop through every digit.
    //

    SeenDecimal = FALSE;
    while (TRUE) {

        //
        // Uppercase any letters.
        //

        if ((Character >= L'a') && (Character <= L'z')) {
            Character = L'A' + Character - L'a';
        }

        //
        // Potentially add the next digit.
        //

        if ((Character >= L'0') && (Character <= L'9')) {
            Digit = Character - L'0';

        //
        // It could also be a letter digit.
        //

        } else if ((Base == 16) &&
                   (Character >= L'A') && (Character <= L'F')) {

            Digit = Character - L'A' + 10;

        //
        // Handle a decimal point. Hopefully it was the the first and only one.
        //

        } else if (Character == L'.') {
            if (SeenDecimal != FALSE) {
                break;
            }

            SeenDecimal = TRUE;

        //
        // Or it could be something entirely different, in which case the
        // number is over.
        //

        } else {
            break;
        }

        if (Character != L'.') {

            //
            // If a decimal point has not been seen yet, this is the next
            // integer digit, so multiply everything by the base and add
            // this digit.
            //

            if (SeenDecimal == FALSE) {
                Value = (Value * (double)Base) + Digit;

            //
            // This is a fractional part, so multiply it by the current
            // negative exponent, add it to the value, and shrink down to the
            // next exponent.
            //

            } else {
                Value += Digit * NegativeExponent;
                NegativeExponent *= OneOverBase;
            }

            ValidCharacterFound = TRUE;
        }

        CharacterCount += 1;
        FieldSize -= 1;
        if (FieldSize == 0) {
            goto ScanDoubleWideEnd;
        }

        Result = RtlpScannerGetInputWide(Input, &Character);
        if ((Result == FALSE) || (Character == L'\0')) {
            break;
        }
    }

    //
    // If the loop broke without ever finding a valid character, fail.
    //

    if (ValidCharacterFound == FALSE) {
        CharacterCount = 0;
        Status = STATUS_INVALID_SEQUENCE;
        goto ScanDoubleWideEnd;
    }

    if (FieldSize == 0) {
        goto ScanDoubleWideEnd;
    }

    //
    // Look for an exponent character, and if none is found, finish.
    //

    ExponentCharacter = 0;
    if (((Base == 10) && ((Character == L'e') || (Character == L'E'))) ||
        ((Base == 16) && ((Character == L'p') || (Character == L'P')))) {

        ExponentCharacter = Character;
    }

    if (ExponentCharacter == 0) {
        RtlpScannerUnputWide(Input, Character);
        goto ScanDoubleWideEnd;
    }

    CharacterCount += 1;
    FieldSize -= 1;
    if (FieldSize == 0) {
        goto ScanDoubleWideEnd;
    }

    Result = RtlpScannerGetInputWide(Input, &Character);
    if ((Result == FALSE) || (Character == L'\0')) {
        RtlpScannerUnputWide(Input, ExponentCharacter);
        CharacterCount -= 1;
        goto ScanDoubleWideEnd;
    }

    //
    // Look for an optional plus or minus.
    //

    ExponentSign = 0;
    if ((Character == L'+') || (Character == L'-')) {
        ExponentSign = Character;
        CharacterCount += 1;
        FieldSize -= 1;
        if (FieldSize == 0) {
            goto ScanDoubleWideEnd;
        }

        Result = RtlpScannerGetInputWide(Input, &Character);
        if ((Result == FALSE) || (Character == L'\0')) {
            RtlpScannerUnputWide(Input, ExponentSign);
            RtlpScannerUnputWide(Input, ExponentCharacter);
            CharacterCount -= 2;
            goto ScanDoubleWideEnd;
        }
    }

    //
    // If there are not exponent digits, the exponent and sign were a fakeout.
    //

    if (!((Character >= L'0') && (Character <= L'9'))) {
        RtlpScannerUnputWide(Input, Character);
        if (ExponentSign != 0) {
            RtlpScannerUnputWide(Input, ExponentSign);
            CharacterCount -= 1;
        }

        RtlpScannerUnputWide(Input, ExponentCharacter);
        CharacterCount -= 1;
        goto ScanDoubleWideEnd;
    }

    //
    // Scan the base decimal integer exponent (meaning the exponent is always
    // a string in base 10).
    //

    Exponent = 0;
    while ((Character >= L'0') && (Character <= L'9')) {
        Exponent = (Exponent * 10) + (Character - L'0');
        CharacterCount += 1;
        FieldSize -= 1;
        if (FieldSize == 0) {
            break;
        }

        Result = RtlpScannerGetInputWide(Input, &Character);
        if ((Result == FALSE) || (Character == L'\0')) {
            break;
        }
    }

    //
    // If the character that caused the loop to break wasn't an integer, put.
    // the candle. back.
    //

    if ((FieldSize != 0) && (Result != FALSE)) {
        RtlpScannerUnputWide(Input, Character);
    }

    if (Exponent > 300) {
        if (Value == 0.0) {
            goto ScanDoubleWideEnd;
        }

        Status = STATUS_OUT_OF_BOUNDS;
        Result = FALSE;
        if (ExponentSign == L'-') {
            Value = 0.0;

        } else {
            Value = DOUBLE_HUGE_VALUE;
        }

        goto ScanDoubleWideEnd;
    }

    //
    // Create a value with the desired exponent.
    //

    if (Base == 10) {

        //
        // Put together the approximation using powers of 2.
        //

        if (ExponentSign == L'-') {
            ExponentValue = RtlFirst16NegativePowersOf10[Exponent & 0x0F];

        } else {
            ExponentValue = RtlFirst16PowersOf10[Exponent & 0x0F];
        }

        Exponent = Exponent >> 4;
        for (PowerIndex = 0; PowerIndex < 5; PowerIndex += 1) {
            if (Exponent == 0) {
                break;
            }

            if ((Exponent & 0x1) != 0) {
                if (ExponentSign == L'-') {
                    ExponentValue *= RtlNegativePowersOf2[PowerIndex];

                } else {
                    ExponentValue *= RtlPositivePowersOf2[PowerIndex];
                }
            }

            Exponent = Exponent >> 1;
        }

    //
    // For base 16, just multiply the power of 2 out.
    //

    } else {
        ExponentValue = 1.0;
        if (ExponentSign == L'-') {
            ExponentMultiplier = 0.5;

        } else {
            ExponentMultiplier = 2.0;
        }

        while (Exponent != 0) {
            ExponentValue *= ExponentMultiplier;
            Exponent -= 1;
        }
    }

    Value *= ExponentValue;

ScanDoubleWideEnd:
    if ((!KSUCCESS(Status)) && (Result != FALSE)) {
        RtlpScannerUnputWide(Input, Character);
    }

    *CharactersConsumed = CharacterCount;
    if (Negative != FALSE) {
        Value = -Value;
    }

    *Double = Value;
    return Status;
}

VOID
RtlpScannerUnputWide (
    PSCAN_INPUT Input,
    WCHAR Character
    )

/*++

Routine Description:

    This routine puts a byte of input back into the scanner's input stream.

Arguments:

    Input - Supplies a pointer to the input scanner structure.

    Character - Supplies the character to put back.

Return Value:

    None.

--*/

{

    ASSERT(Input->ValidUnputCharacters < SCANNER_UNPUT_SIZE);

    Input->UnputCharacters[Input->ValidUnputCharacters] = Character;
    Input->ValidUnputCharacters += 1;
    return;
}

BOOL
RtlpScannerGetInputWide (
    PSCAN_INPUT Input,
    PWCHAR Character
    )

/*++

Routine Description:

    This routine retrieves another character of input from the input scanner.

Arguments:

    Input - Supplies a pointer to the input scanner structure.

    Character - Supplies a pointer where the character will be returned on
        success.

Return Value:

    TRUE if a character was read.

    FALSE if the end of the file or string was encountered.

--*/

{

    if (Input->ValidUnputCharacters != 0) {
        *Character = Input->UnputCharacters[Input->ValidUnputCharacters - 1];
        Input->ValidUnputCharacters -= 1;
        return TRUE;
    }

    return Input->ReadU.GetInputWide(Input, Character);
}

BOOL
RtlpStringScannerGetInputWide (
    PSCAN_INPUT Input,
    PWCHAR Character
    )

/*++

Routine Description:

    This routine retrieves another character of input from the input scanner
    for a string based scanner.

Arguments:

    Input - Supplies a pointer to the input scanner structure.

    Character - Supplies a pointer where the character will be returned on
        success.

Return Value:

    TRUE if a character was read.

    FALSE if the end of the file or string was encountered.

--*/

{

    if (Input->StringSize == 0) {
        return FALSE;
    }

    Input->CharactersRead += 1;
    Input->StringSize -= 1;
    *Character = *(Input->DataU.WideString);
    Input->DataU.WideString += 1;
    return TRUE;
}

