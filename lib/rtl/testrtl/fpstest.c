/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    fpstest.c

Abstract:

    This module tests the single-precision soft floating point support baked
    into the Rtl Library.

Author:

    Chris Stevens 11-Jan-2017

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

typedef struct _SOFT_FLOAT_SINGLE_MATH_CASE {
    float Value1;
    float Value2;
    float Sum;
    float Difference;
    float Product;
    float Quotient;
    float Remainder;
    float SquareRoot;
    int Equal;
    int LessThanOrEqual;
    int LessThan;
    LONG Int32;
    LONGLONG Int64;
    ULONGLONG Double;
} SOFT_FLOAT_SINGLE_MATH_CASE, *PSOFT_FLOAT_SINGLE_MATH_CASE;

typedef struct _SOFT_FLOAT_SINGLE_CONVERT_CASE {
    ULONGLONG Integer;
    float FromInt32;
    float FromInt64;
} SOFT_FLOAT_SINGLE_CONVERT_CASE, *PSOFT_FLOAT_SINGLE_CONVERT_CASE;

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

SOFT_FLOAT_SINGLE_MATH_CASE TestSoftFloatSingleMathCases[] = {
    {0x0.000000p+0,
     0x0.000000p+0,
     0x0.000000p+0,
     0x0.000000p+0,
     0x0.000000p+0,
     -NAN,
     -NAN,
     0x0.000000p+0,
     1,
     1,
     0,
     0x0,
     0x0ULL,
     0x0ULL},

    {-0x0.000000p+0,
     0x0.000000p+0,
     0x0.000000p+0,
     -0x0.000000p+0,
     -0x0.000000p+0,
     -NAN,
     -NAN,
     -0x0.000000p+0,
     1,
     1,
     0,
     0x0,
     0x0ULL,
     0x8000000000000000ULL},

    {0x0.000000p+0,
     -0x0.000000p+0,
     0x0.000000p+0,
     0x0.000000p+0,
     -0x0.000000p+0,
     -NAN,
     -NAN,
     0x0.000000p+0,
     1,
     1,
     0,
     0x0,
     0x0ULL,
     0x0ULL},

    {-0x0.000000p+0,
     -0x0.000000p+0,
     -0x0.000000p+0,
     0x0.000000p+0,
     0x0.000000p+0,
     -NAN,
     -NAN,
     -0x0.000000p+0,
     1,
     1,
     0,
     0x0,
     0x0ULL,
     0x8000000000000000ULL},

    {0x0.000000p+0,
     INFINITY,
     INFINITY,
     -INFINITY,
     -NAN,
     0x0.000000p+0,
     0x0.000000p+0,
     0x0.000000p+0,
     0,
     1,
     1,
     0x0,
     0x0ULL,
     0x0ULL},

    {-0x0.000000p+0,
     INFINITY,
     INFINITY,
     -INFINITY,
     -NAN,
     -0x0.000000p+0,
     -0x0.000000p+0,
     -0x0.000000p+0,
     0,
     1,
     1,
     0x0,
     0x0ULL,
     0x8000000000000000ULL},

    {INFINITY,
     0x0.000000p+0,
     INFINITY,
     INFINITY,
     -NAN,
     INFINITY,
     -NAN,
     INFINITY,
     0,
     0,
     0,
     0x7fffffff,
     0x7fffffffffffffffULL,
     0x7ff0000000000000ULL},

    {INFINITY,
     -0x0.000000p+0,
     INFINITY,
     INFINITY,
     -NAN,
     -INFINITY,
     -NAN,
     INFINITY,
     0,
     0,
     0,
     0x7fffffff,
     0x7fffffffffffffffULL,
     0x7ff0000000000000ULL},

    {0x0.000000p+0,
     -INFINITY,
     -INFINITY,
     INFINITY,
     -NAN,
     -0x0.000000p+0,
     0x0.000000p+0,
     0x0.000000p+0,
     0,
     0,
     0,
     0x0,
     0x0ULL,
     0x0ULL},

    {-0x0.000000p+0,
     -INFINITY,
     -INFINITY,
     INFINITY,
     -NAN,
     0x0.000000p+0,
     -0x0.000000p+0,
     -0x0.000000p+0,
     0,
     0,
     0,
     0x0,
     0x0ULL,
     0x8000000000000000ULL},

    {-INFINITY,
     0x0.000000p+0,
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
     0xfff0000000000000ULL},

    {-INFINITY,
     -0x0.000000p+0,
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
     0xfff0000000000000ULL},

    {0x0.000000p+0,
     NAN,
     NAN,
     NAN,
     NAN,
     NAN,
     NAN,
     0x0.000000p+0,
     0,
     0,
     0,
     0x0,
     0x0ULL,
     0x0ULL},

    {-0x0.000000p+0,
     NAN,
     NAN,
     NAN,
     NAN,
     NAN,
     NAN,
     -0x0.000000p+0,
     0,
     0,
     0,
     0x0,
     0x0ULL,
     0x8000000000000000ULL},

    {NAN,
     0x0.000000p+0,
     NAN,
     NAN,
     NAN,
     NAN,
     NAN,
     NAN,
     0,
     0,
     0,
     0x7fffffff,
     0x7fffffffffffffffULL,
     0x7ff8000000000000ULL},

    {NAN,
     -0x0.000000p+0,
     NAN,
     NAN,
     NAN,
     NAN,
     NAN,
     NAN,
     0,
     0,
     0,
     0x7fffffff,
     0x7fffffffffffffffULL,
     0x7ff8000000000000ULL},

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
     0x7fffffff,
     0x7fffffffffffffffULL,
     0x7ff0000000000000ULL},

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
     0x7fffffff,
     0x7fffffffffffffffULL,
     0x7ff0000000000000ULL},

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
     0xfff0000000000000ULL},

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
     0xfff0000000000000ULL},

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
     0x7fffffff,
     0x7fffffffffffffffULL,
     0x7ff0000000000000ULL},

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
     0xfff0000000000000ULL},

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
     0x7fffffff,
     0x7fffffffffffffffULL,
     0x7ff8000000000000ULL},

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
     0x7fffffff,
     0x7fffffffffffffffULL,
     0x7ff8000000000000ULL},

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
     0x7fffffff,
     0x7fffffffffffffffULL,
     0x7ff0000000000000ULL},

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
     0xfff0000000000000ULL},

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
     0x7fffffff,
     0x7fffffffffffffffULL,
     0x7ff8000000000000ULL},

    {0x1.000000p+0,
     0x0.000000p+0,
     0x1.000000p+0,
     0x1.000000p+0,
     0x0.000000p+0,
     INFINITY,
     -NAN,
     0x1.000000p+0,
     0,
     0,
     0,
     0x1,
     0x1ULL,
     0x3ff0000000000000ULL},

    {0x0.000000p+0,
     -0x1.000000p+0,
     -0x1.000000p+0,
     0x1.000000p+0,
     -0x0.000000p+0,
     -0x0.000000p+0,
     0x0.000000p+0,
     0x0.000000p+0,
     0,
     0,
     0,
     0x0,
     0x0ULL,
     0x0ULL},

    {0x1.000000p+0,
     -0x1.000000p+0,
     0x0.000000p+0,
     0x1.000000p+1,
     -0x1.000000p+0,
     -0x1.000000p+0,
     0x0.000000p+0,
     0x1.000000p+0,
     0,
     0,
     0,
     0x1,
     0x1ULL,
     0x3ff0000000000000ULL},

    {0x1.000000p+0,
     0x1.000000p+0,
     0x1.000000p+1,
     0x0.000000p+0,
     0x1.000000p+0,
     0x1.000000p+0,
     0x0.000000p+0,
     0x1.000000p+0,
     1,
     1,
     0,
     0x1,
     0x1ULL,
     0x3ff0000000000000ULL},

    {0x1.000000p+0,
     0x1.99999ap-4,
     0x1.19999ap+0,
     0x1.ccccccp-1,
     0x1.99999ap-4,
     0x1.400000p+3,
     -0x1.000000p-26,
     0x1.000000p+0,
     0,
     0,
     0,
     0x1,
     0x1ULL,
     0x3ff0000000000000ULL},

    {-0x1.000000p+0,
     -0x1.99999ap-4,
     -0x1.19999ap+0,
     -0x1.ccccccp-1,
     0x1.99999ap-4,
     0x1.400000p+3,
     0x1.000000p-26,
     -NAN,
     0,
     1,
     1,
     0xffffffff,
     0xffffffffffffffffULL,
     0xbff0000000000000ULL},

    {0x1.6374bcp+1,
     0x1.c70a3ep+1,
     0x1.953f7cp+2,
     -0x1.8e5608p-1,
     0x1.3be95ap+3,
     0x1.8ff352p-1,
     -0x1.8e5608p-1,
     0x1.aa9b60p+0,
     0,
     1,
     1,
     0x3,
     0x3ULL,
     0x4006374bc0000000ULL},

    {INFINITY,
     0x0.000000p+0,
     INFINITY,
     INFINITY,
     -NAN,
     INFINITY,
     -NAN,
     INFINITY,
     0,
     0,
     0,
     0x7fffffff,
     0x7fffffffffffffffULL,
     0x7ff0000000000000ULL},

    {0x1.180baep+109,
     0x1.19999ap+0,
     0x1.180baep+109,
     0x1.180baep+109,
     0x1.340cdap+109,
     0x1.fd2c82p+108,
     -0x1.4080b0p-2,
     0x1.7aa8f2p+54,
     0,
     0,
     0,
     0x7fffffff,
     0x7fffffffffffffffULL,
     0x46c180bae0000000ULL},

    {0x1.200000p+3,
     0x1.900000p+4,
     0x1.100000p+5,
     -0x1.000000p+4,
     0x1.c20000p+7,
     0x1.70a3d8p-2,
     0x1.200000p+3,
     0x1.800000p+1,
     0,
     1,
     1,
     0x9,
     0x9ULL,
     0x4022000000000000ULL},

    {-0x1.000000p+4,
     0x1.99999ap-4,
     -0x1.fcccccp+3,
     -0x1.01999ap+4,
     -0x1.99999ap+0,
     -0x1.400000p+7,
     0x1.000000p-22,
     -NAN,
     0,
     1,
     1,
     0xfffffff0,
     0xfffffffffffffff0ULL,
     0xc030000000000000ULL},

    {0x1.ad7f2ap-24,
     -0x1.333334p-2,
     -0x1.33332ep-2,
     0x1.33333ap-2,
     -0x1.01b2b4p-25,
     -0x1.65e9f8p-22,
     0x1.ad7f2ap-24,
     0x1.4b96bep-12,
     0,
     0,
     0,
     0x0,
     0x0ULL,
     0x3e7ad7f2a0000000ULL},

    {0x1.900000p+4,
     0x1.400000p+2,
     0x1.e00000p+4,
     0x1.400000p+4,
     0x1.f40000p+6,
     0x1.400000p+2,
     0x0.000000p+0,
     0x1.400000p+2,
     0,
     0,
     0,
     0x19,
     0x19ULL,
     0x4039000000000000ULL},

    {0x1.555556p+0,
     0x1.3c1c0ep-29,
     0x1.555556p+0,
     0x1.555556p+0,
     0x1.a57abep-29,
     0x1.146d66p+29,
     0x1.02d050p-31,
     0x1.279a74p+0,
     0,
     0,
     0,
     0x1,
     0x1ULL,
     0x3ff5555560000000ULL},

    {0x1.000000p+32,
     0x1.000000p+15,
     0x1.000080p+32,
     0x1.ffff00p+31,
     0x1.000000p+47,
     0x1.000000p+17,
     0x0.000000p+0,
     0x1.000000p+16,
     0,
     0,
     0,
     0x7fffffff,
     0x100000000ULL,
     0x41f0000000000000ULL},

    {0x1.000000p+15,
     0x1.fc0000p+7,
     0x1.01fc00p+15,
     0x1.fc0800p+14,
     0x1.fc0000p+22,
     0x1.020408p+7,
     0x1.000000p+1,
     0x1.6a09e6p+7,
     0,
     0,
     0,
     0x8000,
     0x8000ULL,
     0x40e0000000000000ULL},

    {-0x1.c00000p+2,
     -0x1.c00000p+2,
     -0x1.c00000p+3,
     0x0.000000p+0,
     0x1.880000p+5,
     0x1.000000p+0,
     -0x0.000000p+0,
     -NAN,
     1,
     1,
     0,
     0xfffffff9,
     0xfffffffffffffff9ULL,
     0xc01c000000000000ULL},

    {0x1.028f5cp+0,
     -0x1.388000p+15,
     -0x1.387dfap+15,
     0x1.388206p+15,
     -0x1.3ba000p+15,
     -0x1.a79fecp-16,
     0x1.028f5cp+0,
     0x1.0146dep+0,
     0,
     0,
     0,
     0x1,
     0x1ULL,
     0x3ff028f5c0000000ULL},

    {0x1.000000p+0,
     -0x1.000000p-1,
     0x1.000000p-1,
     0x1.800000p+0,
     -0x1.000000p-1,
     -0x1.000000p+1,
     0x0.000000p+0,
     0x1.000000p+0,
     0,
     0,
     0,
     0x1,
     0x1ULL,
     0x3ff0000000000000ULL},

    {0x1.19999ap+0,
     0x1.19999ap+0,
     0x1.19999ap+1,
     0x0.000000p+0,
     0x1.35c290p+0,
     0x1.000000p+0,
     0x0.000000p+0,
     0x1.0c7ebcp+0,
     1,
     1,
     0,
     0x1,
     0x1ULL,
     0x3ff19999a0000000ULL},

    {0x0.000000p+0,
     -0x1.800000p+2,
     -0x1.800000p+2,
     0x1.800000p+2,
     -0x0.000000p+0,
     -0x0.000000p+0,
     0x0.000000p+0,
     0x0.000000p+0,
     0,
     0,
     0,
     0x0,
     0x0ULL,
     0x0ULL},

    {0x1.2a05f2p+33,
     0x1.74876ep+36,
     0x1.99c82cp+36,
     -0x1.4f46b0p+36,
     0x1.b1ae4cp+69,
     0x1.99999ap-4,
     0x1.2a05f2p+33,
     0x1.86a000p+16,
     0,
     1,
     1,
     0x7fffffff,
     0x2540be400ULL,
     0x4202a05f20000000ULL},

    {0x1.74876ep+36,
     0x1.2a05f2p+33,
     0x1.99c82cp+36,
     0x1.4f46b0p+36,
     0x1.b1ae4cp+69,
     0x1.400000p+3,
     -0x1.000000p+11,
     0x1.34d0f0p+18,
     0,
     0,
     0,
     0x7fffffff,
     0x174876e000ULL,
     0x42374876e0000000ULL},

    {-0x1.2a05f2p+33,
     0x1.74876ep+36,
     0x1.4f46b0p+36,
     -0x1.99c82cp+36,
     -0x1.b1ae4cp+69,
     -0x1.99999ap-4,
     -0x1.2a05f2p+33,
     -NAN,
     0,
     1,
     1,
     0x80000000,
     0xfffffffdabf41c00ULL,
     0xc202a05f20000000ULL},

    {-0x1.74876ep+36,
     0x1.2a05f2p+33,
     -0x1.4f46b0p+36,
     -0x1.99c82cp+36,
     -0x1.b1ae4cp+69,
     -0x1.400000p+3,
     0x1.000000p+11,
     -NAN,
     0,
     1,
     1,
     0x80000000,
     0xffffffe8b7892000ULL,
     0xc2374876e0000000ULL},

    {0x1.2a05f2p+33,
     -0x1.74876ep+36,
     -0x1.4f46b0p+36,
     0x1.99c82cp+36,
     -0x1.b1ae4cp+69,
     -0x1.99999ap-4,
     0x1.2a05f2p+33,
     0x1.86a000p+16,
     0,
     0,
     0,
     0x7fffffff,
     0x2540be400ULL,
     0x4202a05f20000000ULL},

    {0x1.74876ep+36,
     -0x1.2a05f2p+33,
     0x1.4f46b0p+36,
     0x1.99c82cp+36,
     -0x1.b1ae4cp+69,
     -0x1.400000p+3,
     -0x1.000000p+11,
     0x1.34d0f0p+18,
     0,
     0,
     0,
     0x7fffffff,
     0x174876e000ULL,
     0x42374876e0000000ULL},

    {-0x1.2a05f2p+33,
     -0x1.74876ep+36,
     -0x1.99c82cp+36,
     0x1.4f46b0p+36,
     0x1.b1ae4cp+69,
     0x1.99999ap-4,
     -0x1.2a05f2p+33,
     -NAN,
     0,
     0,
     0,
     0x80000000,
     0xfffffffdabf41c00ULL,
     0xc202a05f20000000ULL},

    {-0x1.74876ep+36,
     -0x1.2a05f2p+33,
     -0x1.99c82cp+36,
     -0x1.4f46b0p+36,
     0x1.b1ae4cp+69,
     0x1.400000p+3,
     0x1.000000p+11,
     -NAN,
     0,
     1,
     1,
     0x80000000,
     0xffffffe8b7892000ULL,
     0xc2374876e0000000ULL},
};

