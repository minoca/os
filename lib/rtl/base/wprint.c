/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    wprint.c

Abstract:

    This module implements wide character print format support.

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

#define CONVERSION_CHARACTER L'%'
#define POSITIONAL_ARGUMENT L'$'
#define FIELD_IN_ARGUMENT L'*'
#define THOUSANDS_GROUPING L'\''
#define LEFT_JUSTIFIED L'-'
#define SPACE_FOR_PLUS L' '
#define PRINT_SIGN L'+'
#define PRINT_RADIX_IDENTIFIER L'#'
#define PRINT_LEADING_ZEROES L'0'
#define PRECISION_SPECIFIED L'.'
#define FORMAT_SHORT L'h'
#define FORMAT_LONG L'l'
#define FORMAT_INTMAX L'j'
#define FORMAT_SIZE_T L'z'
#define FORMAT_PTRDIFF_T L't'
#define FORMAT_LONG_DOUBLE L'L'

#define FORMAT_DOUBLE_HEX L'a'
#define FORMAT_DOUBLE_HEX_CAPITAL L'A'
#define FORMAT_FLOAT L'f'
#define FORMAT_FLOAT_CAPITAL L'F'
#define FORMAT_SCIENTIFIC L'e'
#define FORMAT_SCIENTIFIC_CAPITAL L'E'
#define FORMAT_DOUBLE L'g'
#define FORMAT_DOUBLE_CAPITAL L'G'
#define FORMAT_CHARACTER L'c'
#define FORMAT_LONG_CHARACTER L'C'
#define FORMAT_STRING L's'
#define FORMAT_LONG_STRING L'S'
#define FORMAT_BYTES_PRINTED L'n'
#define FORMAT_POINTER L'p'
#define FORMAT_NONE L'%'
#define FORMAT_DECIMAL L'd'
#define FORMAT_DECIMAL2 L'i'
#define FORMAT_OCTAL L'o'
#define FORMAT_UNSIGNED L'u'
#define FORMAT_HEX L'x'
#define FORMAT_HEX_CAPITAL L'X'
#define FORMAT_LONGLONG_START L'I'

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

BOOL
RtlpConvertFormatSpecifierWide (
    PPRINT_FORMAT_CONTEXT Context,
    PCWSTR Format,
    PULONG Index,
    va_list *Arguments
    );

ULONG
RtlpPrintIntegerWide (
    PPRINT_FORMAT_CONTEXT Context,
    ULONGLONG Integer,
    PPRINT_FORMAT_PROPERTIES Properties
    );

ULONG
RtlpPrintDoubleWide (
    PPRINT_FORMAT_CONTEXT Context,
    double Value,
    PPRINT_FORMAT_PROPERTIES Properties
    );

ULONG
RtlpPrintHexDoubleWide (
    PPRINT_FORMAT_CONTEXT Context,
    double Value,
    PPRINT_FORMAT_PROPERTIES Properties
    );

BOOL
RtlpPrintStringWide (
    PPRINT_FORMAT_CONTEXT Context,
    PWSTR String,
    LONG FieldWidth,
    LONG Precision,
    BOOL LeftJustified,
    BOOL Character
    );

BOOL
RtlpPrintByteStringWide (
    PPRINT_FORMAT_CONTEXT Context,
    PSTR String,
    LONG FieldWidth,
    LONG Precision,
    BOOL LeftJustified,
    BOOL Character
    );

BOOL
RtlpFormatWriteCharacterWide (
    PPRINT_FORMAT_CONTEXT Context,
    WCHAR Character
    );

ULONGLONG
RtlpGetPositionalArgumentWide (
    PCWSTR Format,
    ULONG ArgumentNumber,
    va_list *Arguments
    );

ULONG
RtlpGetPositionalArgumentSizeWide (
    PCWSTR Format,
    ULONG ArgumentNumber
    );

