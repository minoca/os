/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    fptest.c

Abstract:

    This module tests the double-precision soft floating point support baked
    into the Rtl Library.

Author:

    Evan Green 25-Jul-2013

Environment:

    Test

--*/

//
// ------------------------------------------------------------------- Includes
//

#define RTL_API

#include <minoca/lib/types.h>
#include <minoca/lib/status.h>
#include <minoca/lib/rtl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _SOFT_FLOAT_DOUBLE_MATH_CASE {
    double Value1;
    double Value2;
    double Sum;
    double Difference;
    double Product;
    double Quotient;
    double Remainder;
    double SquareRoot;
    int Equal;
    int LessThanOrEqual;
    int LessThan;
    LONG Int32;
    LONGLONG Int64;
    ULONG Float;
} SOFT_FLOAT_DOUBLE_MATH_CASE, *PSOFT_FLOAT_DOUBLE_MATH_CASE;

typedef struct _SOFT_FLOAT_DOUBLE_CONVERT_CASE {
    ULONGLONG Integer;
    double FromInt32;
    double FromInt64;
} SOFT_FLOAT_DOUBLE_CONVERT_CASE, *PSOFT_FLOAT_DOUBLE_CONVERT_CASE;

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

SOFT_FLOAT_DOUBLE_MATH_CASE TestSoftFloatDoubleMathCases[] = {
    {0x0.0000000000000P+0,
     0x0.0000000000000P+0,
     0x0.0000000000000P+0,
     0x0.0000000000000P+0,
     0x0.0000000000000P+0,
     -NAN,
     -NAN,
     0x0.0000000000000P+0,
     1,
     1,
     0,
     0x0,
     0x0ULL,
     0x0},

    {-0x0.0000000000000P+0,
     0x0.0000000000000P+0,
     0x0.0000000000000P+0,
     -0x0.0000000000000P+0,
     -0x0.0000000000000P+0,
     -NAN,
     -NAN,
     -0x0.0000000000000P+0,
     1,
     1,
     0,
     0x0,
     0x0ULL,
     0x80000000},

    {0x0.0000000000000P+0,
     -0x0.0000000000000P+0,
     0x0.0000000000000P+0,
     0x0.0000000000000P+0,
     -0x0.0000000000000P+0,
     -NAN,
     -NAN,
     0x0.0000000000000P+0,
     1,
     1,
     0,
     0x0,
     0x0ULL,
     0x0},

    {-0x0.0000000000000P+0,
     -0x0.0000000000000P+0,
     -0x0.0000000000000P+0,
     0x0.0000000000000P+0,
     0x0.0000000000000P+0,
     -NAN,
     -NAN,
     -0x0.0000000000000P+0,
     1,
     1,
     0,
     0x0,
     0x0ULL,
     0x80000000},

    {0x0.0000000000000P+0,
     INFINITY,
     INFINITY,
     -INFINITY,
     -NAN,
     0x0.0000000000000P+0,
     0x0.0000000000000P+0,
     0x0.0000000000000P+0,
     0,
     1,
     1,
     0x0,
     0x0ULL,
     0x0},

    {-0x0.0000000000000P+0,
     INFINITY,
     INFINITY,
     -INFINITY,
     -NAN,
     -0x0.0000000000000P+0,
     -0x0.0000000000000P+0,
     -0x0.0000000000000P+0,
     0,
     1,
     1,
     0x0,
     0x0ULL,
     0x80000000},

    {INFINITY,
     0x0.0000000000000P+0,
     INFINITY,
     INFINITY,
     -NAN,
     INFINITY,
     -NAN,
     INFINITY,
     0,
     0,
     0,
     0x7FFFFFFF,
     0x7FFFFFFFFFFFFFFFULL,
     0x7F800000},

    {INFINITY,
     -0x0.0000000000000P+0,
     INFINITY,
     INFINITY,
     -NAN,
     -INFINITY,
     -NAN,
     INFINITY,
     0,
     0,
     0,
     0x7FFFFFFF,
     0x7FFFFFFFFFFFFFFFULL,
     0x7F800000},

    {0x0.0000000000000P+0,
     -INFINITY,
     -INFINITY,
     INFINITY,
     -NAN,
     -0x0.0000000000000P+0,
     0x0.0000000000000P+0,
     0x0.0000000000000P+0,
     0,
     0,
     0,
     0x0,
     0x0ULL,
     0x0},

    {-0x0.0000000000000P+0,
     -INFINITY,
     -INFINITY,
     INFINITY,
     -NAN,
     0x0.0000000000000P+0,
     -0x0.0000000000000P+0,
     -0x0.0000000000000P+0,
     0,
     0,
     0,
     0x0,
     0x0ULL,
     0x80000000},

    {-INFINITY,
     0x0.0000000000000P+0,
     -INFINITY,
     -INFINITY,
     -NAN,
     -INFINITY,
     -NAN,
     -NAN,
     0,
     1,
     1,
     0x80000000,
     0x8000000000000000ULL,
     0xFF800000},

    {-INFINITY,
     -0x0.0000000000000P+0,
     -INFINITY,
     -INFINITY,
     -NAN,
     INFINITY,
     -NAN,
     -NAN,
     0,
     1,
     1,
     0x80000000,
     0x8000000000000000ULL,
     0xFF800000},

    {0x0.0000000000000P+0,
     NAN,
     NAN,
     NAN,
     NAN,
     NAN,
     NAN,
     0x0.0000000000000P+0,
     0,
     0,
     0,
     0x0,
     0x0ULL,
     0x0},

    {-0x0.0000000000000P+0,
     NAN,
     NAN,
     NAN,
     NAN,
     NAN,
     NAN,
     -0x0.0000000000000P+0,
     0,
     0,
     0,
     0x0,
     0x0ULL,
     0x80000000},

    {NAN,
     0x0.0000000000000P+0,
     NAN,
     NAN,
     NAN,
     NAN,
     NAN,
     NAN,
     0,
     0,
     0,
     0x7FFFFFFF,
     0x7FFFFFFFFFFFFFFFULL,
     0x7FC00000},

    {NAN,
     -0x0.0000000000000P+0,
     NAN,
     NAN,
     NAN,
     NAN,
     NAN,
     NAN,
     0,
     0,
     0,
     0x7FFFFFFF,
     0x7FFFFFFFFFFFFFFFULL,
     0x7FC00000},

    {INFINITY,
     INFINITY,
     INFINITY,
     -NAN,
     INFINITY,
     -NAN,
     -NAN,
     INFINITY,
     1,
     1,
     0,
     0x7FFFFFFF,
     0x7FFFFFFFFFFFFFFFULL,
     0x7F800000},

    {INFINITY,
     -INFINITY,
     -NAN,
     INFINITY,
     -INFINITY,
     -NAN,
     -NAN,
     INFINITY,
     0,
     0,
     0,
     0x7FFFFFFF,
     0x7FFFFFFFFFFFFFFFULL,
     0x7F800000},

    {-INFINITY,
     INFINITY,
     -NAN,
     -INFINITY,
     -INFINITY,
     -NAN,
     -NAN,
     -NAN,
     0,
     1,
     1,
     0x80000000,
     0x8000000000000000ULL,
     0xFF800000},

    {-INFINITY,
     -INFINITY,
     -INFINITY,
     -NAN,
     INFINITY,
     -NAN,
     -NAN,
     -NAN,
     1,
     1,
     0,
     0x80000000,
     0x8000000000000000ULL,
     0xFF800000},

    {INFINITY,
     NAN,
     NAN,
     NAN,
     NAN,
     NAN,
     NAN,
     INFINITY,
     0,
     0,
     0,
     0x7FFFFFFF,
     0x7FFFFFFFFFFFFFFFULL,
     0x7F800000},

    {-INFINITY,
     NAN,
     NAN,
     NAN,
     NAN,
     NAN,
     NAN,
     -NAN,
     0,
     0,
     0,
     0x80000000,
     0x8000000000000000ULL,
     0xFF800000},

    {NAN,
     INFINITY,
     NAN,
     NAN,
     NAN,
     NAN,
     NAN,
     NAN,
     0,
     0,
     0,
     0x7FFFFFFF,
     0x7FFFFFFFFFFFFFFFULL,
     0x7FC00000},

    {NAN,
     -INFINITY,
     NAN,
     NAN,
     NAN,
     NAN,
     NAN,
     NAN,
     0,
     0,
     0,
     0x7FFFFFFF,
     0x7FFFFFFFFFFFFFFFULL,
     0x7FC00000},

    {INFINITY,
     NAN,
     NAN,
     NAN,
     NAN,
     NAN,
     NAN,
     INFINITY,
     0,
     0,
     0,
     0x7FFFFFFF,
     0x7FFFFFFFFFFFFFFFULL,
     0x7F800000},

    {-INFINITY,
     NAN,
     NAN,
     NAN,
     NAN,
     NAN,
     NAN,
     -NAN,
     0,
     0,
     0,
     0x80000000,
     0x8000000000000000ULL,
     0xFF800000},

    {NAN,
     NAN,
     NAN,
     NAN,
     NAN,
     NAN,
     NAN,
     NAN,
     0,
     0,
     0,
     0x7FFFFFFF,
     0x7FFFFFFFFFFFFFFFULL,
     0x7FC00000},

    {0x1.0000000000000P+0,
     0x0.0000000000000P+0,
     0x1.0000000000000P+0,
     0x1.0000000000000P+0,
     0x0.0000000000000P+0,
     INFINITY,
     -NAN,
     0x1.0000000000000P+0,
     0,
     0,
     0,
     0x1,
     0x1ULL,
     0x3F800000},

    {0x0.0000000000000P+0,
     -0x1.0000000000000P+0,
     -0x1.0000000000000P+0,
     0x1.0000000000000P+0,
     -0x0.0000000000000P+0,
     -0x0.0000000000000P+0,
     0x0.0000000000000P+0,
     0x0.0000000000000P+0,
     0,
     0,
     0,
     0x0,
     0x0ULL,
     0x0},

    {0x1.0000000000000P+0,
     -0x1.0000000000000P+0,
     0x0.0000000000000P+0,
     0x1.0000000000000P+1,
     -0x1.0000000000000P+0,
     -0x1.0000000000000P+0,
     0x0.0000000000000P+0,
     0x1.0000000000000P+0,
     0,
     0,
     0,
     0x1,
     0x1ULL,
     0x3F800000},

    {0x1.0000000000000P+0,
     0x1.0000000000000P+0,
     0x1.0000000000000P+1,
     0x0.0000000000000P+0,
     0x1.0000000000000P+0,
     0x1.0000000000000P+0,
     0x0.0000000000000P+0,
     0x1.0000000000000P+0,
     1,
     1,
     0,
     0x1,
     0x1ULL,
     0x3F800000},

    {0x1.0000000000000P+0,
     0x1.999999999999AP-4,
     0x1.199999999999AP+0,
     0x1.CCCCCCCCCCCCDP-1,
     0x1.999999999999AP-4,
     0x1.4000000000000P+3,
     -0x1.0000000000000P-54,
     0x1.0000000000000P+0,
     0,
     0,
     0,
     0x1,
     0x1ULL,
     0x3F800000},

    {-0x1.0000000000000P+0,
     -0x1.999999999999AP-4,
     -0x1.199999999999AP+0,
     -0x1.CCCCCCCCCCCCDP-1,
     0x1.999999999999AP-4,
     0x1.4000000000000P+3,
     0x1.0000000000000P-54,
     -NAN,
     0,
     1,
     1,
     0xFFFFFFFF,
     0xFFFFFFFFFFFFFFFFULL,
     0xBF800000},

    {0x1.6374BC6A7EF9EP+1,
     0x1.C70A3D70A3D71P+1,
     0x1.953F7CED91688P+2,
     -0x1.8E5604189374CP-1,
     0x1.3BE9595FEDA67P+3,
     0x1.8FF3537606C4EP-1,
     -0x1.8E5604189374CP-1,
     0x1.AA9B5FB578508P+0,
     0,
     1,
     1,
     0x3,
     0x3ULL,
     0x4031BA5E},

    {0x1.658E3AB795204P+830,
     0x1.3BB71C6153DA8P-829,
     0x1.658E3AB795204P+830,
     0x1.658E3AB795204P+830,
     0x1.B8F5C28F5C28EP+1,
     INFINITY,
     0x1.06F976DF15960P-831,
     0x1.2E8BD69AA19CCP+415,
     0,
     0,
     0,
     0x7FFFFFFF,
     0x7FFFFFFFFFFFFFFFULL,
     0x7F800000},

    {0x1.180BADD74D1B4P+109,
     0x1.199999999999AP+0,
     0x1.180BADD74D1B4P+109,
     0x1.180BADD74D1B4P+109,
     0x1.340CD8D33B37AP+109,
     0x1.FD2C81E48C318P+108,
     -0x1.016AEAB94B870P-3,
     0x1.7AA8F28489A7AP+54,
     0,
     0,
     0,
     0x7FFFFFFF,
     0x7FFFFFFFFFFFFFFFULL,
     0x760C05D7},

    {0x1.2000000000000P+3,
     0x1.9000000000000P+4,
     0x1.1000000000000P+5,
     -0x1.0000000000000P+4,
     0x1.C200000000000P+7,
     0x1.70A3D70A3D70AP-2,
     0x1.2000000000000P+3,
     0x1.8000000000000P+1,
     0,
     1,
     1,
     0x9,
     0x9ULL,
     0x41100000},

    {-0x1.0000000000000P+4,
     0x1.999999999999AP-4,
     -0x1.FCCCCCCCCCCCDP+3,
     -0x1.019999999999AP+4,
     -0x1.999999999999AP+0,
     -0x1.4000000000000P+7,
     0x1.0000000000000P-50,
     -NAN,
     0,
     1,
     1,
     0xFFFFFFF0,
     0xFFFFFFFFFFFFFFF0ULL,
     0xC1800000},

    {0x1.AD7F29ABCAF48P-24,
     -0x1.3333333333333P-2,
     -0x1.33332C7D368C8P-2,
     0x1.333339E92FD9EP-2,
     -0x1.01B2B29A4692BP-25,
     -0x1.65E9F80F29212P-22,
     0x1.AD7F29ABCAF48P-24,
     0x1.4B96BE9C2DA2CP-12,
     0,
     0,
     0,
     0x0,
     0x0ULL,
     0x33D6BF95},

    {0x1.9000000000000P+4,
     0x1.4000000000000P+2,
     0x1.E000000000000P+4,
     0x1.4000000000000P+4,
     0x1.F400000000000P+6,
     0x1.4000000000000P+2,
     0x0.0000000000000P+0,
     0x1.4000000000000P+2,
     0,
     0,
     0,
     0x19,
     0x19ULL,
     0x41C80000},

    {0x1.5555555551AB1P+0,
     0x1.3C1C0E493105EP-29,
     0x1.5555555F328B8P+0,
     0x1.5555554B70CAAP+0,
     0x1.A57ABDB6E7814P-29,
     0x1.146D660768A04P+29,
     -0x1.75D581A47EDE0P-33,
     0x1.279A7459019B8P+0,
     0,
     0,
     0,
     0x1,
     0x1ULL,
     0x3FAAAAAB},

    {0x1.0000000000000P+32,
     0x1.0000000000000P+15,
     0x1.0000800000000P+32,
     0x1.FFFF000000000P+31,
     0x1.0000000000000P+47,
     0x1.0000000000000P+17,
     0x0.0000000000000P+0,
     0x1.0000000000000P+16,
     0,
     0,
     0,
     0x7FFFFFFF,
     0x100000000ULL,
     0x4F800000},

    {0x1.0000000000000P+15,
     0x1.FC00000000000P+7,
     0x1.01FC000000000P+15,
     0x1.FC08000000000P+14,
     0x1.FC00000000000P+22,
     0x1.0204081020408P+7,
     0x1.0000000000000P+1,
     0x1.6A09E667F3BCDP+7,
     0,
     0,
     0,
     0x8000,
     0x8000ULL,
     0x47000000},

    {-0x1.C000000000000P+2,
     -0x1.C000000000000P+2,
     -0x1.C000000000000P+3,
     0x0.0000000000000P+0,
     0x1.8800000000000P+5,
     0x1.0000000000000P+0,
     -0x0.0000000000000P+0,
     -NAN,
     1,
     1,
     0,
     0xFFFFFFF9,
     0xFFFFFFFFFFFFFFF9ULL,
     0xC0E00000},

    {0x1.028F5C28F5C29P+0,
     -0x1.3880000000000P+15,
     -0x1.387DFAE147AE1P+15,
     0x1.3882051EB851FP+15,
     -0x1.3BA0000000000P+15,
     -0x1.A79FEC99F1AE3P-16,
     0x1.028F5C28F5C29P+0,
     0x1.0146DD68287F3P+0,
     0,
     0,
     0,
     0x1,
     0x1ULL,
     0x3F8147AE},

    {0x1.0000000000000P+0,
     -0x1.0000000000000P-1,
     0x1.0000000000000P-1,
     0x1.8000000000000P+0,
     -0x1.0000000000000P-1,
     -0x1.0000000000000P+1,
     0x0.0000000000000P+0,
     0x1.0000000000000P+0,
     0,
     0,
     0,
     0x1,
     0x1ULL,
     0x3F800000},

    {0x1.199999999999AP+0,
     0x1.199999999999AP+0,
     0x1.199999999999AP+1,
     0x0.0000000000000P+0,
     0x1.35C28F5C28F5DP+0,
     0x1.0000000000000P+0,
     0x0.0000000000000P+0,
     0x1.0C7EBC96A56F6P+0,
     1,
     1,
     0,
     0x1,
     0x1ULL,
     0x3F8CCCCD},

    {0x0.0000000000000P+0,
     -0x1.8000000000000P+2,
     -0x1.8000000000000P+2,
     0x1.8000000000000P+2,
     -0x0.0000000000000P+0,
     -0x0.0000000000000P+0,
     0x0.0000000000000P+0,
     0x0.0000000000000P+0,
     0,
     0,
     0,
     0x0,
     0x0ULL,
     0x0},

    {0x1.2A05F20000000P+33,
     0x1.74876E8000000P+36,
     0x1.99C82CC000000P+36,
     -0x1.4F46B04000000P+36,
     0x1.B1AE4D6E2EF50P+69,
     0x1.999999999999AP-4,
     0x1.2A05F20000000P+33,
     0x1.86A0000000000P+16,
     0,
     1,
     1,
     0x7FFFFFFF,
     0x2540BE400ULL,
     0x501502F9},

    {0x1.74876E8000000P+36,
     0x1.2A05F20000000P+33,
     0x1.99C82CC000000P+36,
     0x1.4F46B04000000P+36,
     0x1.B1AE4D6E2EF50P+69,
     0x1.4000000000000P+3,
     0x0.0000000000000P+0,
     0x1.34D0F1066B7CCP+18,
     0,
     0,
     0,
     0x7FFFFFFF,
     0x174876E800ULL,
     0x51BA43B7},

    {-0x1.2A05F20000000P+33,
     0x1.74876E8000000P+36,
     0x1.4F46B04000000P+36,
     -0x1.99C82CC000000P+36,
     -0x1.B1AE4D6E2EF50P+69,
     -0x1.999999999999AP-4,
     -0x1.2A05F20000000P+33,
     -NAN,
     0,
     1,
     1,
     0x80000000,
     0xFFFFFFFDABF41C00ULL,
     0xD01502F9},

    {-0x1.74876E8000000P+36,
     0x1.2A05F20000000P+33,
     -0x1.4F46B04000000P+36,
     -0x1.99C82CC000000P+36,
     -0x1.B1AE4D6E2EF50P+69,
     -0x1.4000000000000P+3,
     -0x0.0000000000000P+0,
     -NAN,
     0,
     1,
     1,
     0x80000000,
     0xFFFFFFE8B7891800ULL,
     0xD1BA43B7},

    {0x1.2A05F20000000P+33,
     -0x1.74876E8000000P+36,
     -0x1.4F46B04000000P+36,
     0x1.99C82CC000000P+36,
     -0x1.B1AE4D6E2EF50P+69,
     -0x1.999999999999AP-4,
     0x1.2A05F20000000P+33,
     0x1.86A0000000000P+16,
     0,
     0,
     0,
     0x7FFFFFFF,
     0x2540BE400ULL,
     0x501502F9},

    {0x1.74876E8000000P+36,
     -0x1.2A05F20000000P+33,
     0x1.4F46B04000000P+36,
     0x1.99C82CC000000P+36,
     -0x1.B1AE4D6E2EF50P+69,
     -0x1.4000000000000P+3,
     0x0.0000000000000P+0,
     0x1.34D0F1066B7CCP+18,
     0,
     0,
     0,
     0x7FFFFFFF,
     0x174876E800ULL,
     0x51BA43B7},

    {-0x1.2A05F20000000P+33,
     -0x1.74876E8000000P+36,
     -0x1.99C82CC000000P+36,
     0x1.4F46B04000000P+36,
     0x1.B1AE4D6E2EF50P+69,
     0x1.999999999999AP-4,
     -0x1.2A05F20000000P+33,
     -NAN,
     0,
     0,
     0,
     0x80000000,
     0xFFFFFFFDABF41C00ULL,
     0xD01502F9},

    {-0x1.74876E8000000P+36,
     -0x1.2A05F20000000P+33,
     -0x1.99C82CC000000P+36,
     -0x1.4F46B04000000P+36,
     0x1.B1AE4D6E2EF50P+69,
     0x1.4000000000000P+3,
     -0x0.0000000000000P+0,
     -NAN,
     0,
     1,
     1,
     0x80000000,
     0xFFFFFFE8B7891800ULL,
     0xD1BA43B7},
};