SOFT_FLOAT_SINGLE_CONVERT_CASE TestSoftFloatSingleFromIntegers[] = {

    {0x0ULL, 0x0.000000p+0, 0x0.000000p+0},
    {0x1ULL, 0x1.000000p+0, 0x1.000000p+0},
    {0xFFFFFFFFFFFFFFFFULL, -0x1.000000p+0, -0x1.000000p+0},
    {0x5ULL, 0x1.400000p+2, 0x1.400000p+2},
    {0xAULL, 0x1.400000p+3, 0x1.400000p+3},
    {0x64ULL, 0x1.900000p+6, 0x1.900000p+6},
    {0x29AULL, 0x1.4D0000p+9, 0x1.4D0000p+9},
    {0xFFFFULL, 0x1.FFFE00p+15, 0x1.FFFE00p+15},
    {0xFFFFFULL, 0x1.FFFFE0p+19, 0x1.FFFFE0p+19},
    {0x123456ULL, 0x1.234560p+20, 0x1.234560p+20},
    {0x87654321ULL, -0x1.E26AF4p+30, 0x1.0ECA86p+31},
    {0x77654321ULL, 0x1.DD950Cp+30, 0x1.DD950Cp+30},
    {0xCCCCCCCCULL, -0x1.99999Ap+29, 0x1.99999Ap+31},
    {0xFFFFFFFFULL, -0x1.000000p+0, 0x1.000000p+32},
    {0x100000000ULL, 0x0.000000p+0, 0x1.000000p+32},
    {0x100000001ULL, 0x1.000000p+0, 0x1.000000p+32},
    {0xFFFFFFFFFULL, -0x1.000000p+0, 0x1.000000p+36},
    {0x765432112345678ULL, 0x1.234568p+28, 0x1.D950C8p+58},
    {0x7FFFFFFFFFFFFFFFULL, -0x1.000000p+0, 0x1.000000p+63},
    {0x8000000000000000ULL, 0x0.000000p+0, -0x1.000000p+63},
    {0x8000000000000001ULL, 0x1.000000p+0, -0x1.000000p+63},
    {0xCCCCCCCCCCCCCCCCULL, -0x1.99999Ap+29, -0x1.99999Ap+61},
    {0xFFFFFFFFFFFFFFFEULL, -0x1.000000p+1, -0x1.000000p+1},
    {0xFFFFFFFFFFFFFFFFULL, -0x1.000000p+0, -0x1.000000p+0},
};

