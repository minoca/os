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