SOFT_FLOAT_DOUBLE_CONVERT_CASE TestSoftFloatDoubleFromIntegers[] = {
    {0x0ULL, 0x0.0000000000000P+0, 0x0.0000000000000P+0},
    {0x1ULL, 0x1.0000000000000P+0, 0x1.0000000000000P+0},
    {0xFFFFFFFFFFFFFFFFULL, -0x1.0000000000000P+0, -0x1.0000000000000P+0},
    {0x5ULL, 0x1.4000000000000P+2, 0x1.4000000000000P+2},
    {0xAULL, 0x1.4000000000000P+3, 0x1.4000000000000P+3},
    {0x64ULL, 0x1.9000000000000P+6, 0x1.9000000000000P+6},
    {0x29AULL, 0x1.4D00000000000P+9, 0x1.4D00000000000P+9},
    {0xFFFFULL, 0x1.FFFE000000000P+15, 0x1.FFFE000000000P+15},
    {0xFFFFFULL, 0x1.FFFFE00000000P+19, 0x1.FFFFE00000000P+19},
    {0x123456ULL, 0x1.2345600000000P+20, 0x1.2345600000000P+20},
    {0x87654321ULL, -0x1.E26AF37C00000P+30, 0x1.0ECA864200000P+31},
    {0x77654321ULL, 0x1.DD950C8400000P+30, 0x1.DD950C8400000P+30},
    {0xCCCCCCCCULL, -0x1.999999A000000P+29, 0x1.9999999800000P+31},
    {0xFFFFFFFFULL, -0x1.0000000000000P+0, 0x1.FFFFFFFE00000P+31},
    {0x100000000ULL, 0x0.0000000000000P+0, 0x1.0000000000000P+32},
    {0x100000001ULL, 0x1.0000000000000P+0, 0x1.0000000100000P+32},
    {0xFFFFFFFFFULL, -0x1.0000000000000P+0, 0x1.FFFFFFFFE0000P+35},
    {0x765432112345678ULL, 0x1.2345678000000P+28, 0x1.D950C8448D15AP+58},
    {0x7FFFFFFFFFFFFFFFULL, -0x1.0000000000000P+0, 0x1.0000000000000P+63},
    {0x8000000000000000ULL, 0x0.0000000000000P+0, -0x1.0000000000000P+63},
    {0x8000000000000001ULL, 0x1.0000000000000P+0, -0x1.0000000000000P+63},
    {0xCCCCCCCCCCCCCCCCULL, -0x1.999999A000000P+29, -0x1.999999999999AP+61},
    {0xFFFFFFFFFFFFFFFEULL, -0x1.0000000000000P+1, -0x1.0000000000000P+1},
    {0xFFFFFFFFFFFFFFFFULL, -0x1.0000000000000P+0, -0x1.0000000000000P+0},
};

