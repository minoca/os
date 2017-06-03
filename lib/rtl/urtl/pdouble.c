/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    pdouble.c

Abstract:

    This module handles printing a floating point value.

Author:

    Evan Green 2-Jun-2017

Environment:

    User Mode

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

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

ULONG
RtlpPrintHexDouble (
    PPRINT_FORMAT_CONTEXT Context,
    double Value,
    PPRINT_FORMAT_PROPERTIES Properties
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

VOID
RtlpGetDoubleArgument (
    BOOL LongDouble,
    va_list *ArgumentList,
    PDOUBLE_PARTS DoubleParts
    )

/*++

Routine Description:

    This routine gets a double from the argument list. It is used by printf,
    and is a separate function so that floating point support can be shaved out
    of the library.

Arguments:

    LongDouble - Supplies a boolean indicating if the argument is a long double
        or just a regular double.

    ArgumentList - Supplies a pointer to the VA argument list. It's a pointer
        so that the effect of the va_arg can be felt by the calling function.

    DoubleParts - Supplies a pointer where the double is returned, disguised in
        a structure so as not to force floating point arguments.

Return Value:

    None.

--*/

{

    if (LongDouble != FALSE) {
        DoubleParts->Double = va_arg(*ArgumentList, long double);

    } else {
        DoubleParts->Double = va_arg(*ArgumentList, double);
    }

    return;
}

BOOL
RtlpPrintDouble (
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

    TRUE on success.

    FALSE on failure.

--*/

{

    UCHAR Character;
    LONG CurrentExponent;
    CHAR Digit;
    LONG DigitCount;
    LONG Exponent;
    CHAR ExponentCharacter;
    ULONG ExponentIndex;
    CHAR ExponentString[MAX_DOUBLE_EXPONENT_SIZE];
    ULONG FieldCount;
    ULONG FieldIndex;
    CHAR LocalBuffer[MAX_DOUBLE_DIGITS_SIZE];
    ULONG LocalIndex;
    BOOL Negative;
    PSTR NonNumberString;
    ULONG NumberLength;
    DOUBLE_PARTS Parts;
    LONG Precision;
    LONG PrecisionIndex;
    CHAR Prefix;
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
                NonNumberString = "NAN";

            } else {
                NonNumberString = "nan";
            }

        //
        // Also handle positive and negative infinity.
        //

        } else {
            if (Properties->PrintUpperCase != FALSE) {
                NonNumberString = "INF";

            } else {
                NonNumberString = "inf";
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
            LocalBuffer[LocalIndex] = '-';
            LocalIndex += 1;

        } else if (Properties->AlwaysPrintSign != FALSE) {
            LocalBuffer[LocalIndex] = '+';
            LocalIndex += 1;

        } else if (Properties->SpaceForPlus != FALSE) {
            LocalBuffer[LocalIndex] = ' ';
            LocalIndex += 1;
        }

        RtlStringCopy(LocalBuffer + LocalIndex,
                      NonNumberString,
                      sizeof(LocalBuffer) - LocalIndex);

        Result = RtlpPrintString(Context,
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
        return RtlpPrintHexDouble(Context, Value, Properties);
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
            LocalBuffer[DigitCount] = (LONG)Value + '0';
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

        while ((DigitCount > 1) && (LocalBuffer[DigitCount - 1] == '0')) {
            DigitCount -= 1;
        }
    }

    //
    // Figure out what kind of decorations can go on the integer. There could
    // be up to 1 character for the sign ('+', '-', or ' ').
    //

    if (Negative != FALSE) {
        Prefix = '-';

    } else if (Properties->AlwaysPrintSign != FALSE) {
        Prefix = '+';

    } else if (Properties->SpaceForPlus != FALSE) {
        Prefix = ' ';
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
            Result = RtlpFormatWriteCharacter(Context, Prefix);
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

        Character = ' ';
        if (Properties->PrintLeadingZeroes != FALSE) {
            Character = '0';
        }

        for (FieldIndex = 0; FieldIndex < FieldCount; FieldIndex += 1) {
            Result = RtlpFormatWriteCharacter(Context, Character);
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
        Result = RtlpFormatWriteCharacter(Context, Prefix);
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
            Digit = '0';

        } else {
            Digit = LocalBuffer[LocalIndex];

            ASSERT(Digit != '0');

            LocalIndex += 1;
        }

        Result = RtlpFormatWriteCharacter(Context, Digit);
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
            Result = RtlpFormatWriteCharacter(Context, '.');
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
                Digit = '0';
            }

            Result = RtlpFormatWriteCharacter(Context, Digit);
            if (Result == FALSE) {
                return FALSE;
            }
        }

        //
        // Determine the exponent character.
        //

        ExponentCharacter = 'e';
        if (Properties->PrintUpperCase != FALSE) {
            ExponentCharacter = 'E';
        }

        //
        // Print the exponent.
        //

        RtlPrintToString(ExponentString,
                         MAX_DOUBLE_EXPONENT_SIZE,
                         Context->State.Encoding,
                         "%c%+0.2d",
                         ExponentCharacter,
                         Exponent);

        for (ExponentIndex = 0;
             ExponentIndex < MAX_DOUBLE_EXPONENT_SIZE;
             ExponentIndex += 1) {

            if (ExponentString[ExponentIndex] == '\0') {
                break;
            }

            Result = RtlpFormatWriteCharacter(Context,
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
                    Digit = '0';
                }

                Result = RtlpFormatWriteCharacter(Context, Digit);
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
            Result = RtlpFormatWriteCharacter(Context, '0');
            if (Result == FALSE) {
                return FALSE;
            }

            CurrentExponent = -1;
        }

        //
        // Print the radix character.
        //

        if ((Precision != 0) || (Properties->PrintRadix != FALSE)) {
            Result = RtlpFormatWriteCharacter(Context, '.');
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
                Digit = '0';

            } else if (LocalIndex < DigitCount) {
                Digit = LocalBuffer[LocalIndex];
                LocalIndex += 1;

            } else {
                Digit = '0';
            }

            Result = RtlpFormatWriteCharacter(Context, Digit);
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
        Result = RtlpFormatWriteCharacter(Context, ' ');
        if (Result == FALSE) {
            return FALSE;
        }
    }

    return TRUE;
}

