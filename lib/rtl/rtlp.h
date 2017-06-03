/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    rtlp.h

Abstract:

    This header contains internal definitions for the Runtime Library.

Author:

    Evan Green 19-Feb-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

#ifndef RTL_API

#define RTL_API __DLLPROTECTED

#endif

//
// Define away the kernel API decorator since Rtl is always linked statically
// with the kernel.
//

#define KERNEL_API

#include <minoca/lib/types.h>
#include <minoca/lib/status.h>
#include <minoca/lib/rtl.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define common print format values.
//

#define DEFAULT_FLOAT_PRECISION 6
#define MAX_DOUBLE_DIGITS_SIZE 15
#define MAX_DOUBLE_EXPONENT_SIZE 7
#define SCIENTIFIC_NOTATION_AUTO_LOWER_LIMIT -4
#define LOG2 0.30102999566398119521

//
// Define the string size of the longest possible integer,
// 01000000000000000000000 (octal).
//

#define MAX_INTEGER_STRING_SIZE 24

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define some types the C library expects but that aren't necessarily defined
// in the kernel.
//

typedef __INTMAX_TYPE__ intmax_t;
typedef __PTRDIFF_TYPE__ ptrdiff_t;
typedef __SIZE_TYPE__ size_t;

/*++

Structure Description:

    This structure defines the properties associated with printing a format
    specifier.

Members:

    Radix - Stores the base to print the number in.

    FieldWidth - Stores the width of the field to print.

    IntegerSize - Stores the size of the integer being printed, in bytes.

    Precision - Stores the desired precision, in digits.

    AlwaysPrintSign - Stores a boolean indicating whether or not to always
        print a sign (normally positive values have their signs omitted).

    LeftJustified - Stores a boolean indicating if the value is left justified
        to its field (normally it's right justified).

    PrintUpperCase - Stores a boolean indicating whether letters used in
        numbers (like hex or the exponent letter) should be printed in upper
        case.

    PrintLeadingZeroes - Stores a boolean indicating if leading zeros should be
        printed to fill the field width.

    PrintRadix - Stores a boolean indicating if a radix (0x) should be printed.

    SpaceForPlus - Stores a boolean indicating if a space should be left in
        lieu of the plus sign for positive values.

    ThousandsGrouping - Stores a boolean indicating if thousands should be
        grouped together.

    Unsigned - Stores a boolean indicating if the integer is unsigned.

    FloatFormat - Stores a boolean indicating whether to use the float format
        in which all significant digits are printed. If both this and the
        scientific format booleans are false, an automatic selection will be
        made.

    ScientificFormat - Stores a boolean indicating whether to use the
        scientific floating poing format in which an exponent is printed. If
        both this and the scientific format booleans are false, an automatic
        selection will be made.

    SignificantDigitPrecision - Stores a boolean indicating if the precision
        represents the number of digits after the decimal point (FALSE) or the
        number of significant digits (TRUE).

--*/

typedef struct _PRINT_FORMAT_PROPERTIES {
    ULONG Radix;
    LONG FieldWidth;
    LONG IntegerSize;
    LONG Precision;
    BOOL AlwaysPrintSign;
    BOOL LeftJustified;
    BOOL PrintUpperCase;
    BOOL PrintLeadingZeroes;
    BOOL PrintRadix;
    BOOL SpaceForPlus;
    BOOL ThousandsGrouping;
    BOOL Unsigned;
    BOOL FloatFormat;
    BOOL ScientificFormat;
    BOOL SignificantDigitPrecision;
} PRINT_FORMAT_PROPERTIES, *PPRINT_FORMAT_PROPERTIES;

//
// -------------------------------------------------------------------- Globals
//

//
// Store some numeric constants used for string scanning.
//

extern double RtlFirst16NegativePowersOf10[16];
extern double RtlFirst16PowersOf10[16];
extern double RtlPositivePowersOf2[5];
extern double RtlNegativePowersOf2[5];

//
// -------------------------------------------------------- Function Prototypes
//

VOID
RtlpGetDoubleArgument (
    BOOL LongDouble,
    va_list *ArgumentList,
    PDOUBLE_PARTS DoubleParts
    );

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

BOOL
RtlpPrintDouble (
    PPRINT_FORMAT_CONTEXT Context,
    double Value,
    PPRINT_FORMAT_PROPERTIES Properties
    );

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

BOOL
RtlpPrintString (
    PPRINT_FORMAT_CONTEXT Context,
    PSTR String,
    LONG FieldWidth,
    LONG Precision,
    BOOL LeftJustified,
    BOOL Character
    );

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

ULONG
RtlpPrintInteger (
    PPRINT_FORMAT_CONTEXT Context,
    ULONGLONG Integer,
    PPRINT_FORMAT_PROPERTIES Properties
    );

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

BOOL
RtlpFormatWriteCharacter (
    PPRINT_FORMAT_CONTEXT Context,
    INT Character
    );

/*++

Routine Description:

    This routine writes a character to the print format destination.

Arguments:

    Context - Supplies a pointer to the print format context.

    Character - Supplies the character to write.

Return Value:

    TRUE if the character was written.

    FALSE on failure.

--*/

LONG
RtlpGetDoubleBase10Exponent (
    double Value,
    double *InversePowerOfTen
    );

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