BOOL
RtlpStringFormatWriteCharacterWide (
    WCHAR Character,
    PPRINT_FORMAT_CONTEXT Context
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

RTL_API
ULONG
RtlPrintToStringWide (
    PWSTR Destination,
    ULONG DestinationSize,
    CHARACTER_ENCODING Encoding,
    PCWSTR Format,
    ...
    )

/*++

Routine Description:

    This routine prints a formatted wide string out to a buffer.

Arguments:

    Destination - Supplies a pointer to the buffer where the formatted string
        will be placed.

    DestinationSize - Supplies the size of the destination buffer, in bytes.

    Encoding - Supplies the character encoding to use for any non-wide
        characters or strings.

    Format - Supplies the printf-style format string to print. The contents of
        this string determine the rest of the arguments passed.

    ... - Supplies any arguments needed to convert the Format string.

Return Value:

    Returns the length of the final string (in characters) after all formatting
    has been completed. The length will be returned even if NULL is passed as
    the destination.

--*/

{

    va_list ArgumentList;
    ULONG Result;

    va_start(ArgumentList, Format);
    Result = RtlFormatStringWide(Destination,
                                 DestinationSize,
                                 Encoding,
                                 Format,
                                 ArgumentList);

    va_end(ArgumentList);
    return Result;
}

RTL_API
ULONG
RtlFormatStringWide (
    PWSTR Destination,
    ULONG DestinationSize,
    CHARACTER_ENCODING Encoding,
    PCWSTR Format,
    va_list ArgumentList
    )

/*++

Routine Description:

    This routine converts a printf-style wide format string given the
    parameters.

Arguments:

    Destination - Supplies a pointer to the buffer where the final string will
        be printed. It is assumed that this string is allocated and is big
        enough to hold the converted string. Pass NULL here to determine how big
        a buffer is necessary to hold the string. If the buffer is not big
        enough, it will be truncated but still NULL terminated.

    DestinationSize - Supplies the size of the destination buffer. If NULL was
        passed as the Destination argument, then this argument is ignored.

    Encoding - Supplies the character encoding to use for any non-wide
        characters or strings.

    Format - Supplies a pointer to the printf-style format string.

    ArgumentList - Supplies an initialized list of arguments to the format
        string.

Return Value:

    Returns the length of the final string after all formatting has been
    completed, including the null terminator. The length will be returned even
    if NULL is passed as the destination.

--*/

{

    UINTN CharactersWritten;
    PRINT_FORMAT_CONTEXT Context;

    RtlZeroMemory(&Context, sizeof(PRINT_FORMAT_CONTEXT));
    Context.U.WriteWideCharacter = RtlpStringFormatWriteCharacterWide;
    Context.Context = Destination;
    if (DestinationSize != 0) {
        Context.Limit = DestinationSize - 1;
    }

    RtlInitializeMultibyteState(&(Context.State), Encoding);
    RtlFormatWide(&Context, Format, ArgumentList);
    CharactersWritten = Context.CharactersWritten;
    if (DestinationSize != 0) {
        if (Context.CharactersWritten > Context.Limit) {
            Context.CharactersWritten = Context.Limit;
        }

        Context.Limit = DestinationSize;
    }

    RtlpFormatWriteCharacterWide(&Context, WIDE_STRING_TERMINATOR);
    return CharactersWritten + 1;
}

RTL_API
BOOL
RtlFormatWide (
    PPRINT_FORMAT_CONTEXT Context,
    PCWSTR Format,
    va_list ArgumentList
    )

/*++

Routine Description:

    This routine converts a printf-style wide format string given the
    parameters.

Arguments:

    Context - Supplies a pointer to the initialized context structure.

    Format - Supplies a pointer to the printf-style format string.

    ArgumentList - Supplies an initialized list of arguments to the format
        string.

Return Value:

    TRUE if all characters were written to the destination.

    FALSE if the destination or limit cut the conversion short.

--*/

{

    va_list ArgumentListCopy;
    ULONG Index;
    BOOL Result;

    ASSERT((Context != NULL) && (Context->U.WriteWideCharacter != NULL) &&
           (Context->CharactersWritten == 0) &&
           (RtlIsCharacterEncodingSupported(Context->State.Encoding) != FALSE));

    if (Format == NULL) {
        Format = L"(null)";
    }

    //
    // Copy each character to the destination, handling formats along the way.
    //

    va_copy(ArgumentListCopy, ArgumentList);
    Result = TRUE;
    Index = 0;
    while (Format[Index] != WIDE_STRING_TERMINATOR) {
        if (Format[Index] == CONVERSION_CHARACTER) {
            Result = RtlpConvertFormatSpecifierWide(Context,
                                                    Format,
                                                    &Index,
                                                    &ArgumentListCopy);

            if (Result == FALSE) {
                goto FormatWideEnd;
            }

        } else {
            Result = RtlpFormatWriteCharacterWide(Context, Format[Index]);
            if (Result == FALSE) {
                goto FormatWideEnd;
            }

            Index += 1;
        }
    }

FormatWideEnd:
    va_end(ArgumentListCopy);
    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

BOOL
RtlpConvertFormatSpecifierWide (
    PPRINT_FORMAT_CONTEXT Context,
    PCWSTR Format,
    PULONG Index,
    va_list *Arguments
    )

/*++

Routine Description:

    This routine converts one printf-style wide format specifier to its wide
    string conversion.

Arguments:

    Context - Supplies a pointer to the initialized context structure.

    Format - Supplies a pointer to the printf-style conversion specifier.

    Index - Supplies a pointer that upon input contains the index of the
        L'%' specifier to convert. On output, this will be advanced beyond the
        specifier.

    Arguments - Supplies a pointer to the variable list of arguments.

Return Value:

    TRUE if all characters were written to the destination.

    FALSE if the string was truncated.

--*/

{

    CHAR ByteCharacterArgument;
    PSTR ByteStringArgument;
    WCHAR CharacterArgument;
    PCWSTR CurrentFormat;
    DOUBLE_PARTS DoubleParts;
    LONGLONG Integer;
    ULONGLONG IntegerArgument;
    BOOL IsFloat;
    BOOL IsInteger;
    BOOL LongDoubleSpecified;
    BOOL LongSpecified;
    ULONG Position;
    PRINT_FORMAT_PROPERTIES Properties;
    ULONG RemainingSize;
    BOOL Result;
    WCHAR Specifier;
    KSTATUS Status;
    PWSTR StringArgument;

    CurrentFormat = Format + *Index;
    RtlZeroMemory(&Properties, sizeof(PRINT_FORMAT_PROPERTIES));
    IntegerArgument = 0;
    Properties.Precision = -1;

    //
    // Check for the format character.
    //

    if (*CurrentFormat != CONVERSION_CHARACTER) {
        Result = FALSE;
        goto ConvertFormatSpecifierWideEnd;
    }

    CurrentFormat += 1;
    Position = 0;

    //
    // If there's a non-zero digit, grab it. It could be the position or field
    // width.
    //

    if ((*CurrentFormat >= L'1') && (*CurrentFormat <= L'9')) {
        RemainingSize = -1;
        Status = RtlStringScanIntegerWide(&CurrentFormat,
                                          &RemainingSize,
                                          10,
                                          FALSE,
                                          &Integer);

        if (!KSUCCESS(Status)) {
            Integer = 0;
        }

        if (*CurrentFormat == POSITIONAL_ARGUMENT) {
            if (Integer < 0) {
                Result = FALSE;
                goto ConvertFormatSpecifierWideEnd;
            }

            Position = (ULONG)Integer;
            CurrentFormat += 1;

        } else {
            Properties.FieldWidth = (ULONG)Integer;
        }
    }

    //
    // Process any flags.
    //

    while (TRUE) {
        if (*CurrentFormat == THOUSANDS_GROUPING) {
            Properties.ThousandsGrouping = TRUE;

        } else if (*CurrentFormat == LEFT_JUSTIFIED) {
            Properties.LeftJustified = TRUE;

        } else if (*CurrentFormat == SPACE_FOR_PLUS) {
            Properties.SpaceForPlus = TRUE;

        } else if (*CurrentFormat == PRINT_SIGN) {
            Properties.AlwaysPrintSign = TRUE;

        } else if (*CurrentFormat == PRINT_RADIX_IDENTIFIER) {
            Properties.PrintRadix = TRUE;

        } else if (*CurrentFormat == PRINT_LEADING_ZEROES) {
            Properties.PrintLeadingZeroes = TRUE;

        } else {
            break;
        }

        CurrentFormat += 1;
    }

    //
    // If both print leading zeroes and left justify are specified, print
    // leading zeroes is ignored.
    //

    if (Properties.LeftJustified != FALSE) {
        Properties.PrintLeadingZeroes = FALSE;
    }

    if (Properties.AlwaysPrintSign != FALSE) {
        Properties.SpaceForPlus = FALSE;
    }

    //
    // Process a field width. It could have already been sucked in, be a
    // decimal, be a star, or be a star followed by a position and a dollar
    // sign.
    //

    if (*CurrentFormat == FIELD_IN_ARGUMENT) {
        CurrentFormat += 1;
        if ((*CurrentFormat >= L'1') && (*CurrentFormat <= L'9')) {
            RemainingSize = -1;
            Status = RtlStringScanIntegerWide(&CurrentFormat,
                                              &RemainingSize,
                                              10,
                                              FALSE,
                                              &Integer);

            if ((!KSUCCESS(Status)) || (Integer < 0)) {
                Result = FALSE;
                goto ConvertFormatSpecifierWideEnd;
            }

            if (*CurrentFormat != POSITIONAL_ARGUMENT) {
                Result = FALSE;
                goto ConvertFormatSpecifierWideEnd;
            }

            CurrentFormat += 1;
            Properties.FieldWidth = (INT)RtlpGetPositionalArgumentWide(
                                                                Format,
                                                                (ULONG)Integer,
                                                                Arguments);

        } else {
            Properties.FieldWidth = va_arg(*Arguments, INT);
        }

    } else if ((*CurrentFormat >= L'1') && (*CurrentFormat <= L'9')) {
        RemainingSize = -1;
        Status = RtlStringScanIntegerWide(&CurrentFormat,
                                          &RemainingSize,
                                          10,
                                          FALSE,
                                          &Integer);

        if (!KSUCCESS(Status)) {
            Result = FALSE;
            goto ConvertFormatSpecifierWideEnd;
        }

        Properties.FieldWidth = (ULONG)Integer;
    }

    if (Properties.FieldWidth < 0) {
        Properties.LeftJustified = TRUE;
        Properties.FieldWidth = -Properties.FieldWidth;
    }

    //
    // If there's a dot, then the precision follows. Like the field width, it
    // could either be a decimal, a star, or a star plus a position and a
    // dollar sign.
    //

    if (*CurrentFormat == PRECISION_SPECIFIED) {
        CurrentFormat += 1;
        if (*CurrentFormat == FIELD_IN_ARGUMENT) {
            CurrentFormat += 1;
            if ((*CurrentFormat >= L'0') && (*CurrentFormat <= L'9')) {
                RemainingSize = -1;
                Status = RtlStringScanIntegerWide(&CurrentFormat,
                                                  &RemainingSize,
                                                  10,
                                                  FALSE,
                                                  &Integer);

                if ((!KSUCCESS(Status)) || (Integer < 0)) {
                    Result = FALSE;
                    goto ConvertFormatSpecifierWideEnd;
                }

                if (*CurrentFormat != POSITIONAL_ARGUMENT) {
                    Result = FALSE;
                    goto ConvertFormatSpecifierWideEnd;
                }

                CurrentFormat += 1;
                Properties.Precision = (INT)RtlpGetPositionalArgumentWide(
                                                                Format,
                                                                (ULONG)Integer,
                                                                Arguments);

            } else {
                Properties.Precision = va_arg(*Arguments, INT);
            }

        } else if ((*CurrentFormat >= L'0') && (*CurrentFormat <= L'9')) {
            RemainingSize = -1;
            Status = RtlStringScanIntegerWide(&CurrentFormat,
                                              &RemainingSize,
                                              10,
                                              FALSE,
                                              &Integer);

            if (!KSUCCESS(Status)) {
                Result = FALSE;
                goto ConvertFormatSpecifierWideEnd;
            }

            if (Integer >= 0) {
                Properties.Precision = (ULONG)Integer;
            }

        } else {
            Properties.Precision = 0;
        }
    }

    //
    // A negative precision is taken as precision being omitted.
    //

    if (Properties.Precision < 0) {
        Properties.Precision = -1;
    }

    //
    // Look for the length modifiers: hh, h, l, ll, j, z, t, L, I64.
    //

    LongSpecified = FALSE;
    LongDoubleSpecified = FALSE;
    Properties.IntegerSize = sizeof(INT);
    if (*CurrentFormat == FORMAT_SHORT) {
        CurrentFormat += 1;
        Properties.IntegerSize = sizeof(SHORT);
        if (*CurrentFormat == FORMAT_SHORT) {
            CurrentFormat += 1;
            Properties.IntegerSize = sizeof(CHAR);
        }

    } else if (*CurrentFormat == FORMAT_LONG) {
        LongSpecified = TRUE;
        CurrentFormat += 1;
        Properties.IntegerSize = sizeof(LONG);
        if (*CurrentFormat == FORMAT_LONG) {
            LongSpecified = FALSE;
            CurrentFormat += 1;
            Properties.IntegerSize = sizeof(LONGLONG);
        }

    } else if (*CurrentFormat == FORMAT_INTMAX) {
        CurrentFormat += 1;
        Properties.IntegerSize = sizeof(intmax_t);

    } else if (*CurrentFormat == FORMAT_SIZE_T) {
        CurrentFormat += 1;
        Properties.IntegerSize = sizeof(size_t);

    } else if (*CurrentFormat == FORMAT_LONG_DOUBLE) {

        //
        // Printing of long doubles is not currently supported.
        //

        ASSERT(FALSE);

        LongDoubleSpecified = TRUE;
        CurrentFormat += 1;
        Properties.IntegerSize = sizeof(long double);

    } else if ((*CurrentFormat == FORMAT_LONGLONG_START) &&
               (*(CurrentFormat + 1) == '6') &&
               (*(CurrentFormat + 2) == '4')) {

        CurrentFormat += 3;
        Properties.IntegerSize = sizeof(LONGLONG);
    }

    //
    // Now, finally, get the conversion specifier
    //

    Specifier = *CurrentFormat;
    if (LongSpecified != FALSE) {
        if (Specifier == FORMAT_CHARACTER) {
            Specifier = FORMAT_LONG_CHARACTER;

        } else if (Specifier == FORMAT_STRING) {
            Specifier = FORMAT_LONG_STRING;
        }
    }

    IsInteger = FALSE;
    IsFloat = FALSE;
    Properties.Unsigned = TRUE;
    switch (Specifier) {
    case FORMAT_DECIMAL:
    case FORMAT_DECIMAL2:
        IsInteger = TRUE;
        Properties.Radix = 10;
        Properties.Unsigned = FALSE;
        break;

    case FORMAT_OCTAL:
        IsInteger = TRUE;
        Properties.Radix = 8;
        break;

    case FORMAT_UNSIGNED:
        IsInteger = TRUE;
        Properties.Radix = 10;
        break;

    case FORMAT_HEX:
        IsInteger = TRUE;
        Properties.Radix = 16;
        break;

    case FORMAT_POINTER:
        IsInteger = TRUE;
        Properties.IntegerSize = sizeof(UINTN);
        Properties.Radix = 16;
        Properties.PrintUpperCase = TRUE;
        Properties.PrintRadix = TRUE;
        break;

    case FORMAT_HEX_CAPITAL:
        IsInteger = TRUE;
        Properties.Radix = 16;
        Properties.PrintUpperCase = TRUE;
        break;

    case FORMAT_BYTES_PRINTED:
        IsInteger = TRUE;
        Properties.IntegerSize = sizeof(PVOID);
        break;

    case FORMAT_FLOAT:
        IsFloat = TRUE;
        Properties.FloatFormat = TRUE;
        break;

    case FORMAT_FLOAT_CAPITAL:
        IsFloat = TRUE;
        Properties.FloatFormat = TRUE;
        Properties.PrintUpperCase = TRUE;
        break;

    case FORMAT_DOUBLE:
        IsFloat = TRUE;
        Properties.SignificantDigitPrecision = TRUE;
        break;

    case FORMAT_DOUBLE_CAPITAL:
        IsFloat = TRUE;
        Properties.PrintUpperCase = TRUE;
        Properties.SignificantDigitPrecision = TRUE;
        break;

    case FORMAT_SCIENTIFIC:
        IsFloat = TRUE;
        Properties.ScientificFormat = TRUE;
        break;

    case FORMAT_SCIENTIFIC_CAPITAL:
        IsFloat = TRUE;
        Properties.ScientificFormat = TRUE;
        Properties.PrintUpperCase = TRUE;
        break;

    case FORMAT_DOUBLE_HEX:
        IsFloat = TRUE;
        Properties.ScientificFormat = TRUE;
        Properties.Radix = 16;
        break;

    case FORMAT_DOUBLE_HEX_CAPITAL:
        IsFloat = TRUE;
        Properties.ScientificFormat = TRUE;
        Properties.PrintUpperCase = TRUE;
        Properties.Radix = 16;
        break;

    case FORMAT_LONG_CHARACTER:
        if (Position != 0) {
            CharacterArgument = (WCHAR)RtlpGetPositionalArgumentWide(Format,
                                                                     Position,
                                                                     Arguments);

        } else {
            CharacterArgument = va_arg(*Arguments, int);
        }

        Result = RtlpPrintStringWide(Context,
                                     &CharacterArgument,
                                     Properties.FieldWidth,
                                     Properties.Precision,
                                     Properties.LeftJustified,
                                     TRUE);

        if (Result == FALSE) {
            goto ConvertFormatSpecifierWideEnd;
        }

        break;

    case FORMAT_CHARACTER:
        RtlResetMultibyteState(&(Context->State));
        if (Position != 0) {
            ByteCharacterArgument = (UCHAR)RtlpGetPositionalArgumentWide(
                                                                    Format,
                                                                    Position,
                                                                    Arguments);

        } else {
            ByteCharacterArgument = (CHAR)va_arg(*Arguments, INT);
        }

        Result = RtlpPrintByteStringWide(Context,
                                         &ByteCharacterArgument,
                                         Properties.FieldWidth,
                                         Properties.Precision,
                                         Properties.LeftJustified,
                                         TRUE);

        if (Result == FALSE) {
            goto ConvertFormatSpecifierWideEnd;
        }

        break;

    case FORMAT_LONG_STRING:
        if (Position != 0) {
            IntegerArgument = RtlpGetPositionalArgumentWide(Format,
                                                            Position,
                                                            Arguments);

            StringArgument = (PWSTR)(UINTN)IntegerArgument;

        } else {
            StringArgument = va_arg(*Arguments, PWSTR);
        }

        Result = RtlpPrintStringWide(Context,
                                     StringArgument,
                                     Properties.FieldWidth,
                                     Properties.Precision,
                                     Properties.LeftJustified,
                                     FALSE);

        if (Result == FALSE) {
            goto ConvertFormatSpecifierWideEnd;
        }

        break;

    case FORMAT_STRING:
        RtlResetMultibyteState(&(Context->State));
        if (Position != 0) {
            IntegerArgument = RtlpGetPositionalArgumentWide(Format,
                                                            Position,
                                                            Arguments);

            ByteStringArgument = (PSTR)(UINTN)IntegerArgument;

        } else {
            ByteStringArgument = va_arg(*Arguments, PSTR);
        }

        Result = RtlpPrintByteStringWide(Context,
                                         ByteStringArgument,
                                         Properties.FieldWidth,
                                         Properties.Precision,
                                         Properties.LeftJustified,
                                         FALSE);

        if (Result == FALSE) {
            goto ConvertFormatSpecifierWideEnd;
        }

        break;

    case FORMAT_NONE:
        IsInteger = FALSE;
        CharacterArgument = FORMAT_NONE;
        Result = RtlpPrintStringWide(Context,
                                     &CharacterArgument,
                                     Properties.FieldWidth,
                                     Properties.Precision,
                                     Properties.LeftJustified,
                                     TRUE);

        if (Result == FALSE) {
            goto ConvertFormatSpecifierWideEnd;
        }

        break;

    default:
        Result = FALSE;
        goto ConvertFormatSpecifierWideEnd;
    }

    CurrentFormat += 1;

    //
    // If it's an integer, get the argument and process it.
    //

    if (IsInteger != FALSE) {
        if (Position != 0) {
            IntegerArgument = RtlpGetPositionalArgumentWide(Format,
                                                            Position,
                                                            Arguments);

            switch (Properties.IntegerSize) {
            case 0:
                break;

            case sizeof(CHAR):
                IntegerArgument = (UCHAR)IntegerArgument;
                break;

            case sizeof(SHORT):
                IntegerArgument = (USHORT)IntegerArgument;
                break;

            case sizeof(LONG):
                IntegerArgument = (ULONG)IntegerArgument;
                break;

            case sizeof(LONGLONG):
                break;

            default:

                ASSERT(FALSE);

                Result = FALSE;
                goto ConvertFormatSpecifierWideEnd;
            }

        } else {
            switch (Properties.IntegerSize) {
            case 0:
                break;

            case sizeof(CHAR):
                IntegerArgument = (CHAR)va_arg(*Arguments, UINT);
                break;

            case sizeof(SHORT):
                IntegerArgument = (SHORT)va_arg(*Arguments, UINT);
                break;

            case sizeof(LONG):
                IntegerArgument = va_arg(*Arguments, ULONG);
                break;

            case sizeof(LONGLONG):
                IntegerArgument = va_arg(*Arguments, ULONGLONG);
                break;

            default:

                ASSERT(FALSE);

                Result = FALSE;
                goto ConvertFormatSpecifierWideEnd;
            }
        }

        if (Specifier == FORMAT_BYTES_PRINTED) {

            ASSERT(IntegerArgument != (UINTN)NULL);

            *((PINT)(UINTN)IntegerArgument) = Context->CharactersWritten;
            Result = TRUE;

        } else {
            Result = RtlpPrintIntegerWide(Context,
                                          IntegerArgument,
                                          &Properties);

            if (Result == FALSE) {
                goto ConvertFormatSpecifierWideEnd;
            }
        }

    //
    // If it's an float, get the argument and process it.
    //

    } else if (IsFloat != FALSE) {
        if (Position != 0) {

            //
            // TODO: Support long double.
            //

            DoubleParts.Ulonglong = RtlpGetPositionalArgumentWide(Format,
                                                                  Position,
                                                                  Arguments);

        } else {
            if (LongDoubleSpecified != FALSE) {
                DoubleParts.Double = (double)va_arg(*Arguments, long double);

            } else {
                DoubleParts.Double = va_arg(*Arguments, double);
            }
        }

        Result = RtlpPrintDoubleWide(Context, DoubleParts.Double, &Properties);
        if (Result == FALSE) {
            goto ConvertFormatSpecifierWideEnd;
        }
    }

    Result = TRUE;

ConvertFormatSpecifierWideEnd:
    *Index += ((UINTN)CurrentFormat - (UINTN)(Format + *Index)) / sizeof(WCHAR);
    return Result;
}

ULONG
RtlpPrintIntegerWide (
    PPRINT_FORMAT_CONTEXT Context,
    ULONGLONG Integer,
    PPRINT_FORMAT_PROPERTIES Properties
    )

/*++

Routine Description:

    This routine prints an integer to the destination given the style
    properties.

Arguments:

    Context - Supplies a pointer to the initialized context structure.

    DestinationSize - Supplies the size of the destination buffer. If NULL was
        passed as the Destination argument, then this argument is ignored.

    Integer - Supplies the integer argument to convert to a string.

    Properties - Supplies the style characteristics to use when printing this
        integer.

Return Value:

    Returns the length of the final string after the format conversion has
    completed. The length will be returned even if NULL is passed as the
    destination.

--*/

{

    WCHAR Character;
    ULONG FieldCount;
    ULONG FieldIndex;
    ULONG IntegerLength;
    WCHAR LocalBuffer[MAX_INTEGER_STRING_SIZE];
    ULONG LocalIndex;
    BOOL Negative;
    ULONGLONG NextInteger;
    LONG Precision;
    ULONG PrecisionCount;
    ULONG PrecisionIndex;
    WCHAR Prefix[4];
    ULONG PrefixIndex;
    ULONG PrefixSize;
    ULONGLONG Remainder;
    BOOL Result;

    IntegerLength = 0;
    Negative = FALSE;
    Precision = Properties->Precision;
    if (Precision == -1) {
        Precision = 1;
    }

    //
    // Get the integer. If it's signed, allow it to be extended to a signed
    // long long (as a signed char is probably just sitting as 0x0000...00FF).
    //

    if (Properties->Unsigned == FALSE) {
        switch (Properties->IntegerSize) {
        case sizeof(CHAR):
            Integer = (CHAR)Integer;
            break;

        case sizeof(SHORT):
            Integer = (SHORT)Integer;
            break;

        case sizeof(LONG):
            Integer = (LONG)Integer;
            break;

        default:
            break;
        }
    }

    if (Integer == 0) {
        Properties->PrintRadix = FALSE;
    }

    if ((Integer != 0) || (Precision != 0)) {

        //
        // If the integer is signed and negative, make it positive.
        //

        if ((Properties->Unsigned == FALSE) && ((LONGLONG)Integer < 0)) {
            Negative = TRUE;
            Integer = -Integer;
        }

        //
        // Convert the integer into a reversed string.
        //

        RtlZeroMemory(LocalBuffer, sizeof(LocalBuffer));
        do {

            //
            // Get the least significant digit.
            //

            NextInteger = RtlDivideUnsigned64(Integer,
                                              Properties->Radix,
                                              &Remainder);

            Character = (WCHAR)Remainder;
            if (Character > 9) {
                if (Properties->PrintUpperCase != FALSE) {
                    Character = Character - 10 + L'A';

                } else {
                    Character = Character - 10 + L'a';
                }

            } else {
                Character += L'0';
            }

            //
            // Write out the character.
            //

            LocalBuffer[IntegerLength] = Character;
            IntegerLength += 1;

            //
            // Use the divided integer to get the next least significant digit.
            //

            Integer = NextInteger;

        } while (Integer > 0);

        //
        // Reverse the integer string.
        //

        RtlStringReverseWide(LocalBuffer, LocalBuffer + IntegerLength);
    }

    //
    // Figure out what kind of decorations can go on the integer. There could
    // be up to 1 character for the sign ('+', '-', or ' '), and up to two for
    // the radix ('0x').
    //

    PrefixSize = 0;
    if ((Properties->Unsigned == FALSE) && (Negative != FALSE)) {
        Prefix[PrefixSize] = L'-';
        PrefixSize += 1;

    } else if (Properties->AlwaysPrintSign != FALSE) {
        Prefix[PrefixSize] = L'+';
        PrefixSize += 1;

    } else if (Properties->SpaceForPlus != FALSE) {
        Prefix[PrefixSize] = L' ';
        PrefixSize += 1;
    }

    if (Properties->PrintRadix != FALSE) {
        if (Properties->Radix == 8) {
            if (LocalBuffer[0] != L'0') {
                Prefix[PrefixSize] = L'0';
                PrefixSize += 1;
            }

        } else if (Properties->Radix == 16) {
            Prefix[PrefixSize] = L'0';
            PrefixSize += 1;
            if (Properties->PrintUpperCase != 0) {
                Prefix[PrefixSize] = L'X';

            } else {
                Prefix[PrefixSize] = L'x';
            }

            PrefixSize += 1;
        }
    }

    //
    // Also remember if there are additional precision digits that will need to
    // go on the number.
    //

    PrecisionCount = 0;
    if (IntegerLength < Precision) {
        PrecisionCount = Precision - IntegerLength;
    }

    //
    // If the field width is bigger than the integer, there will need to be
    // some field spacing characters.
    //

    FieldCount = 0;
    if (IntegerLength + PrefixSize + PrecisionCount < Properties->FieldWidth) {
        FieldCount = Properties->FieldWidth -
                     (IntegerLength + PrefixSize + PrecisionCount);
    }

    //
    // Everything's ready, start writing out the number to the destination. If
    // the field is not left justified or leading zeros are supposed to be
    // printed, start with the extra field width.
    //

    if ((Properties->LeftJustified == FALSE) ||
        (Properties->PrintLeadingZeroes != FALSE)) {

        //
        // If the field is leading zero padding, then the prefix needs to go
        // first, otherwise -0001 would look like 00-1.
        //

        Character = L' ';
        if (Properties->PrintLeadingZeroes != FALSE) {
            Character = L'0';
            for (PrefixIndex = 0; PrefixIndex < PrefixSize; PrefixIndex += 1) {
                Result = RtlpFormatWriteCharacterWide(Context,
                                                      Prefix[PrefixIndex]);

                if (Result == FALSE) {
                    return FALSE;
                }
            }

            //
            // Zero out the prefix size so it won't be written again.
            //

            PrefixSize = 0;
        }

        for (FieldIndex = 0; FieldIndex < FieldCount; FieldIndex += 1) {
            Result = RtlpFormatWriteCharacterWide(Context, Character);
            if (Result == FALSE) {
                return FALSE;
            }
        }

        FieldCount = 0;
    }

    //
    // Now write the prefix, followed by the precision leading zeroes,
    // followed by the integer itself.
    //

    for (PrefixIndex = 0; PrefixIndex < PrefixSize; PrefixIndex += 1) {
        Result = RtlpFormatWriteCharacterWide(Context, Prefix[PrefixIndex]);
        if (Result == FALSE) {
            return FALSE;
        }
    }

    for (PrecisionIndex = 0;
         PrecisionIndex < PrecisionCount;
         PrecisionIndex += 1) {

        Result = RtlpFormatWriteCharacterWide(Context, L'0');
        if (Result == FALSE) {
            return FALSE;
        }
    }

    for (LocalIndex = 0; LocalIndex < IntegerLength; LocalIndex += 1) {
        Result = RtlpFormatWriteCharacterWide(Context, LocalBuffer[LocalIndex]);
        if (Result == FALSE) {
            return FALSE;
        }
    }

    //
    // Finally, if there are still field characters to be spit out, print them.
    // They must be spaces, as there can't be leading zeroes on the end.
    //

    for (FieldIndex = 0; FieldIndex < FieldCount; FieldIndex += 1) {
        Result = RtlpFormatWriteCharacterWide(Context, L' ');
        if (Result == FALSE) {
            return FALSE;
        }
    }

    return TRUE;
}

ULONG
RtlpPrintDoubleWide (
    PPRINT_FORMAT_CONTEXT Context,
    double Value,
    PPRINT_FORMAT_PROPERTIES Properties
    )

/*++

Routine Description:

    This routine prints a double to the destination given the style
    properties.

Arguments:

    Context - Supplies a pointer to the initialized context structure.

    DestinationSize - Supplies the size of the destination buffer. If NULL was
        passed as the Destination argument, then this argument is ignored.

    Value - Supplies a pointer to the value to convert to a string.

    Properties - Supplies the style characteristics to use when printing this
        integer.

Return Value:

    Returns the length of the final string after the format conversion has
    completed. The length will be returned even if NULL is passed as the
    destination.

--*/

{

    WCHAR Character;
    LONG CurrentExponent;
    WCHAR Digit;
    LONG DigitCount;
    LONG Exponent;
    WCHAR ExponentCharacter;
    ULONG ExponentIndex;
    WCHAR ExponentString[MAX_DOUBLE_EXPONENT_SIZE];
    ULONG FieldCount;
    ULONG FieldIndex;
    WCHAR LocalBuffer[MAX_DOUBLE_DIGITS_SIZE];
    ULONG LocalIndex;
    BOOL Negative;
    PWSTR NonNumberString;
    ULONG NumberLength;
    DOUBLE_PARTS Parts;
    LONG Precision;
    LONG PrecisionIndex;
    WCHAR Prefix;
    BOOL PrintExponent;
    BOOL Result;
    double RoundingAmount;
    LONG SignificantDigits;
    double TenPower;

    NumberLength = 0;
    Negative = FALSE;
    Parts.Double = Value;
    Precision = Properties->Precision;
    if (Precision == -1) {
        Precision = DEFAULT_FLOAT_PRECISION;
    }

    if ((Properties->SignificantDigitPrecision != FALSE) && (Precision == 0)) {
        Precision = 1;
    }

    Prefix = 0;

    //
    // Handle NaN and the infinities.
    //

    if ((Parts.Ulong.High & (~DOUBLE_SIGN_BIT >> DOUBLE_HIGH_WORD_SHIFT)) >=
        NAN_HIGH_WORD) {

        //
        // NaN is the only value that doesn't equal itself.
        //

        if (Value != Value) {
            if (Properties->PrintUpperCase != FALSE) {
                NonNumberString = L"NAN";

            } else {
                NonNumberString = L"nan";
            }

        //
        // Also handle positive and negative infinity.
        //

        } else {
            if (Properties->PrintUpperCase != FALSE) {
                NonNumberString = L"INF";

            } else {
                NonNumberString = L"inf";
            }

            if (Value < 0) {
                Negative = TRUE;
            }
        }

        //
        // Create a string in the local buffer containing a sign (maybe) and
        // the weird string.
        //

        LocalIndex = 0;
        if (Negative != FALSE) {
            LocalBuffer[LocalIndex] = L'-';
            LocalIndex += 1;

        } else if (Properties->AlwaysPrintSign != FALSE) {
            LocalBuffer[LocalIndex] = L'+';
            LocalIndex += 1;

        } else if (Properties->SpaceForPlus != FALSE) {
            LocalBuffer[LocalIndex] = L' ';
            LocalIndex += 1;
        }

        RtlStringCopyWide(LocalBuffer + LocalIndex,
                          NonNumberString,
                          sizeof(LocalBuffer) - LocalIndex);

        Result = RtlpPrintStringWide(Context,
                                     LocalBuffer,
                                     Properties->FieldWidth,
                                     Properties->Precision,
                                     Properties->LeftJustified,
                                     FALSE);

        return Result;
    }

    //
    // Use a special routine for hex formats.
    //

    if (Properties->Radix == 16) {
        return RtlpPrintHexDoubleWide(Context, Value, Properties);
    }

    //
    // If the value is negative, make it positive.
    //

    if ((Parts.Ulong.High & (DOUBLE_SIGN_BIT >> DOUBLE_HIGH_WORD_SHIFT)) != 0) {
        Negative = TRUE;
        Value = -Value;
    }

    //
    // Get the base 10 exponent of the value to determine whether or not to
    // print the exponent.
    //

    Exponent = RtlpGetDoubleBase10Exponent(Value, &TenPower);
    RoundingAmount = 0.5;

    //
    // Figure out whether or not to print the exponent. If not explicitly
    // specified, print it out if the exponent is less than -4 or greater than
    // the precision.
    //

    PrintExponent = Properties->ScientificFormat;
    if ((PrintExponent == FALSE) && (Properties->FloatFormat == FALSE)) {
        if ((Exponent < SCIENTIFIC_NOTATION_AUTO_LOWER_LIMIT) ||
            (Exponent >= Precision)) {

            PrintExponent = TRUE;
        }
    }

    DigitCount = 0;
    if (Value != 0.0) {

        //
        // In scientific notation or with significant digit based precision,
        // the rounding amount should be adjusted by the exponent.
        //

        if ((PrintExponent != FALSE) ||
            (Properties->SignificantDigitPrecision != FALSE)) {

            RoundingAmount /= TenPower;

            //
            // Scoot the rounding amount up by one because the loop below is
            // going to go one too far because it's not taking into account
            // the integral digit as a precision digit.
            //

            if (Properties->SignificantDigitPrecision != FALSE) {
                RoundingAmount *= 10.0;
            }
        }

        //
        // Figure out the rounding amount to add for the proper precision.
        //

        for (PrecisionIndex = 0;
             PrecisionIndex < Precision;
             PrecisionIndex += 1) {

            RoundingAmount *= 0.1;
        }

        Value += RoundingAmount;

        //
        // Normalize the value into the range 1 to 10 to take the rounding
        // amount into account.
        //

        Value = Value * TenPower;

        //
        // The rounding could have bumped it up by a power of 10 (ie 0.99999999
        // rounding to 1.000000, so adjust for that if needed.
        //

        if ((LONG)Value > 9) {
            Value *= 0.1;
            Exponent += 1;
        }

        //
        // Convert this batch of numbers into characters, not worrying about
        // the decimal point.
        //

        while ((Value != 0.0) && (DigitCount < MAX_DOUBLE_DIGITS_SIZE)) {
            LocalBuffer[DigitCount] = (LONG)Value + L'0';
            DigitCount += 1;
            Value = (Value - (double)(LONG)Value) * 10.0;
        }

        //
        // If significant digits matter, chop the digits down to the precision.
        // This lops off any digits that were added solely by the rounding
        // value.
        //

        if (Properties->SignificantDigitPrecision != FALSE) {

            ASSERT(Precision > 0);

            if (DigitCount > Precision) {
                DigitCount = Precision;
            }
        }

        //
        // Remove any zero characters on the end.
        //

        while ((DigitCount > 1) && (LocalBuffer[DigitCount - 1] == L'0')) {
            DigitCount -= 1;
        }
    }

    //
    // Figure out what kind of decorations can go on the integer. There could
    // be up to 1 character for the sign ('+', '-', or ' ').
    //

    if (Negative != FALSE) {
        Prefix = L'-';

    } else if (Properties->AlwaysPrintSign != FALSE) {
        Prefix = L'+';

    } else if (Properties->SpaceForPlus != FALSE) {
        Prefix = L' ';
    }

    //
    // If printing with significant digit precision, then the number of
    // significant digits is capped to the precision, and the precision
    // is capped to the number of significant digits. So %.4g with 0.01 prints
    // 0.01, and %.4g with 0.0123456 prints 0.1235.
    //

    SignificantDigits = DigitCount;
    if (Properties->SignificantDigitPrecision != FALSE)  {
        if (SignificantDigits > Precision) {
            SignificantDigits = Precision;
        }

        if (Precision > SignificantDigits) {
            Precision = SignificantDigits;

            //
            // For a number like 100, there's only one significant digit, but
            // a precision of 3 indicates that all three digits should be
            // printed.
            //

            if ((PrintExponent == FALSE) && ((Exponent + 1) > Precision)) {
                Precision = Exponent + 1;
            }

            if (Precision == 0) {
                Precision = 1;
            }
        }
    }

    NumberLength = Precision;

    //
    // Figure out if a radix character is going to come out of here.
    //

    if (Properties->PrintRadix != FALSE) {
        NumberLength += 1;

    } else if (Properties->SignificantDigitPrecision != FALSE) {
        if (PrintExponent != FALSE) {
            if (Precision > 1) {
                NumberLength += 1;
            }

        } else {

            //
            // A radix character is printed if the number of significant digits
            // (capped to the precision) is greater than the number of integral
            // digits. For example, 10.1 has 3 significant digits, only 2 of
            // which are integral, so any precision greater than 2 causes the
            // radix to be printed. Anything not in scientific notation with
            // a negative exponent also has a radix.
            //

            if ((Exponent < 0) || ((Exponent + 1) - SignificantDigits < 0)) {
                NumberLength += 1;
            }
        }

    } else if (Precision != 0) {
        NumberLength += 1;
    }

    //
    // Figure out the total length of the number.
    //

    if (PrintExponent != FALSE) {

        //
        // Add extras for the exponent character, sign, and (at least) two
        // exponent digits.
        //

        NumberLength += 4;

        //
        // If the precision only represents the fractional part, add one more
        // for the integer portion.
        //

        if (Properties->SignificantDigitPrecision == FALSE) {
            NumberLength += 1;
        }

        //
        // Figure out how wide the exponent is. Negative exponents look like
        // 1e-01.
        //

        if (Exponent < 0) {
            if (Exponent <= -100) {
                NumberLength += 1;
                if (Exponent <= -1000) {
                    NumberLength += 1;
                }
            }

        } else {
            if (Exponent >= 100) {
                NumberLength += 1;
                if (Exponent >= 1000) {
                    NumberLength += 1;
                }
            }
        }

    //
    // This is the regular float format where all the digits are printed.
    //

    } else {

        //
        // If the exponent is not negative, then the number of digits before
        // a radix character is the exponent.
        //

        if (Exponent >= 0) {
            if (Properties->SignificantDigitPrecision == FALSE) {
                NumberLength += Exponent + 1;
            }

        //
        // The exponent is negative, so add 1 for the zero.
        //

        } else {
            NumberLength += 1;

            //
            // If the precision is the fractional part, that's all that needs
            // to be done. If the precision is the number of significant digits,
            // add the exponent to the precision so that the precision again
            // just represents the fractional part.
            //

            if (Properties->SignificantDigitPrecision != FALSE) {
                Precision += (-Exponent) - 1;
                NumberLength += (-Exponent) - 1;
            }
        }
    }

    if (Prefix != 0) {
        NumberLength += 1;
    }

    //
    // If the field width is bigger than the integer, there will need to be
    // some field spacing characters.
    //

    FieldCount = 0;
    if (NumberLength < Properties->FieldWidth) {
        FieldCount = Properties->FieldWidth - NumberLength;
    }

    //
    // If the field is left justified or the extra field width is leading
    // zeroes, print the prefix now.
    //

    if ((Properties->LeftJustified != FALSE) ||
        (Properties->PrintLeadingZeroes != FALSE)) {

        if (Prefix != 0) {
            Result = RtlpFormatWriteCharacterWide(Context, Prefix);
            if (Result == FALSE) {
                return FALSE;
            }
        }

        //
        // Zero out the prefix so it won't be written again.
        //

        Prefix = 0;
    }

    //
    // If the field is not right justified or leading zeros are supposed to be
    // printed, spit out the extra field width.
    //

    if ((Properties->LeftJustified == FALSE) ||
        (Properties->PrintLeadingZeroes != FALSE)) {

        Character = L' ';
        if (Properties->PrintLeadingZeroes != FALSE) {
            Character = L'0';
        }

        for (FieldIndex = 0; FieldIndex < FieldCount; FieldIndex += 1) {
            Result = RtlpFormatWriteCharacterWide(Context, Character);
            if (Result == FALSE) {
                return FALSE;
            }
        }

        FieldCount = 0;
    }

    //
    // In the case of a right justified number with no leading zeroes, the
    // extra field width comes before the prefix. So print the prefix now if
    // it has not yet been printed.
    //

    if (Prefix != 0) {
        Result = RtlpFormatWriteCharacterWide(Context, Prefix);
        if (Result == FALSE) {
            return FALSE;
        }
    }

    //
    // Time to print the number itself.
    //

    LocalIndex = 0;
    if (PrintExponent != FALSE) {

        //
        // Print the first character, always.
        //

        if (DigitCount == 0) {
            Digit = L'0';

        } else {
            Digit = LocalBuffer[LocalIndex];

            ASSERT(Digit != L'0');

            LocalIndex += 1;
        }

        Result = RtlpFormatWriteCharacterWide(Context, Digit);
        if (Result == FALSE) {
            return FALSE;
        }

        //
        // If the precision is the number of significant digits, then this
        // guy counts as a significant digit.
        //

        if ((Properties->SignificantDigitPrecision != FALSE) &&
            (Precision != 0)) {

            Precision -= 1;
        }

        //
        // Print the radix character.
        //

        if ((Precision != 0) || (Properties->PrintRadix != FALSE)) {
            Result = RtlpFormatWriteCharacterWide(Context, L'.');
            if (Result == FALSE) {
                return FALSE;
            }
        }

        //
        // Print the rest of the desired precision.
        //

        for (PrecisionIndex = 0;
             PrecisionIndex < Precision;
             PrecisionIndex += 1) {

            if (LocalIndex < DigitCount) {
                Digit = LocalBuffer[LocalIndex];
                LocalIndex += 1;

            } else {
                Digit = L'0';
            }

            Result = RtlpFormatWriteCharacterWide(Context, Digit);
            if (Result == FALSE) {
                return FALSE;
            }
        }

        //
        // Determine the exponent character.
        //

        ExponentCharacter = L'e';
        if (Properties->PrintUpperCase != FALSE) {
            ExponentCharacter = L'E';
        }

        //
        // Print the exponent.
        //

        RtlPrintToStringWide(ExponentString,
                             MAX_DOUBLE_EXPONENT_SIZE,
                             Context->State.Encoding,
                             L"%c%+0.2d",
                             ExponentCharacter,
                             Exponent);

        for (ExponentIndex = 0;
             ExponentIndex < MAX_DOUBLE_EXPONENT_SIZE;
             ExponentIndex += 1) {

            if (ExponentString[ExponentIndex] == '\0') {
                break;
            }

            Result = RtlpFormatWriteCharacterWide(
                                                Context,
                                                ExponentString[ExponentIndex]);

            if (Result == FALSE) {
                return FALSE;
            }
        }

    //
    // This is being printed in non-scientific notation. Could be a lot of
    // zeros here.
    //

    } else {
        if (Exponent >= 0) {
            CurrentExponent = Exponent;

            //
            // Print the integral portion.
            //

            while (CurrentExponent >= 0) {
                if (LocalIndex < DigitCount) {
                    Digit = LocalBuffer[LocalIndex];
                    LocalIndex += 1;

                } else {
                    Digit = L'0';
                }

                Result = RtlpFormatWriteCharacterWide(Context, Digit);
                if (Result == FALSE) {
                    return FALSE;
                }

                CurrentExponent -= 1;

                //
                // Count this as a precision digit if the precision is the
                // number of significant digits.
                //

                if ((Properties->SignificantDigitPrecision != FALSE) &&
                    (Precision != 0)) {

                    Precision -= 1;
                }
            }

        //
        // Print the integer part, which is 0.
        //

        } else {
            Result = RtlpFormatWriteCharacterWide(Context, L'0');
            if (Result == FALSE) {
                return FALSE;
            }

            CurrentExponent = -1;
        }

        //
        // Print the radix character.
        //

        if ((Precision != 0) || (Properties->PrintRadix != FALSE)) {
            Result = RtlpFormatWriteCharacterWide(Context, L'.');
            if (Result == FALSE) {
                return FALSE;
            }
        }

        //
        // Print as many digits of precision as are desired. If the precision
        // is significant digits and the exponent is way negative, the
        // precision variable should have already been adjusted above.
        //

        for (PrecisionIndex = 0;
             PrecisionIndex < Precision;
             PrecisionIndex += 1) {

            //
            // If the current exponent has not yet met up with the exponent
            // of the digits, it's a leading zero (something like
            // 0.00000000000000000000000000012345.
            //

            if (CurrentExponent > Exponent) {
                Digit = L'0';

            } else if (LocalIndex < DigitCount) {
                Digit = LocalBuffer[LocalIndex];
                LocalIndex += 1;

            } else {
                Digit = L'0';
            }

            Result = RtlpFormatWriteCharacterWide(Context, Digit);
            if (Result == FALSE) {
                return FALSE;
            }

            CurrentExponent -= 1;
        }
    }

    //
    // Finally, if there are still field characters to be spit out, print them.
    // They must be spaces, as there can't be leading zeroes on the end.
    //

    for (FieldIndex = 0; FieldIndex < FieldCount; FieldIndex += 1) {
        Result = RtlpFormatWriteCharacterWide(Context, L' ');
        if (Result == FALSE) {
            return FALSE;
        }
    }

    return TRUE;
}

ULONG
RtlpPrintHexDoubleWide (
    PPRINT_FORMAT_CONTEXT Context,
    double Value,
    PPRINT_FORMAT_PROPERTIES Properties
    )

/*++

Routine Description:

    This routine prints a double to the destination in hex given the style
    properties.

Arguments:

    Context - Supplies a pointer to the initialized context structure.

    DestinationSize - Supplies the size of the destination buffer. If NULL was
        passed as the Destination argument, then this argument is ignored.

    Value - Supplies a pointer to the value to convert to a string.

    Properties - Supplies the style characteristics to use when printing this
        integer.

Return Value:

    Returns the length of the final string after the format conversion has
    completed. The length will be returned even if NULL is passed as the
    destination.

--*/

{

    LONG AbsoluteExponent;
    WCHAR Character;
    WCHAR Digit;
    LONG Exponent;
    WCHAR ExponentCharacter;
    WCHAR ExponentString[MAX_DOUBLE_EXPONENT_SIZE];
    ULONG FieldCount;
    ULONG FieldIndex;
    ULONGLONG HalfWay;
    WCHAR IntegerPortion;
    WCHAR LocalBuffer[MAX_DOUBLE_DIGITS_SIZE];
    ULONG LocalIndex;
    BOOL Negative;
    ULONG NumberLength;
    DOUBLE_PARTS Parts;
    LONG Precision;
    ULONG PrecisionIndex;
    WCHAR Prefix[4];
    ULONG PrefixIndex;
    ULONG PrefixSize;
    BOOL Result;
    ULONGLONG RoundingValue;
    ULONGLONG Significand;

    Negative = FALSE;
    Precision = Properties->Precision;
    Parts.Double = Value;

    //
    // If the integer is negative, make it positive.
    //

    if ((Parts.Ulong.High & (DOUBLE_SIGN_BIT >> DOUBLE_HIGH_WORD_SHIFT)) != 0) {
        Negative = TRUE;
        Parts.Double = -Parts.Double;
    }

    Exponent = (Parts.Ulong.High &
                (DOUBLE_EXPONENT_MASK >> DOUBLE_HIGH_WORD_SHIFT)) >>
               (DOUBLE_EXPONENT_SHIFT - DOUBLE_HIGH_WORD_SHIFT);

    Exponent -= DOUBLE_EXPONENT_BIAS;
    AbsoluteExponent = Exponent;
    if (AbsoluteExponent < 0) {
        AbsoluteExponent = -AbsoluteExponent;
    }

    if (Value == 0.0) {
        Exponent = 0;
        AbsoluteExponent = 0;
        Significand = 0;
        IntegerPortion = L'0';
        if (Precision == -1) {
            Precision = 0;
        }

        for (LocalIndex = 0;
             LocalIndex < DOUBLE_SIGNIFICAND_HEX_DIGITS;
             LocalIndex += 1) {

            LocalBuffer[LocalIndex] = L'0';
        }

    } else {
        Significand = Parts.Ulong.Low |
                      ((ULONGLONG)(Parts.Ulong.High & DOUBLE_HIGH_VALUE_MASK) <<
                       (sizeof(ULONG) * BITS_PER_BYTE));

        //
        // If there's a precision, add a half (8 of 16) to the digit beyond the
        // precision.
        //

        IntegerPortion = L'1';
        if (Precision != -1) {
            HalfWay = 1ULL << (DOUBLE_EXPONENT_SHIFT - 1);
            RoundingValue = HalfWay;
            if (4 * Precision > (sizeof(ULONGLONG) * BITS_PER_BYTE)) {
                RoundingValue = 0;

            } else {
                RoundingValue = RoundingValue >> (4 * Precision);
            }

            Significand += RoundingValue;
            if (Significand >= (1ULL << DOUBLE_EXPONENT_SHIFT)) {
                Significand -= (1ULL << DOUBLE_EXPONENT_SHIFT);
                IntegerPortion += 1;
            }
        }

        //
        // Convert the significand into a hex string.
        //

        ASSERT(MAX_DOUBLE_DIGITS_SIZE >= DOUBLE_SIGNIFICAND_HEX_DIGITS);

        for (LocalIndex = 0;
             LocalIndex < DOUBLE_SIGNIFICAND_HEX_DIGITS;
             LocalIndex += 1) {

            Digit = (Significand >> (LocalIndex * 4)) & 0xF;
            if (Digit < 10) {
                Character = Digit + L'0';

            } else if (Properties->PrintUpperCase != FALSE) {
                Character = Digit + L'A' - 10;

            } else {
                Character = Digit + L'a' - 10;
            }

            LocalBuffer[DOUBLE_SIGNIFICAND_HEX_DIGITS - LocalIndex - 1] =
                                                                     Character;
        }

        //
        // Figure out how many significant digits there are if there is no
        // precision.
        //

        if (Precision == -1) {
            Precision = DOUBLE_SIGNIFICAND_HEX_DIGITS;
            while ((Precision - 1 >= 0) &&
                   (LocalBuffer[Precision - 1] == L'0')) {

                Precision -= 1;
            }
        }
    }

    //
    // Figure out what kind of decorations can go on the integer. There could
    // be up to 1 character for the sign ('+', '-', or ' '), and up to two for
    // the radix ('0x').
    //

    PrefixSize = 0;
    if (Negative != FALSE) {
        Prefix[PrefixSize] = L'-';
        PrefixSize += 1;

    } else if (Properties->AlwaysPrintSign != FALSE) {
        Prefix[PrefixSize] = L'+';
        PrefixSize += 1;

    } else if (Properties->SpaceForPlus != FALSE) {
        Prefix[PrefixSize] = L' ';
        PrefixSize += 1;
    }

    Prefix[PrefixSize] = L'0';
    PrefixSize += 1;
    if (Properties->PrintUpperCase != 0) {
        Prefix[PrefixSize] = L'X';

    } else {
        Prefix[PrefixSize] = L'x';
    }

    PrefixSize += 1;

    //
    // Figure out the size of the number, which is the integer portion plus
    // the precision, plus one more for a radix character if there was a
    // precision.
    //

    NumberLength = 1 + Precision;
    if ((Properties->PrintRadix != FALSE) || (Precision != 0)) {
        NumberLength += 1;
    }

    //
    // Don't forget about the exponent (the 'p', a sign, and at least one
    // digit).
    //

    NumberLength += 3;
    if (AbsoluteExponent > 10) {
        NumberLength += 1;
        if (AbsoluteExponent > 100) {
            NumberLength += 1;
            if (AbsoluteExponent > 1000) {
                NumberLength += 1;
            }
        }
    }

    ExponentCharacter = L'p';
    if (Properties->PrintUpperCase != FALSE) {
        ExponentCharacter = L'P';
    }

    RtlPrintToStringWide(ExponentString,
                         sizeof(ExponentString),
                         Context->State.Encoding,
                         L"%c%+d",
                         ExponentCharacter,
                         Exponent);

    //
    // If the field width is bigger than the integer, there will need to be
    // some field spacing characters.
    //

    FieldCount = 0;
    if (NumberLength + PrefixSize < Properties->FieldWidth) {
        FieldCount = Properties->FieldWidth - (NumberLength + PrefixSize);
    }

    //
    // Everything's ready, start writing out the number to the destination. If
    // the field is not left justified or leading zeros are supposed to be
    // printed, start with the extra field width.
    //

    if ((Properties->LeftJustified == FALSE) ||
        (Properties->PrintLeadingZeroes != FALSE)) {

        //
        // If the field is leading zero padding, then the prefix needs to go
        // first, otherwise -0001 would look like 00-1.
        //

        Character = L' ';
        if (Properties->PrintLeadingZeroes != FALSE) {
            Character = L'0';
            for (PrefixIndex = 0; PrefixIndex < PrefixSize; PrefixIndex += 1) {
                Result = RtlpFormatWriteCharacterWide(Context,
                                                      Prefix[PrefixIndex]);

                if (Result == FALSE) {
                    return FALSE;
                }
            }

            //
            // Zero out the prefix size so it won't be written again.
            //

            PrefixSize = 0;
        }

        for (FieldIndex = 0; FieldIndex < FieldCount; FieldIndex += 1) {
            Result = RtlpFormatWriteCharacterWide(Context, Character);
            if (Result == FALSE) {
                return FALSE;
            }
        }

        FieldCount = 0;
    }

    //
    // Now write the prefix, followed by the precision leading zeroes,
    // followed by the integer itself.
    //

    for (PrefixIndex = 0; PrefixIndex < PrefixSize; PrefixIndex += 1) {
        Result = RtlpFormatWriteCharacterWide(Context, Prefix[PrefixIndex]);
        if (Result == FALSE) {
            return FALSE;
        }
    }

    //
    // Print the integer portion.
    //

    Result = RtlpFormatWriteCharacterWide(Context, IntegerPortion);
    if (Result == FALSE) {
        return FALSE;
    }

    //
    // Print a radix if needed.
    //

    if ((Properties->PrintRadix != FALSE) || (Precision != 0)) {
        Result = RtlpFormatWriteCharacterWide(Context, L'.');
        if (Result == FALSE) {
            return FALSE;
        }
    }

    //
    // Print the precision digits.
    //

    for (PrecisionIndex = 0; PrecisionIndex < Precision; PrecisionIndex += 1) {
        if (PrecisionIndex >= DOUBLE_SIGNIFICAND_HEX_DIGITS) {
            Digit = L'0';

        } else {
            Digit = LocalBuffer[PrecisionIndex];
        }

        Result = RtlpFormatWriteCharacterWide(Context, Digit);
        if (Result == FALSE) {
            return FALSE;
        }
    }

    //
    // Print the exponent.
    //

    RtlpPrintStringWide(Context, ExponentString, 0, -1, FALSE, FALSE);

    //
    // Finally, if there are still field characters to be spit out, print them.
    // They must be spaces, as there can't be leading zeroes on the end.
    //

    for (FieldIndex = 0; FieldIndex < FieldCount; FieldIndex += 1) {
        Result = RtlpFormatWriteCharacterWide(Context, L' ');
        if (Result == FALSE) {
            return FALSE;
        }
    }

    return TRUE;
}

BOOL
RtlpPrintStringWide (
    PPRINT_FORMAT_CONTEXT Context,
    PWSTR String,
    LONG FieldWidth,
    LONG Precision,
    BOOL LeftJustified,
    BOOL Character
    )

/*++

Routine Description:

    This routine prints a string destination buffer given the style properties.

Arguments:

    Context - Supplies a pointer to the initialized context structure.

    String - Supplies a pointer to the string to print.

    FieldWidth - Supplies the width of the string or character field. If the
        argument doesn't fill up this space, it will be padded with spaces.

    Precision - Supplies the precision of the string (the maximum number of
        characters to print). Supply -1 to print the whole string.

    LeftJustified - Supplies a flag indicating whether or not the character in
        the string is to be left justfied.

    Character - Supplies a boolean indicating that this is a character rather
        than a full string.

Return Value:

    TRUE if all characters were written to the destination.

    FALSE if the destination crapped out before all characters could be written.

--*/

{

    ULONG PaddingIndex;
    ULONG PaddingLength;
    BOOL Result;
    ULONG StringLength;

    if (String == NULL) {
        String = L"(null)";
    }

    if (Character != FALSE) {
        StringLength = 1;

    } else {
        StringLength = RtlStringLengthWide(String);
    }

    if ((Precision >= 0) && (StringLength > Precision)) {
        StringLength = Precision;
    }

    //
    // Find out how much padding to add to the field.
    //

    PaddingLength = 0;
    if (FieldWidth > StringLength) {
        PaddingLength = FieldWidth - StringLength;
    }

    PaddingIndex = PaddingLength;

    //
    // Pad left, if required.
    //

    if (LeftJustified == FALSE) {
        while (PaddingIndex > 0) {
            Result = RtlpFormatWriteCharacterWide(Context, L' ');
            if (Result == FALSE) {
                return FALSE;
            }

            PaddingIndex -= 1;
        }
    }

    //
    // Copy the string.
    //

    while (StringLength != 0) {
        Result = RtlpFormatWriteCharacterWide(Context, *String);
        if (Result == FALSE) {
            return FALSE;
        }

        String += 1;
        StringLength -= 1;
    }

    //
    // Pad right, if required.
    //

    while (PaddingIndex > 0) {
        Result = RtlpFormatWriteCharacterWide(Context, L' ');
        if (Result == FALSE) {
            return FALSE;
        }

        PaddingIndex -= 1;
    }

    return TRUE;
}

BOOL
RtlpPrintByteStringWide (
    PPRINT_FORMAT_CONTEXT Context,
    PSTR String,
    LONG FieldWidth,
    LONG Precision,
    BOOL LeftJustified,
    BOOL Character
    )

/*++

Routine Description:

    This routine prints a byte-based string to a wide print destination.

Arguments:

    Context - Supplies a pointer to the initialized context structure.

    String - Supplies a pointer to the string to print.

    FieldWidth - Supplies the width of the string or character field. If the
        argument doesn't fill up this space, it will be padded with spaces.

    Precision - Supplies the precision of the string (the maximum number of
        characters to print). Supply -1 to print the whole string.

    LeftJustified - Supplies a flag indicating whether or not the character in
        the string is to be left justfied.

    Character - Supplies a boolean indicating that this is a character rather
        than a full string.

Return Value:

    TRUE if all characters were written to the destination.

    FALSE if the destination crapped out before all characters could be written.

--*/

{

    ULONG PaddingIndex;
    ULONG PaddingLength;
    BOOL Result;
    KSTATUS Status;
    ULONG StringLength;
    WCHAR WideCharacter;

    if (String == NULL) {
        String = "(null)";
    }

    if (Character != FALSE) {
        StringLength = 1;

    } else {
        StringLength = RtlStringLength(String);
    }

    //
    // Find out how much padding to add to the field.
    //

    PaddingLength = 0;
    if (FieldWidth > StringLength) {
        PaddingLength = FieldWidth - StringLength;
    }

    PaddingIndex = PaddingLength;

    //
    // Pad left, if required.
    //

    if (LeftJustified == FALSE) {
        while (PaddingIndex > 0) {
            Result = RtlpFormatWriteCharacterWide(Context, L' ');
            if (Result == FALSE) {
                return FALSE;
            }

            PaddingIndex -= 1;
        }
    }

    //
    // Copy the string by converting bytes into wide characters.
    //

    while (StringLength != 0) {
        Status = RtlConvertMultibyteCharacterToWide(&String,
                                                    &StringLength,
                                                    &WideCharacter,
                                                    &(Context->State));

        if (!KSUCCESS(Status)) {
            return FALSE;
        }

        Result = RtlpFormatWriteCharacterWide(Context, WideCharacter);
        if (Result == FALSE) {
            return FALSE;
        }
    }

    //
    // Pad right, if required.
    //

    while (PaddingIndex > 0) {
        Result = RtlpFormatWriteCharacterWide(Context, L' ');
        if (Result == FALSE) {
            return FALSE;
        }

        PaddingIndex -= 1;
    }

    return TRUE;
}

BOOL
RtlpFormatWriteCharacterWide (
    PPRINT_FORMAT_CONTEXT Context,
    WCHAR Character
    )

/*++

Routine Description:

    This routine writes a wide character to the print format destination.

Arguments:

    Context - Supplies a pointer to the print format context.

    Character - Supplies the character to write.

Return Value:

    TRUE if the character was written.

    FALSE on failure.

--*/

{

    BOOL Result;

    Result = Context->U.WriteWideCharacter(Character, Context);
    if (Result == FALSE) {
        return FALSE;
    }

    Context->CharactersWritten += 1;
    return TRUE;
}

ULONGLONG
RtlpGetPositionalArgumentWide (
    PCWSTR Format,
    ULONG ArgumentNumber,
    va_list *Arguments
    )

/*++

Routine Description:

    This routine attempts to get a positional argument by rescanning the
    string from the beginning and counting up all arguments less than it. This
    is more than a little slow (O(N^2) for each argument), but it doesn't
    require allocations, which is nice for a library like this shared between
    several environments.

Arguments:

    Format - Supplies the format string.

    ArgumentNumber - Supplies the argument number to retrieve.

    Arguments - Supplies a pointer to a VA list initialized at the beginning
        of the printf arguments. This list will be copied.

Return Value:

    Returns the argument.

--*/

{

    ULONGLONG Argument;
    ULONG ArgumentIndex;
    va_list ArgumentsCopy;
    ULONG ArgumentSize;

    ASSERT(ArgumentNumber != 0);

    va_copy(ArgumentsCopy, *Arguments);
    for (ArgumentIndex = 1;
         ArgumentIndex < ArgumentNumber;
         ArgumentIndex += 1) {

        //
        // Get the size of this argument.
        //

        ArgumentSize = RtlpGetPositionalArgumentSizeWide(Format, ArgumentIndex);
        switch (ArgumentSize) {
        case 0:
            break;

        case sizeof(CHAR):
            Argument = va_arg(ArgumentsCopy, INT);
            break;

        case sizeof(SHORT):
            Argument = va_arg(ArgumentsCopy, INT);
            break;

        case sizeof(LONG):
            Argument = va_arg(ArgumentsCopy, LONG);
            break;

        case sizeof(LONGLONG):
            Argument = va_arg(ArgumentsCopy, LONGLONG);
            break;

        default:

            ASSERT(FALSE);

            break;
        }
    }

    //
    // Now the important one, get the size of the specified argument.
    //

    Argument = 0;
    ArgumentSize = RtlpGetPositionalArgumentSizeWide(Format, ArgumentNumber);
    switch (ArgumentSize) {
    case 0:
        break;

    case sizeof(CHAR):
        Argument = (UCHAR)va_arg(ArgumentsCopy, INT);
        break;

    case sizeof(SHORT):
        Argument = (USHORT)va_arg(ArgumentsCopy, INT);
        break;

    case sizeof(LONG):
        Argument = va_arg(ArgumentsCopy, LONG);
        break;

    case sizeof(LONGLONG):
        Argument = va_arg(ArgumentsCopy, LONGLONG);
        break;

    default:

        ASSERT(FALSE);

        break;
    }

    va_end(ArgumentsCopy);
    return Argument;
}

ULONG
RtlpGetPositionalArgumentSizeWide (
    PCWSTR Format,
    ULONG ArgumentNumber
    )

/*++

Routine Description:

    This routine scans through the format string to determine the size of the
    given positional argument.

Arguments:

    Format - Supplies the format string.

    ArgumentNumber - Supplies the argument number to retrieve.

Return Value:

    Returns the size of the argument.

    0 if the given positional argument was not found.

--*/

{

    ULONG ArgumentSize;
    ULONG CurrentArgumentSize;
    LONGLONG Integer;
    ULONG Position;
    ULONG RemainingSize;
    KSTATUS Status;

    ArgumentSize = 0;
    while (*Format != WIDE_STRING_TERMINATOR) {
        if (*Format != CONVERSION_CHARACTER) {
            Format += 1;
            continue;
        }

        Position = 0;
        Format += 1;

        //
        // If there's a non-zero digit, grab it. It could be the position or
        // field width.
        //

        if ((*Format >= L'1') && (*Format <= L'9')) {
            RemainingSize = -1;
            Status = RtlStringScanIntegerWide(&Format,
                                              &RemainingSize,
                                              10,
                                              FALSE,
                                              &Integer);

            if (!KSUCCESS(Status)) {
                return 0;
            }

            if (*Format == POSITIONAL_ARGUMENT) {
                Position = (ULONG)Integer;
                Format += 1;
            }
        }

        //
        // Get past any flags.
        //

        while (TRUE) {
            if ((*Format != THOUSANDS_GROUPING) &&
                (*Format != LEFT_JUSTIFIED) &&
                (*Format != SPACE_FOR_PLUS) &&
                (*Format != PRINT_SIGN) &&
                (*Format != PRINT_RADIX_IDENTIFIER) &&
                (*Format != PRINT_LEADING_ZEROES)) {

                break;
            }

            Format += 1;
        }

        //
        // Process a field width. It could have already been sucked in, be a
        // decimal, be a star, or be a star followed by a position and a dollar
        // sign.
        //

        if (*Format == FIELD_IN_ARGUMENT) {
            Format += 1;
            if ((*Format >= L'1') && (*Format <= L'9')) {
                RemainingSize = -1;
                Status = RtlStringScanIntegerWide(&Format,
                                                  &RemainingSize,
                                                  10,
                                                  FALSE,
                                                  &Integer);

                if ((!KSUCCESS(Status)) || (Integer < 0)) {
                    return 0;
                }

                if (*Format != POSITIONAL_ARGUMENT) {
                    return 0;
                }

                Format += 1;

                //
                // This is a positional argument and its size is int.
                //

                if ((Integer == ArgumentNumber) &&
                    (ArgumentSize < sizeof(INT))) {

                    ArgumentSize = sizeof(INT);
                }
            }

        } else if ((*Format >= L'1') && (*Format <= L'9')) {
            RemainingSize = -1;
            RtlStringScanIntegerWide(&Format,
                                     &RemainingSize,
                                     10,
                                     FALSE,
                                     &Integer);
        }

        //
        // If there's a dot, then the precision follows. Like the field width,
        // it could either be a decimal, a star, or a star plus a position and a
        // dollar sign.
        //

        if (*Format == PRECISION_SPECIFIED) {
            Format += 1;
            if (*Format == FIELD_IN_ARGUMENT) {
                Format += 1;
                if ((*Format >= L'1') && (*Format <= L'9')) {
                    RemainingSize = -1;
                    Status = RtlStringScanIntegerWide(&Format,
                                                      &RemainingSize,
                                                      10,
                                                      FALSE,
                                                      &Integer);

                    if ((!KSUCCESS(Status)) || (Integer < 0)) {
                        return 0;
                    }

                    if (*Format != POSITIONAL_ARGUMENT) {
                        return 0;
                    }

                    Format += 1;

                    //
                    // This is a positional argument and its size is int.
                    //

                    if ((Integer == ArgumentNumber) &&
                        (ArgumentSize < sizeof(INT))) {

                        ArgumentSize = sizeof(INT);
                    }
                }

            } else if ((*Format >= L'1') && (*Format <= L'9')) {
                RemainingSize = -1;
                RtlStringScanIntegerWide(&Format,
                                         &RemainingSize,
                                         10,
                                         FALSE,
                                         &Integer);
            }
        }

        //
        // Look for the length modifiers: hh, h, l, ll, j, z, t, L, I64.
        //

        CurrentArgumentSize = sizeof(INT);
        if (*Format == FORMAT_SHORT) {
            Format += 1;
            CurrentArgumentSize = sizeof(SHORT);
            if (*Format == FORMAT_SHORT) {
                Format += 1;
                CurrentArgumentSize = sizeof(CHAR);
            }

        } else if (*Format == FORMAT_LONG) {
            Format += 1;
            CurrentArgumentSize = sizeof(LONG);
            if (*Format == FORMAT_LONG) {
                Format += 1;
                CurrentArgumentSize = sizeof(LONGLONG);
            }

        } else if (*Format == FORMAT_INTMAX) {
            Format += 1;
            CurrentArgumentSize = sizeof(intmax_t);

        } else if (*Format == FORMAT_SIZE_T) {
            Format += 1;
            CurrentArgumentSize = sizeof(size_t);

        } else if (*Format == FORMAT_LONG_DOUBLE) {
            Format += 1;
            CurrentArgumentSize = sizeof(long double);

        } else if ((*Format == FORMAT_LONGLONG_START) &&
                   (*(Format + 1) == L'6') &&
                   (*(Format + 2) == L'4')) {

            Format += 3;
            CurrentArgumentSize = sizeof(LONGLONG);
        }

        //
        // Now, finally, get the conversion specifier.
        //

        if ((*Format == FORMAT_POINTER) || (*Format == FORMAT_BYTES_PRINTED)) {
            CurrentArgumentSize = sizeof(PVOID);

        } else if (*Format == FORMAT_LONG_CHARACTER) {
            CurrentArgumentSize = sizeof(SHORT);

        } else if (*Format == FORMAT_CHARACTER) {
            CurrentArgumentSize = sizeof(CHAR);

        } else if ((*Format == FORMAT_LONG_STRING) ||
                   (*Format == FORMAT_STRING)) {

            CurrentArgumentSize = sizeof(PVOID);

        } else if (*Format == FORMAT_NONE) {
            CurrentArgumentSize = 0;
        }

        //
        // If the argument is the right position, up the argument size.
        //

        if ((Position == ArgumentNumber) &&
            (CurrentArgumentSize > ArgumentSize)) {

            ArgumentSize = CurrentArgumentSize;
        }

        Format += 1;
    }

    return ArgumentSize;
}

BOOL
RtlpStringFormatWriteCharacterWide (
    WCHAR Character,
    PPRINT_FORMAT_CONTEXT Context
    )

/*++

Routine Description:

    This routine writes a character to the string during a printf-style
    formatting operation.

Arguments:

    Character - Supplies the character to be written.

    Context - Supplies a pointer to the printf-context.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PWSTR String;

    String = Context->Context;
    if ((String != NULL) && (Context->CharactersWritten < Context->Limit)) {
        String[Context->CharactersWritten] = Character;
    }

    return TRUE;
}