LONG
RtlpGetDoubleBase10Exponent (
    double Value,
    double *InversePowerOfTen
    )

/*++

Routine Description:

    This routine gets the base 10 exponent of the given double.

Arguments:

    Value - Supplies the value to get the base 10 exponent of.

    InversePowerOfTen - Supplies a pointer where the power of 10 correponding
        to the returned exponent will be returned.

Return Value:

    Returns the base 10 exponent of the given value.

--*/

{

    LONG Base2Exponent;
    LONG CurrentExponent;
    LONG Exponent;
    ULONG ExponentMask;
    ULONG ExponentShift;
    DOUBLE_PARTS Parts;
    double TenPower;

    ExponentMask = DOUBLE_EXPONENT_MASK >> DOUBLE_HIGH_WORD_SHIFT;
    ExponentShift = DOUBLE_EXPONENT_SHIFT - DOUBLE_HIGH_WORD_SHIFT;
    if (Value == 0.0) {
        *InversePowerOfTen = 1.0;
        return 0;
    }

    Parts.Double = Value;
    Base2Exponent = ((Parts.Ulong.High & ExponentMask) >> ExponentShift) -
                    DOUBLE_EXPONENT_BIAS;

    //
    // Get the base 10 exponent by multiplying by log10(2).
    //

    Exponent = (LONG)((double)Base2Exponent * LOG2) + 1;

    //
    // Make a double with the inverse of that power of 10 to get the value in
    // the range of 1 to 10.
    //

    CurrentExponent = 0;
    TenPower = 1.0;
    if (Exponent > 0) {
        while (CurrentExponent + 10 <= Exponent) {
            TenPower *= 1.0E-10;
            CurrentExponent += 10;
        }

        while (CurrentExponent + 1 <= Exponent) {
            TenPower *= 0.1;
            CurrentExponent += 1;
        }

    } else {
        while (CurrentExponent - 10 >= Exponent) {
            TenPower *= 1.0E10;
            CurrentExponent -= 10;
        }

        while (CurrentExponent - 1 >= Exponent) {
            TenPower *= 10.0;
            CurrentExponent -= 1;
        }
    }

    //
    // Normalize the value.
    //

    Value *= TenPower;

    //
    // Skip any leading zeros.
    //

    while ((Value != 0.0) && ((LONG)Value == 0)) {
        Value *= 10.0;
        Exponent -= 1;
        TenPower *= 10.0;
    }

    *InversePowerOfTen = TenPower;
    return Exponent;
}

//
// --------------------------------------------------------- Internal Functions
//