//
// ------------------------------------------------------------------ Functions
//

ULONG
TestSoftFloatSingle (
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

    FLOAT_PARTS Answer;
    PSOFT_FLOAT_SINGLE_CONVERT_CASE ConvertTest;
    DOUBLE_PARTS Double;
    ULONG Failures;
    PSOFT_FLOAT_SINGLE_MATH_CASE MathTest;
    FLOAT_PARTS Result;
    ULONG TestCount;
    ULONG TestIndex;

    Failures = 0;
    TestCount = sizeof(TestSoftFloatSingleMathCases) /
                sizeof(TestSoftFloatSingleMathCases[0]);

    for (TestIndex = 0; TestIndex < TestCount; TestIndex += 1) {
        MathTest = &(TestSoftFloatSingleMathCases[TestIndex]);

        //
        // Test the math: add, subtract, multiply, divide, modulo, and
        // square root.
        //

        Answer.Float = MathTest->Sum;
        Result.Float = RtlFloatAdd(MathTest->Value1, MathTest->Value2);
        if (Answer.Ulong != Result.Ulong) {
            printf("SoftFloat Add %0.6a %0.6a was %0.6a, should have "
                   "been %0.6a.\n",
                   MathTest->Value1,
                   MathTest->Value2,
                   Result.Float,
                   Answer.Float);

            Failures += 1;
        }

        Answer.Float = MathTest->Difference;
        Result.Float = RtlFloatSubtract(MathTest->Value1, MathTest->Value2);
        if (Answer.Ulong != Result.Ulong) {
            printf("SoftFloat Subtract %0.6a %0.6a was %0.6a, should have "
                   "been %0.6a.\n",
                   MathTest->Value1,
                   MathTest->Value2,
                   Result.Float,
                   Answer.Float);

            Failures += 1;
        }

        Answer.Float = MathTest->Product;
        Result.Float = RtlFloatMultiply(MathTest->Value1, MathTest->Value2);
        if (Answer.Ulong != Result.Ulong) {
            printf("SoftFloat Multiply %0.6a %0.6a was %0.6a, should have "
                   "been %0.6a.\n",
                   MathTest->Value1,
                   MathTest->Value2,
                   Result.Float,
                   Answer.Float);

            Failures += 1;
        }

        Answer.Float = MathTest->Quotient;
        Result.Float = RtlFloatDivide(MathTest->Value1, MathTest->Value2);
        if (Answer.Ulong != Result.Ulong) {
            printf("SoftFloat Divide %0.6a %0.6a was %0.6a, should have "
                   "been %0.6a.\n",
                   MathTest->Value1,
                   MathTest->Value2,
                   Result.Float,
                   Answer.Float);

            Failures += 1;
        }

        Answer.Float = MathTest->Remainder;
        Result.Float = RtlFloatModulo(MathTest->Value1, MathTest->Value2);
        if (Answer.Ulong != Result.Ulong) {
            printf("SoftFloat Remainder %0.6a %0.6a was %0.6a, should have "
                   "been %0.6a.\n",
                   MathTest->Value1,
                   MathTest->Value2,
                   Result.Float,
                   Answer.Float);

            Failures += 1;
        }

        Answer.Float = MathTest->SquareRoot;
        Result.Float = RtlFloatSquareRoot(MathTest->Value1);
        if (Answer.Ulong != Result.Ulong) {
            printf("SoftFloat Square Root %0.6a was %0.6a, should have "
                   "been %0.6a.\n",
                   MathTest->Value1,
                   Result.Float,
                   Answer.Float);

            Failures += 1;
        }

        //
        // Test comparison, equal, equal (signaling), less than or equal to,
        // less than or equal to (quiet), less than, and less than (quiet).
        //

        Answer.Ulong = MathTest->Equal;
        Result.Ulong = RtlFloatIsEqual(MathTest->Value1, MathTest->Value2);
        if (Answer.Ulong != Result.Ulong) {
            printf("SoftFloat Equal %0.6a %0.6a was %d, should have "
                   "been %d.\n",
                   MathTest->Value1,
                   MathTest->Value2,
                   (ULONG)Result.Ulong,
                   (ULONG)Answer.Ulong);

            Failures += 1;
        }

        Answer.Ulong = MathTest->Equal;
        Result.Ulong = RtlFloatSignalingIsEqual(MathTest->Value1,
                                                MathTest->Value2);

        if (Answer.Ulong != Result.Ulong) {
            printf("SoftFloat Equal (signaling) %0.6a %0.6a was %d, should "
                   "have been %d.\n",
                   MathTest->Value1,
                   MathTest->Value2,
                   (ULONG)Result.Ulong,
                   (ULONG)Answer.Ulong);

            Failures += 1;
        }

        Answer.Ulong = MathTest->LessThanOrEqual;
        Result.Ulong = RtlFloatIsLessThanOrEqual(MathTest->Value1,
                                                 MathTest->Value2);

        if (Answer.Ulong != Result.Ulong) {
            printf("SoftFloat Less/equal %0.6a %0.6a was %d, should "
                   "have been %d.\n",
                   MathTest->Value1,
                   MathTest->Value2,
                   (ULONG)Result.Ulong,
                   (ULONG)Answer.Ulong);

            Failures += 1;
        }

        Answer.Ulong = MathTest->LessThanOrEqual;
        Result.Ulong = RtlFloatIsLessThanOrEqualQuiet(MathTest->Value1,
                                                      MathTest->Value2);

        if (Answer.Ulong != Result.Ulong) {
            printf("SoftFloat Less/equal (quiet) %0.6a %0.6a was %d, should "
                   "have been %d.\n",
                   MathTest->Value1,
                   MathTest->Value2,
                   (ULONG)Result.Ulong,
                   (ULONG)Answer.Ulong);

            Failures += 1;
        }

        Answer.Ulong = MathTest->LessThan;
        Result.Ulong = RtlFloatIsLessThan(MathTest->Value1,
                                          MathTest->Value2);

        if (Answer.Ulong != Result.Ulong) {
            printf("SoftFloat Less than %0.6a %0.6a was %d, should "
                   "have been %d.\n",
                   MathTest->Value1,
                   MathTest->Value2,
                   (ULONG)Result.Ulong,
                   (ULONG)Answer.Ulong);

            Failures += 1;
        }

        Answer.Ulong = MathTest->LessThan;
        Result.Ulong = RtlFloatIsLessThanQuiet(MathTest->Value1,
                                               MathTest->Value2);

        if (Answer.Ulong != Result.Ulong) {
            printf("SoftFloat Less than (quiet) %0.6a %0.6a was %d, should "
                   "have been %d.\n",
                   MathTest->Value1,
                   MathTest->Value2,
                   (ULONG)Result.Ulong,
                   (ULONG)Answer.Ulong);

            Failures += 1;
        }

        //
        // Test the conversion of the float to an integer.
        //

        Answer.Ulong = MathTest->Int32;
        Result.Ulong = RtlFloatConvertToInteger32(MathTest->Value1);
        if (Answer.Ulong != Result.Ulong) {
            printf("SoftFloat ConvertToInt32 %0.6a was 0x%x, should "
                   "have been 0x%x.\n",
                   MathTest->Value1,
                   (ULONG)Result.Ulong,
                   (ULONG)Answer.Ulong);

            Failures += 1;
        }

        Double.Ulonglong = RtlFloatConvertToInteger64(MathTest->Value1);
        if (MathTest->Int64 != Double.Ulonglong) {
            printf("SoftFloat ConvertToInt64 %0.6a was 0x%llx, should "
                   "have been 0x%llx.\n",
                   MathTest->Value1,
                   Double.Ulonglong,
                   MathTest->Int64);

            Failures += 1;
        }

        Double.Double = RtlFloatConvertToDouble(MathTest->Value1);
        if (MathTest->Double != Double.Ulonglong) {
            printf("SoftFloat ConvertFloatToDouble %0.6a was 0x%llx, should "
                   "have been 0x%llx.\n",
                   MathTest->Value1,
                   Double.Ulonglong,
                   MathTest->Double);

            Failures += 1;
        }
    }

    MathTest = NULL;

    //
    // Also test the conversion of integers to floats.
    //

    TestCount = sizeof(TestSoftFloatSingleFromIntegers) /
                sizeof(TestSoftFloatSingleFromIntegers[0]);

    for (TestIndex = 0; TestIndex < TestCount; TestIndex += 1) {
        ConvertTest = &(TestSoftFloatSingleFromIntegers[TestIndex]);
        Answer.Float = ConvertTest->FromInt32;
        Result.Float = RtlFloatConvertFromInteger32((LONG)ConvertTest->Integer);
        if (Answer.Ulong != Result.Ulong) {
            printf("SoftFloat ConvertFromInt32 0x%x was %0.6a, should "
                   "have been %0.6a.\n",
                   (LONG)ConvertTest->Integer,
                   Result.Float,
                   Answer.Float);

            Failures += 1;
        }

        Answer.Float = ConvertTest->FromInt64;
        Result.Float = RtlFloatConvertFromInteger64(ConvertTest->Integer);
        if (Answer.Ulong != Result.Ulong) {
            printf("SoftFloat ConvertFromInt64 0x%llx was %0.6a, should "
                   "have been %0.6a.\n",
                   ConvertTest->Integer,
                   Result.Float,
                   Answer.Float);

            Failures += 1;
        }
    }

    if (Failures != 0) {
        printf("\n\n%d Soft Float single-precision test failures.\n\n",
               Failures);
    }

    return Failures;
}

//
// --------------------------------------------------------- Internal Functions
//