//
// ------------------------------------------------------------------ Functions
//

ULONG
TestSoftFloatDouble (
    VOID
    )

/*++

Routine Description:

    This routine tests the soft float implementation in the runtime library.

Arguments:

    None.

Return Value:

    Returns the number of test failures.

--*/

{

    DOUBLE_PARTS Answer;
    PSOFT_FLOAT_DOUBLE_CONVERT_CASE ConvertTest;
    ULONG Failures;
    FLOAT_PARTS Float;
    PSOFT_FLOAT_DOUBLE_MATH_CASE MathTest;
    DOUBLE_PARTS Result;
    ULONG TestCount;
    ULONG TestIndex;

    Failures = 0;
    TestCount = sizeof(TestSoftFloatDoubleMathCases) /
                sizeof(TestSoftFloatDoubleMathCases[0]);

    for (TestIndex = 0; TestIndex < TestCount; TestIndex += 1) {
        MathTest = &(TestSoftFloatDoubleMathCases[TestIndex]);

        //
        // Test the math: add, subtract, multiply, divide, modulo, and
        // square root.
        //

        Answer.Double = MathTest->Sum;
        Result.Double = RtlDoubleAdd(MathTest->Value1, MathTest->Value2);
        if (Answer.Ulonglong != Result.Ulonglong) {
            printf("SoftFloat Add %0.13a %0.13a was %0.13a, should have "
                   "been %0.13a.\n",
                   MathTest->Value1,
                   MathTest->Value2,
                   Result.Double,
                   Answer.Double);

            Failures += 1;
        }

        Answer.Double = MathTest->Difference;
        Result.Double = RtlDoubleSubtract(MathTest->Value1, MathTest->Value2);
        if (Answer.Ulonglong != Result.Ulonglong) {
            printf("SoftFloat Subtract %0.13a %0.13a was %0.13a, should have "
                   "been %0.13a.\n",
                   MathTest->Value1,
                   MathTest->Value2,
                   Result.Double,
                   Answer.Double);

            Failures += 1;
        }

        Answer.Double = MathTest->Product;
        Result.Double = RtlDoubleMultiply(MathTest->Value1, MathTest->Value2);
        if (Answer.Ulonglong != Result.Ulonglong) {
            printf("SoftFloat Multiply %0.13a %0.13a was %0.13a, should have "
                   "been %0.13a.\n",
                   MathTest->Value1,
                   MathTest->Value2,
                   Result.Double,
                   Answer.Double);

            Failures += 1;
        }

        Answer.Double = MathTest->Quotient;
        Result.Double = RtlDoubleDivide(MathTest->Value1, MathTest->Value2);
        if (Answer.Ulonglong != Result.Ulonglong) {
            printf("SoftFloat Divide %0.13a %0.13a was %0.13a, should have "
                   "been %0.13a.\n",
                   MathTest->Value1,
                   MathTest->Value2,
                   Result.Double,
                   Answer.Double);

            Failures += 1;
        }

        Answer.Double = MathTest->Remainder;
        Result.Double = RtlDoubleModulo(MathTest->Value1, MathTest->Value2);
        if (Answer.Ulonglong != Result.Ulonglong) {
            printf("SoftFloat Remainder %0.13a %0.13a was %0.13a, should have "
                   "been %0.13a.\n",
                   MathTest->Value1,
                   MathTest->Value2,
                   Result.Double,
                   Answer.Double);

            Failures += 1;
        }

        Answer.Double = MathTest->SquareRoot;
        Result.Double = RtlDoubleSquareRoot(MathTest->Value1);
        if (Answer.Ulonglong != Result.Ulonglong) {
            printf("SoftFloat Square Root %0.13a was %0.13a, should have "
                   "been %0.13a.\n",
                   MathTest->Value1,
                   Result.Double,
                   Answer.Double);

            Failures += 1;
        }

        //
        // Test comparison, equal, equal (signaling), less than or equal to,
        // less than or equal to (quiet), less than, and less than (quiet).
        //

        Answer.Ulonglong = MathTest->Equal;
        Result.Ulonglong = RtlDoubleIsEqual(MathTest->Value1, MathTest->Value2);
        if (Answer.Ulonglong != Result.Ulonglong) {
            printf("SoftFloat Equal %0.13a %0.13a was %d, should have "
                   "been %d.\n",
                   MathTest->Value1,
                   MathTest->Value2,
                   (ULONG)Result.Ulonglong,
                   (ULONG)Answer.Ulonglong);

            Failures += 1;
        }

        Answer.Ulonglong = MathTest->Equal;
        Result.Ulonglong = RtlDoubleSignalingIsEqual(MathTest->Value1,
                                                    MathTest->Value2);

        if (Answer.Ulonglong != Result.Ulonglong) {
            printf("SoftFloat Equal (signaling) %0.13a %0.13a was %d, should "
                   "have been %d.\n",
                   MathTest->Value1,
                   MathTest->Value2,
                   (ULONG)Result.Ulonglong,
                   (ULONG)Answer.Ulonglong);

            Failures += 1;
        }

        Answer.Ulonglong = MathTest->LessThanOrEqual;
        Result.Ulonglong = RtlDoubleIsLessThanOrEqual(MathTest->Value1,
                                                      MathTest->Value2);

        if (Answer.Ulonglong != Result.Ulonglong) {
            printf("SoftFloat Less/equal %0.13a %0.13a was %d, should "
                   "have been %d.\n",
                   MathTest->Value1,
                   MathTest->Value2,
                   (ULONG)Result.Ulonglong,
                   (ULONG)Answer.Ulonglong);

            Failures += 1;
        }

        Answer.Ulonglong = MathTest->LessThanOrEqual;
        Result.Ulonglong = RtlDoubleIsLessThanOrEqualQuiet(MathTest->Value1,
                                                           MathTest->Value2);

        if (Answer.Ulonglong != Result.Ulonglong) {
            printf("SoftFloat Less/equal (quiet) %0.13a %0.13a was %d, should "
                   "have been %d.\n",
                   MathTest->Value1,
                   MathTest->Value2,
                   (ULONG)Result.Ulonglong,
                   (ULONG)Answer.Ulonglong);

            Failures += 1;
        }

        Answer.Ulonglong = MathTest->LessThan;
        Result.Ulonglong = RtlDoubleIsLessThan(MathTest->Value1,
                                               MathTest->Value2);

        if (Answer.Ulonglong != Result.Ulonglong) {
            printf("SoftFloat Less than %0.13a %0.13a was %d, should "
                   "have been %d.\n",
                   MathTest->Value1,
                   MathTest->Value2,
                   (ULONG)Result.Ulonglong,
                   (ULONG)Answer.Ulonglong);

            Failures += 1;
        }

        Answer.Ulonglong = MathTest->LessThan;
        Result.Ulonglong = RtlDoubleIsLessThanQuiet(MathTest->Value1,
                                                    MathTest->Value2);

        if (Answer.Ulonglong != Result.Ulonglong) {
            printf("SoftFloat Less than (quiet) %0.13a %0.13a was %d, should "
                   "have been %d.\n",
                   MathTest->Value1,
                   MathTest->Value2,
                   (ULONG)Result.Ulonglong,
                   (ULONG)Answer.Ulonglong);

            Failures += 1;
        }

        //
        // Test the conversion of the double to an integer.
        //

        Answer.Ulonglong = MathTest->Int32;
        Result.Ulonglong = RtlDoubleConvertToInteger32(MathTest->Value1);
        if (Answer.Ulonglong != Result.Ulonglong) {
            printf("SoftFloat ConvertToInt32 %0.13a was 0x%x, should "
                   "have been 0x%x.\n",
                   MathTest->Value1,
                   (ULONG)Result.Ulonglong,
                   (ULONG)Answer.Ulonglong);

            Failures += 1;
        }

        Answer.Ulonglong = MathTest->Int64;
        Result.Ulonglong = RtlDoubleConvertToInteger64(MathTest->Value1);
        if (Answer.Ulonglong != Result.Ulonglong) {
            printf("SoftFloat ConvertToInt64 %0.13a was 0x%llx, should "
                   "have been 0x%llx.\n",
                   MathTest->Value1,
                   Result.Ulonglong,
                   Answer.Ulonglong);

            Failures += 1;
        }

        Answer.Ulonglong = MathTest->Float;
        Float.Float = RtlDoubleConvertToFloat(MathTest->Value1);
        if (Answer.Ulonglong != Float.Ulong) {
            printf("SoftFloat ConvertDoubleToFloat %0.13a was 0x%x, should "
                   "have been 0x%llx.\n",
                   MathTest->Value1,
                   Float.Ulong,
                   Answer.Ulonglong);

            Failures += 1;
        }
    }

    MathTest = NULL;

    //
    // Also test the conversion of integers to doubles.
    //

    TestCount = sizeof(TestSoftFloatDoubleFromIntegers) /
                sizeof(TestSoftFloatDoubleFromIntegers[0]);

    for (TestIndex = 0; TestIndex < TestCount; TestIndex += 1) {
        ConvertTest = &(TestSoftFloatDoubleFromIntegers[TestIndex]);
        Answer.Double = ConvertTest->FromInt32;
        Result.Double =
                     RtlDoubleConvertFromInteger32((LONG)ConvertTest->Integer);

        if (Answer.Ulonglong != Result.Ulonglong) {
            printf("SoftFloat ConvertFromInt32 0x%x was %.13a, should "
                   "have been %.13a.\n",
                   (LONG)ConvertTest->Integer,
                   Result.Double,
                   Answer.Double);

            Failures += 1;
        }

        Answer.Double = ConvertTest->FromInt64;
        Result.Double = RtlDoubleConvertFromInteger64(ConvertTest->Integer);
        if (Answer.Ulonglong != Result.Ulonglong) {
            printf("SoftFloat ConvertFromInt64 0x%llx was %.13a, should "
                   "have been %.13a.\n",
                   ConvertTest->Integer,
                   Result.Double,
                   Answer.Double);

            Failures += 1;
        }
    }

    if (Failures != 0) {
        printf("\n\n%d Soft Float double-precision test failures.\n\n",
               Failures);
    }

    return Failures;
}

//
// --------------------------------------------------------- Internal Functions
//