ULONG
RtlpPrintHexDouble (
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
    UCHAR Character;
    CHAR Digit;
    LONG Exponent;
    CHAR ExponentCharacter;
    CHAR ExponentString[MAX_DOUBLE_EXPONENT_SIZE];
    ULONG FieldCount;
    ULONG FieldIndex;
    ULONGLONG HalfWay;
    CHAR IntegerPortion;
    CHAR LocalBuffer[MAX_DOUBLE_DIGITS_SIZE];
    ULONG LocalIndex;
    BOOL Negative;
    ULONG NumberLength;
    DOUBLE_PARTS Parts;
    LONG Precision;
    ULONG PrecisionIndex;
    UCHAR Prefix[4];
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
        IntegerPortion = '0';
        if (Precision == -1) {
            Precision = 0;
        }

        for (LocalIndex = 0;
             LocalIndex < DOUBLE_SIGNIFICAND_HEX_DIGITS;
             LocalIndex += 1) {

            LocalBuffer[LocalIndex] = '0';
        }

    } else {
        Significand = Parts.Ulong.Low |
                      ((ULONGLONG)(Parts.Ulong.High & DOUBLE_HIGH_VALUE_MASK) <<
                       (sizeof(ULONG) * BITS_PER_BYTE));

        //
        // If there's a precision, add a half (8 of 16) to the digit beyond the
        // precision.
        //

        IntegerPortion = '1';
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
                Character = Digit + '0';

            } else if (Properties->PrintUpperCase != FALSE) {
                Character = Digit + 'A' - 10;

            } else {
                Character = Digit + 'a' - 10;
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
                   (LocalBuffer[Precision - 1] == '0')) {

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
        Prefix[PrefixSize] = '-';
        PrefixSize += 1;

    } else if (Properties->AlwaysPrintSign != FALSE) {
        Prefix[PrefixSize] = '+';
        PrefixSize += 1;

    } else if (Properties->SpaceForPlus != FALSE) {
        Prefix[PrefixSize] = ' ';
        PrefixSize += 1;
    }

    Prefix[PrefixSize] = '0';
    PrefixSize += 1;
    if (Properties->PrintUpperCase != 0) {
        Prefix[PrefixSize] = 'X';

    } else {
        Prefix[PrefixSize] = 'x';
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

    ExponentCharacter = 'p';
    if (Properties->PrintUpperCase != FALSE) {
        ExponentCharacter = 'P';
    }

    RtlPrintToString(ExponentString,
                     sizeof(ExponentString),
                     Context->State.Encoding,
                     "%c%+d",
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

        Character = ' ';
        if (Properties->PrintLeadingZeroes != FALSE) {
            Character = '0';
            for (PrefixIndex = 0; PrefixIndex < PrefixSize; PrefixIndex += 1) {
                Result = RtlpFormatWriteCharacter(Context, Prefix[PrefixIndex]);
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
            Result = RtlpFormatWriteCharacter(Context, Character);
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
        Result = RtlpFormatWriteCharacter(Context, Prefix[PrefixIndex]);
        if (Result == FALSE) {
            return FALSE;
        }
    }

    //
    // Print the integer portion.
    //

    Result = RtlpFormatWriteCharacter(Context, IntegerPortion);
    if (Result == FALSE) {
        return FALSE;
    }

    //
    // Print a radix if needed.
    //

    if ((Properties->PrintRadix != FALSE) || (Precision != 0)) {
        Result = RtlpFormatWriteCharacter(Context, '.');
        if (Result == FALSE) {
            return FALSE;
        }
    }

    //
    // Print the precision digits.
    //

    for (PrecisionIndex = 0; PrecisionIndex < Precision; PrecisionIndex += 1) {
        if (PrecisionIndex >= DOUBLE_SIGNIFICAND_HEX_DIGITS) {
            Digit = '0';

        } else {
            Digit = LocalBuffer[PrecisionIndex];
        }

        Result = RtlpFormatWriteCharacter(Context, Digit);
        if (Result == FALSE) {
            return FALSE;
        }
    }

    //
    // Print the exponent.
    //

    RtlpPrintString(Context, ExponentString, 0, -1, FALSE, FALSE);

    //
    // Finally, if there are still field characters to be spit out, print them.
    // They must be spaces, as there can't be leading zeroes on the end.
    //

    for (FieldIndex = 0; FieldIndex < FieldCount; FieldIndex += 1) {
        Result = RtlpFormatWriteCharacter(Context, ' ');
        if (Result == FALSE) {
            return FALSE;
        }
    }

    return TRUE;
}

