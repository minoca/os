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
    INT Character,
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
    Context.WriteCharacter = RtlpStringFormatWriteCharacterWide;
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

    RtlpFormatWriteCharacter(&Context, WIDE_STRING_TERMINATOR);
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

    ASSERT((Context != NULL) && (Context->WriteCharacter != NULL) &&
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
            Result = RtlpFormatWriteCharacter(Context, Format[Index]);
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
            Result = RtlpPrintInteger(Context, IntegerArgument, &Properties);
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
            RtlpGetDoubleArgument(LongDoubleSpecified,
                                  Arguments,
                                  &DoubleParts);
        }

        Result = RtlpPrintDouble(Context, DoubleParts.Double, &Properties);
        if (Result == FALSE) {
            goto ConvertFormatSpecifierWideEnd;
        }
    }

    Result = TRUE;

ConvertFormatSpecifierWideEnd:
    *Index += ((UINTN)CurrentFormat - (UINTN)(Format + *Index)) / sizeof(WCHAR);
    return Result;
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
            Result = RtlpFormatWriteCharacter(Context, L' ');
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
        Result = RtlpFormatWriteCharacter(Context, *String);
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
        Result = RtlpFormatWriteCharacter(Context, L' ');
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
            Result = RtlpFormatWriteCharacter(Context, L' ');
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

        Result = RtlpFormatWriteCharacter(Context, WideCharacter);
        if (Result == FALSE) {
            return FALSE;
        }
    }

    //
    // Pad right, if required.
    //

    while (PaddingIndex > 0) {
        Result = RtlpFormatWriteCharacter(Context, L' ');
        if (Result == FALSE) {
            return FALSE;
        }

        PaddingIndex -= 1;
    }

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
    INT Character,
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

