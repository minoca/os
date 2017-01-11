/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    mathftst.c

Abstract:

    This module implements the tests for the single-precision floating point
    math portion of the C library.

Author:

    Chris Stevens 6-Jan-2017

Environment:

    Test

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// Define this so it doesn't get defined to an import.
//

#define LIBC_API

#include <minoca/lib/types.h>
#include <minoca/lib/status.h>
#include <minoca/lib/rtl.h>

#include <math.h>
#include <stdio.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the amount that the result is allowed to vary from the known answer.
//

#define MATH_RESULT_SLOP 2

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _MATH_TRIG_FLOAT_VALUE {
    float Argument;
    float Sine;
    float Cosine;
    float Tangent;
    float HyperbolicSine;
    float HyperbolicCosine;
    float HyperbolicTangent;
} MATH_TRIG_FLOAT_VALUE, *PMATH_TRIG_FLOAT_VALUE;

typedef struct _MATH_TEST_SQUARE_ROOT_FLOAT_VALUE {
    float Argument;
    float SquareRoot;
} MATH_TEST_SQUARE_ROOT_FLOAT_VALUE, *PMATH_TEST_SQUARE_ROOT_FLOAT_VALUE;

typedef struct _MATH_ARC_FLOAT_VALUE {
    float Argument;
    float ArcSine;
    float ArcCosine;
    float ArcTangent;
} MATH_ARC_FLOAT_VALUE, *PMATH_ARC_FLOAT_VALUE;

typedef struct _MATH_ARC_TANGENT_FLOAT {
    float Numerator;
    float Denominator;
    float ArcTangent;
    float ArcTangent2;
} MATH_ARC_TANGENT_FLOAT, *PMATH_ARC_TANGENT_FLOAT;

typedef struct _MATH_EXP_FLOAT {
    float Argument;
    float Exponentiation;
    float ExponentiationMinusOne;
} MATH_EXP_FLOAT, *PMATH_EXP_FLOAT;

typedef struct _MATH_POWER_FLOAT {
    float Value;
    float Exponent;
    float Result;
    float Hypotenuse;
} MATH_POWER_FLOAT, *PMATH_POWER_FLOAT;

typedef struct _MATH_LOGARITHM_FLOAT {
    float Argument;
    float Logarithm;
    float Log2;
    float Log10;
} MATH_LOGARITHM_FLOAT, *PMATH_LOGARITHM_FLOAT;

typedef struct _MATH_DECOMPOSITION_FLOAT {
    float Argument;
    float IntegerPart;
    float FractionalPart;
} MATH_DECOMPOSITION_FLOAT, *PMATH_DECOMPOSITION_FLOAT;

typedef struct _MATH_CEILING_FLOOR_FLOAT_VALUE {
    float Argument;
    float Ceiling;
    float Floor;
} MATH_CEILING_FLOOR_FLOAT_VALUE, *PMATH_CEILING_FLOOR_FLOAT_VALUE;

typedef struct _MATH_MODULO_FLOAT_VALUE {
    float Numerator;
    float Denominator;
    float Remainder;
} MATH_MODULO_FLOAT_VALUE, *PMATH_MODULO_FLOAT_VALUE;

//
// ----------------------------------------------- Internal Function Prototypes
//

ULONG
TestBasicTrigonometryFloat (
    VOID
    );

ULONG
TestSquareRootFloat (
    VOID
    );

ULONG
TestArcTrigonometryFloat (
    VOID
    );

ULONG
TestExponentiationFloat (
    VOID
    );

ULONG
TestPowerFloat (
    VOID
    );

ULONG
TestLogarithmFloat (
    VOID
    );

ULONG
TestDecompositionFloat (
    VOID
    );

ULONG
TestCeilingAndFloorFloat (
    VOID
    );

ULONG
TestModuloFloat (
    VOID
    );

BOOL
TestCompareResultsFloat (
    float Value1,
    float Value2
    );

//
// -------------------------------------------------------------------- Globals
//

MATH_TRIG_FLOAT_VALUE TestBasicTrigonometryFloats[] = {
    {-0x1.921fb6p-1,
     -0x1.6a09e8p-1,
     0x1.6a09e6p-1,
     -0x1.000000p+0,
     -0x1.bcc272p-1,
     0x1.531994p+0,
     -0x1.4fc444p-1},

    {0x1.921fb6p-1,
     0x1.6a09e8p-1,
     0x1.6a09e6p-1,
     0x1.000000p+0,
     0x1.bcc272p-1,
     0x1.531994p+0,
     0x1.4fc444p-1},

    {0x1.921fb6p+1,
     -0x1.777a5cp-24,
     -0x1.000000p+0,
     0x1.777a5cp-24,
     0x1.718f46p+3,
     0x1.72f14cp+3,
     0x1.fe1760p-1},

    {-0x1.921fb6p+1,
     0x1.777a5cp-24,
     -0x1.000000p+0,
     -0x1.777a5cp-24,
     -0x1.718f46p+3,
     0x1.72f14cp+3,
     -0x1.fe1760p-1},

    {0x1.f6a7a2p+1,
     -0x1.6a09e6p-1,
     -0x1.6a09e8p-1,
     0x1.fffffcp-1,
     0x1.95dfe0p+4,
     0x1.963094p+4,
     0x1.ff9a46p-1},

    {-0x1.f6a7a2p+1,
     0x1.6a09e6p-1,
     -0x1.6a09e8p-1,
     -0x1.fffffcp-1,
     -0x1.95dfe0p+4,
     0x1.963094p+4,
     -0x1.ff9a46p-1},

    {0x1.921fb6p+2,
     0x1.777a5cp-23,
     0x1.000000p+0,
     0x1.777a5cp-23,
     0x1.0bbeb4p+8,
     0x1.0bbf30p+8,
     0x1.ffff16p-1},

    {-0x1.921fb6p+2,
     -0x1.777a5cp-23,
     0x1.000000p+0,
     -0x1.777a5cp-23,
     -0x1.0bbeb4p+8,
     0x1.0bbf30p+8,
     -0x1.ffff16p-1},

    {0x1.f6a7a2p+1,
     -0x1.6a09e6p-1,
     -0x1.6a09e8p-1,
     0x1.fffffcp-1,
     0x1.95dfe0p+4,
     0x1.963094p+4,
     0x1.ff9a46p-1},

    {-0x1.f6a7a2p+1,
     0x1.6a09e6p-1,
     -0x1.6a09e8p-1,
     -0x1.fffffcp-1,
     -0x1.95dfe0p+4,
     0x1.963094p+4,
     -0x1.ff9a46p-1},

    {0x1.5fdbbep+2,
     -0x1.6a09eap-1,
     0x1.6a09e4p-1,
     -0x1.000004p+0,
     0x1.e84b3cp+6,
     0x1.e84f6cp+6,
     0x1.fffb9ap-1},

    {-0x1.5fdbbep+2,
     0x1.6a09eap-1,
     0x1.6a09e4p-1,
     0x1.000004p+0,
     -0x1.e84b3cp+6,
     0x1.e84f6cp+6,
     -0x1.fffb9ap-1},

    {0x1.86a000p+16,
     0x1.24daaap-5,
     -0x1.ffac38p-1,
     -0x1.250a9ep-5,
     INFINITY,
     INFINITY,
     0x1.000000p+0},

    {0x1.2a05f2p+33,
     -0x1.f334c8p-2,
     0x1.bf098ap-1,
     -0x1.1de000p-1,
     INFINITY,
     INFINITY,
     0x1.000000p+0},

    {0x1.c6bf52p+49,
     0x1.fd267ep-1,
     0x1.af8c60p-4,
     0x1.2e08d6p+3,
     INFINITY,
     INFINITY,
     0x1.000000p+0},

    {0x1.5af1d8p+66,
     0x1.502ad2p-1,
     0x1.822e46p-1,
     0x1.bdb120p-1,
     INFINITY,
     INFINITY,
     0x1.000000p+0},

    {0x1.08b2a2p+83,
     -0x1.9f9964p-2,
     0x1.d3ef68p-1,
     -0x1.c6bc50p-2,
     INFINITY,
     INFINITY,
     0x1.000000p+0},

    {0x1.93e594p+99,
     -0x1.951360p-1,
     -0x1.392444p-1,
     0x1.4b2876p+0,
     INFINITY,
     INFINITY,
     0x1.000000p+0},

    {0x1.342618p+116,
     -0x1.ffaaacp-1,
     -0x1.278c42p-5,
     0x1.bb3312p+4,
     INFINITY,
     INFINITY,
     0x1.000000p+0},

    {INFINITY,
     -NAN,
     -NAN,
     -NAN,
     INFINITY,
     INFINITY,
     0x1.000000p+0},

    {INFINITY,
     -NAN,
     -NAN,
     -NAN,
     INFINITY,
     INFINITY,
     0x1.000000p+0},

    {INFINITY,
     -NAN,
     -NAN,
     -NAN,
     INFINITY,
     INFINITY,
     0x1.000000p+0},

    {INFINITY,
     -NAN,
     -NAN,
     -NAN,
     INFINITY,
     INFINITY,
     0x1.000000p+0},

    {INFINITY,
     -NAN,
     -NAN,
     -NAN,
     INFINITY,
     INFINITY,
     0x1.000000p+0},

    {-0x1.24f800p+18,
     -0x1.b68860p-4,
     -0x1.fd0e9ep-1,
     0x1.b91162p-4,
     -INFINITY,
     INFINITY,
     -0x1.000000p+0},

    {-0x1.550f7ep+51,
     -0x1.764b5ep-1,
     0x1.5d5a70p-1,
     -0x1.1246c4p+0,
     -INFINITY,
     INFINITY,
     -0x1.000000p+0},

    {-0x1.8d0bf4p+84,
     -0x1.149138p-2,
     -0x1.ecf8e6p-1,
     0x1.1f3e02p-2,
     -INFINITY,
     INFINITY,
     -0x1.000000p+0},

    {-INFINITY,
     -NAN,
     -NAN,
     -NAN,
     -INFINITY,
     INFINITY,
     -0x1.000000p+0},

    {-INFINITY,
     -NAN,
     -NAN,
     -NAN,
     -INFINITY,
     INFINITY,
     -0x1.000000p+0},

    {-INFINITY,
     -NAN,
     -NAN,
     -NAN,
     -INFINITY,
     INFINITY,
     -0x1.000000p+0},

    {-INFINITY,
     -NAN,
     -NAN,
     -NAN,
     -INFINITY,
     INFINITY,
     -0x1.000000p+0},

    {0x1.4f8b58p-17,
     0x1.4f8b58p-17,
     0x1.000000p+0,
     0x1.4f8b58p-17,
     0x1.4f8b58p-17,
     0x1.000000p+0,
     0x1.4f8b58p-17},

    {0x1.b7cdfep-34,
     0x1.b7cdfep-34,
     0x1.000000p+0,
     0x1.b7cdfep-34,
     0x1.b7cdfep-34,
     0x1.000000p+0,
     0x1.b7cdfep-34},

    {0x1.203afap-50,
     0x1.203afap-50,
     0x1.000000p+0,
     0x1.203afap-50,
     0x1.203afap-50,
     0x1.000000p+0,
     0x1.203afap-50},

    {0x1.79ca10p-67,
     0x1.79ca10p-67,
     0x1.000000p+0,
     0x1.79ca10p-67,
     0x1.79ca10p-67,
     0x1.000000p+0,
     0x1.79ca10p-67},

    {-0x1.f75104p-16,
     -0x1.f75104p-16,
     0x1.000000p+0,
     -0x1.f75104p-16,
     -0x1.f75104p-16,
     0x1.000000p+0,
     -0x1.f75102p-16},

    {-0x1.07e1fep-35,
     -0x1.07e1fep-35,
     0x1.000000p+0,
     -0x1.07e1fep-35,
     -0x1.07e1fep-35,
     0x1.000000p+0,
     -0x1.07e1fep-35},

    {-0x1.59e060p-52,
     -0x1.59e060p-52,
     0x1.000000p+0,
     -0x1.59e060p-52,
     -0x1.59e060p-52,
     0x1.000000p+0,
     -0x1.59e060p-52},

    {-0x1.1b578cp-65,
     -0x1.1b578cp-65,
     0x1.000000p+0,
     -0x1.1b578cp-65,
     -0x1.1b578cp-65,
     0x1.000000p+0,
     -0x1.1b578cp-65},

    {-0x0.000000p+0,
     -0x0.000000p+0,
     0x1.000000p+0,
     -0x0.000000p+0,
     -0x0.000000p+0,
     0x1.000000p+0,
     -0x0.000000p+0},

    {-0x0.000000p+0,
     -0x0.000000p+0,
     0x1.000000p+0,
     -0x0.000000p+0,
     -0x0.000000p+0,
     0x1.000000p+0,
     -0x0.000000p+0},

    {INFINITY,
     -NAN,
     -NAN,
     -NAN,
     INFINITY,
     INFINITY,
     0x1.000000p+0},

    {-INFINITY,
     -NAN,
     -NAN,
     -NAN,
     -INFINITY,
     INFINITY,
     -0x1.000000p+0},

    {NAN,
     NAN,
     NAN,
     NAN,
     NAN,
     NAN,
     NAN},
};

MATH_TEST_SQUARE_ROOT_FLOAT_VALUE TestSquareRootFloats[] = {
    {0x1.921fb6p-1, 0x1.c5bf8ap-1},
    {0x1.921fb6p+1, 0x1.c5bf8ap+0},
    {0x1.f6a7a2p+1, 0x1.fb4e4ep+0},
    {0x1.921fb6p+2, 0x1.40d932p+1},
    {0x1.f6a7a2p+1, 0x1.fb4e4ep+0},
    {0x1.5fdbbep+2, 0x1.2c2050p+1},
    {0x1.86a000p+16, 0x1.3c3a4ep+8},
    {0x1.2a05f2p+33, 0x1.86a000p+16},
    {0x1.c6bf52p+49, 0x1.e28678p+24},
    {0x1.5af1d8p+66, 0x1.2a05f2p+33},
    {0x1.08b2a2p+83, 0x1.702338p+41},
    {0x1.93e594p+99, 0x1.c6bf52p+49},
    {0x1.342618p+116, 0x1.18dddep+58},
    {INFINITY, INFINITY},
    {INFINITY, INFINITY},
    {INFINITY, INFINITY},
    {INFINITY, INFINITY},
    {INFINITY, INFINITY},
    {0x1.4f8b58p-17, 0x1.9e7c6ep-9},
    {0x1.b7cdfep-34, 0x1.4f8b58p-17},
    {0x1.203afap-50, 0x1.0fa338p-25},
    {0x1.79ca10p-67, 0x1.b7cdfep-34},
    {0x1.000000p+0, 0x1.000000p+0},
    {0x1.000000p+1, 0x1.6a09e6p+0},
    {0x1.800000p+1, 0x1.bb67aep+0},
    {0x1.22ad96p+27, 0x1.81c800p+13},
    {0x1.900000p+4, 0x1.400000p+2},
    {0x1.fae148p-1, 0x1.fd6efep-1},
    {-0x1.000000p+0, -NAN},
    {INFINITY, INFINITY},
    {-INFINITY, -NAN},
    {NAN, NAN},
};

MATH_ARC_FLOAT_VALUE TestArcFloats[] = {
    {-0x0.000000p+0,
     -0x0.000000p+0,
     0x1.921fb6p+0,
     -0x0.000000p+0},

    {-0x1.47ae14p-7,
     -0x1.47af7ap-7,
     0x1.94af14p+0,
     -0x1.47ab48p-7},

    {-0x1.47ae14p-6,
     -0x1.47b3acp-6,
     0x1.973e84p+0,
     -0x1.47a2e6p-6},

    {-0x1.eb851ep-6,
     -0x1.eb9800p-6,
     0x1.99ce16p+0,
     -0x1.eb5f64p-6},

    {-0x1.47ae14p-5,
     -0x1.47c476p-5,
     0x1.9c5dd8p+0,
     -0x1.478162p-5},

    {-0x1.99999ap-5,
     -0x1.99c558p-5,
     0x1.9eede0p+0,
     -0x1.99425ap-5},

    {-0x1.eb851ep-5,
     -0x1.ebd0bcp-5,
     0x1.a17e3cp+0,
     -0x1.eaee72p-5},

    {-0x1.1eb852p-4,
     -0x1.1ef466p-4,
     0x1.a40efcp+0,
     -0x1.1e40c8p-4},

    {-0x1.47ae14p-4,
     -0x1.4807d0p-4,
     0x1.a6a032p+0,
     -0x1.46fbcep-4},

    {-0x1.70a3d8p-4,
     -0x1.7123b6p-4,
     0x1.a931f0p+0,
     -0x1.6fa646p-4},

    {-0x1.99999ap-4,
     -0x1.9a4928p-4,
     0x1.abc448p+0,
     -0x1.983e28p-4},

    {-0x1.c28f5cp-4,
     -0x1.c3793ep-4,
     0x1.ae574ap+0,
     -0x1.c0c17ep-4},

    {-0x1.eb851ep-4,
     -0x1.ecb514p-4,
     0x1.b0eb06p+0,
     -0x1.e92e4ep-4},

    {-0x1.0a3d70p-3,
     -0x1.0afee4p-3,
     0x1.b37f92p+0,
     -0x1.08c154p-3},

    {-0x1.1eb852p-3,
     -0x1.1faa3cp-3,
     0x1.b614fcp+0,
     -0x1.1cde56p-3},

    {-0x1.333334p-3,
     -0x1.345d24p-3,
     0x1.b8ab5ap+0,
     -0x1.30ed38p-3},

    {-0x1.47ae14p-3,
     -0x1.49182ep-3,
     0x1.bb42bcp+0,
     -0x1.44ed0cp-3},

    {-0x1.5c28f6p-3,
     -0x1.5ddbf2p-3,
     0x1.bddb34p+0,
     -0x1.58dcf0p-3},

    {-0x1.70a3d8p-3,
     -0x1.72a908p-3,
     0x1.c074d6p+0,
     -0x1.6cbbfep-3},

    {-0x1.851eb8p-3,
     -0x1.878004p-3,
     0x1.c30fb6p+0,
     -0x1.808956p-3},

    {-0x1.99999ap-3,
     -0x1.9c618cp-3,
     0x1.c5abe6p+0,
     -0x1.944420p-3},

    {-0x1.ae147ap-3,
     -0x1.b14e36p-3,
     0x1.c8497cp+0,
     -0x1.a7eb86p-3},

    {-0x1.c28f5cp-3,
     -0x1.c646aap-3,
     0x1.cae88ap+0,
     -0x1.bb7ebap-3},

    {-0x1.d70a3ep-3,
     -0x1.db4b8cp-3,
     0x1.cd8926p+0,
     -0x1.cefcf2p-3},

    {-0x1.eb851ep-3,
     -0x1.f05d80p-3,
     0x1.d02b66p+0,
     -0x1.e26568p-3},

    {-0x1.000000p-2,
     -0x1.02be9cp-2,
     0x1.d2cf5cp+0,
     -0x1.f5b760p-3},

    {-0x1.0a3d70p-2,
     -0x1.0d55b0p-2,
     0x1.d57522p+0,
     -0x1.04790ep-2},

    {-0x1.147ae2p-2,
     -0x1.17f458p-2,
     0x1.d81cccp+0,
     -0x1.0e0a7ap-2},

    {-0x1.1eb852p-2,
     -0x1.229aecp-2,
     0x1.dac670p+0,
     -0x1.178f98p-2},

    {-0x1.28f5c2p-2,
     -0x1.2d49ccp-2,
     0x1.dd7228p+0,
     -0x1.210816p-2},

    {-0x1.333334p-2,
     -0x1.38015ap-2,
     0x1.e0200cp+0,
     -0x1.2a73a8p-2},

    {-0x1.3d70a4p-2,
     -0x1.42c1f6p-2,
     0x1.e2d032p+0,
     -0x1.33d1fap-2},

    {-0x1.47ae14p-2,
     -0x1.4d8c08p-2,
     0x1.e582b8p+0,
     -0x1.3d22c4p-2},

    {-0x1.51eb86p-2,
     -0x1.585ff8p-2,
     0x1.e837b4p+0,
     -0x1.4665c4p-2},

    {-0x1.5c28f6p-2,
     -0x1.633e30p-2,
     0x1.eaef42p+0,
     -0x1.4f9ab0p-2},

    {-0x1.666666p-2,
     -0x1.6e271ep-2,
     0x1.eda97cp+0,
     -0x1.58c148p-2},

    {-0x1.70a3d8p-2,
     -0x1.791b3ap-2,
     0x1.f06684p+0,
     -0x1.61d954p-2},

    {-0x1.7ae148p-2,
     -0x1.841af2p-2,
     0x1.f32672p+0,
     -0x1.6ae292p-2},

    {-0x1.851eb8p-2,
     -0x1.8f26c2p-2,
     0x1.f5e966p+0,
     -0x1.73dccep-2},

    {-0x1.8f5c28p-2,
     -0x1.9a3f2ap-2,
     0x1.f8af80p+0,
     -0x1.7cc7d6p-2},

    {-0x1.99999ap-2,
     -0x1.a564acp-2,
     0x1.fb78e0p+0,
     -0x1.85a376p-2},

    {-0x1.a3d70ap-2,
     -0x1.b097ccp-2,
     0x1.fe45a8p+0,
     -0x1.8e6f80p-2},

    {-0x1.ae147ap-2,
     -0x1.bbd916p-2,
     0x1.008afep+1,
     -0x1.972bcap-2},

    {-0x1.b851ecp-2,
     -0x1.c7291ep-2,
     0x1.01f4fep+1,
     -0x1.9fd82cp-2},

    {-0x1.c28f5cp-2,
     -0x1.d28876p-2,
     0x1.0360eap+1,
     -0x1.a8747ep-2},

    {-0x1.ccccccp-2,
     -0x1.ddf7bap-2,
     0x1.04ced2p+1,
     -0x1.b1009ep-2},

    {-0x1.d70a3ep-2,
     -0x1.e97794p-2,
     0x1.063ecep+1,
     -0x1.b97c70p-2},

    {-0x1.e147aep-2,
     -0x1.f508a4p-2,
     0x1.07b0f0p+1,
     -0x1.c1e7d2p-2},

    {-0x1.eb851ep-2,
     -0x1.0055d0p-1,
     0x1.09254ep+1,
     -0x1.ca42acp-2},

    {-0x1.f5c290p-2,
     -0x1.0630a2p-1,
     0x1.0a9c02p+1,
     -0x1.d28ceap-2},

    {-0x1.000000p-1,
     -0x1.0c1528p-1,
     0x1.0c1524p+1,
     -0x1.dac670p-2},

    {-0x1.051eb8p-1,
     -0x1.1203c2p-1,
     0x1.0d90cap+1,
     -0x1.e2ef30p-2},

    {-0x1.0a3d70p-1,
     -0x1.17fcdcp-1,
     0x1.0f0f10p+1,
     -0x1.eb071ap-2},

    {-0x1.0f5c28p-1,
     -0x1.1e00e8p-1,
     0x1.109014p+1,
     -0x1.f30e20p-2},

    {-0x1.147ae2p-1,
     -0x1.24105ap-1,
     0x1.1213f0p+1,
     -0x1.fb0438p-2},

    {-0x1.19999ap-1,
     -0x1.2a2baap-1,
     0x1.139ac4p+1,
     -0x1.0174aap-1},

    {-0x1.1eb852p-1,
     -0x1.30535ap-1,
     0x1.1524b0p+1,
     -0x1.055ebap-1},

    {-0x1.23d70ap-1,
     -0x1.3687f4p-1,
     0x1.16b1d6p+1,
     -0x1.094048p-1},

    {-0x1.28f5c2p-1,
     -0x1.3cca06p-1,
     0x1.18425ap+1,
     -0x1.0d1952p-1},

    {-0x1.2e147ap-1,
     -0x1.431a28p-1,
     0x1.19d664p+1,
     -0x1.10e9d8p-1},

    {-0x1.333334p-1,
     -0x1.497900p-1,
     0x1.1b6e1ap+1,
     -0x1.14b1dep-1},

    {-0x1.3851ecp-1,
     -0x1.4fe732p-1,
     0x1.1d09a6p+1,
     -0x1.187162p-1},

    {-0x1.3d70a4p-1,
     -0x1.566576p-1,
     0x1.1ea936p+1,
     -0x1.1c2866p-1},

    {-0x1.428f5cp-1,
     -0x1.5cf48ep-1,
     0x1.204cfep+1,
     -0x1.1fd6f0p-1},

    {-0x1.47ae14p-1,
     -0x1.63954ap-1,
     0x1.21f52cp+1,
     -0x1.237d04p-1},

    {-0x1.4cccccp-1,
     -0x1.6a4884p-1,
     0x1.23a1fcp+1,
     -0x1.271aa6p-1},

    {-0x1.51eb86p-1,
     -0x1.710f2ep-1,
     0x1.2553a6p+1,
     -0x1.2aafdep-1},

    {-0x1.570a3ep-1,
     -0x1.77ea3cp-1,
     0x1.270a68p+1,
     -0x1.2e3cb0p-1},

    {-0x1.5c28f6p-1,
     -0x1.7edac2p-1,
     0x1.28c68ap+1,
     -0x1.31c124p-1},

    {-0x1.6147aep-1,
     -0x1.85e1e8p-1,
     0x1.2a8854p+1,
     -0x1.353d42p-1},

    {-0x1.666666p-1,
     -0x1.8d00eap-1,
     0x1.2c5014p+1,
     -0x1.38b112p-1},

    {-0x1.6b851ep-1,
     -0x1.94391ep-1,
     0x1.2e1e22p+1,
     -0x1.3c1ca0p-1},

    {-0x1.70a3d8p-1,
     -0x1.9b8c00p-1,
     0x1.2ff2dap+1,
     -0x1.3f7ff4p-1},

    {-0x1.75c290p-1,
     -0x1.a2fb1ep-1,
     0x1.31cea0p+1,
     -0x1.42db16p-1},

    {-0x1.7ae148p-1,
     -0x1.aa8838p-1,
     0x1.33b1e8p+1,
     -0x1.462e16p-1},

    {-0x1.800000p-1,
     -0x1.b23536p-1,
     0x1.359d28p+1,
     -0x1.4978fap-1},

    {-0x1.851eb8p-1,
     -0x1.ba0430p-1,
     0x1.3790e6p+1,
     -0x1.4cbbd2p-1},

    {-0x1.8a3d70p-1,
     -0x1.c1f77ap-1,
     0x1.398db8p+1,
     -0x1.4ff6a8p-1},

    {-0x1.8f5c28p-1,
     -0x1.ca11a6p-1,
     0x1.3b9442p+1,
     -0x1.53298ap-1},

    {-0x1.947ae2p-1,
     -0x1.d25594p-1,
     0x1.3da53ep+1,
     -0x1.565484p-1},

    {-0x1.99999ap-1,
     -0x1.dac674p-1,
     0x1.3fc176p+1,
     -0x1.5977a6p-1},

    {-0x1.9eb852p-1,
     -0x1.e367ecp-1,
     0x1.41e9d4p+1,
     -0x1.5c92fap-1},

    {-0x1.a3d70ap-1,
     -0x1.ec3e14p-1,
     0x1.441f5ep+1,
     -0x1.5fa690p-1},

    {-0x1.a8f5c2p-1,
     -0x1.f54d9ep-1,
     0x1.466342p+1,
     -0x1.62b276p-1},

    {-0x1.ae147ap-1,
     -0x1.fe9beap-1,
     0x1.48b6d4p+1,
     -0x1.65b6bep-1},

    {-0x1.b33334p-1,
     -0x1.0417a0p+0,
     0x1.4b1baap+1,
     -0x1.68b372p-1},

    {-0x1.b851ecp-1,
     -0x1.090772p+0,
     0x1.4d9392p+1,
     -0x1.6ba8a4p-1},

    {-0x1.bd70a4p-1,
     -0x1.0e21c0p+0,
     0x1.5020bap+1,
     -0x1.6e9664p-1},

    {-0x1.c28f5cp-1,
     -0x1.136bb6p+0,
     0x1.52c5b4p+1,
     -0x1.717cbep-1},

    {-0x1.c7ae14p-1,
     -0x1.18eb9ep+0,
     0x1.5585a8p+1,
     -0x1.745bc6p-1},

    {-0x1.ccccccp-1,
     -0x1.1ea938p+0,
     0x1.586476p+1,
     -0x1.77338ap-1},

    {-0x1.d1eb86p-1,
     -0x1.24ae46p+0,
     0x1.5b66fcp+1,
     -0x1.7a041cp-1},

    {-0x1.d70a3ep-1,
     -0x1.2b0756p+0,
     0x1.5e9384p+1,
     -0x1.7ccd88p-1},

    {-0x1.dc28f6p-1,
     -0x1.31c50cp+0,
     0x1.61f260p+1,
     -0x1.7f8fe2p-1},

    {-0x1.e147aep-1,
     -0x1.38fe4ep+0,
     0x1.658f02p+1,
     -0x1.824b3ap-1},

    {-0x1.e66666p-1,
     -0x1.40d412p+0,
     0x1.6979e4p+1,
     -0x1.84ffa0p-1},

    {-0x1.eb851ep-1,
     -0x1.4978fap+0,
     0x1.6dcc58p+1,
     -0x1.87ad24p-1},

    {-0x1.f0a3d8p-1,
     -0x1.534258p+0,
     0x1.72b106p+1,
     -0x1.8a53d8p-1},

    {-0x1.f5c290p-1,
     -0x1.5ed692p+0,
     0x1.787b24p+1,
     -0x1.8cf3ccp-1},

    {-0x1.fae148p-1,
     -0x1.6de3c8p+0,
     0x1.8001bep+1,
     -0x1.8f8d10p-1},

    {-0x1.000000p+0,
     -0x1.921fb6p+0,
     0x1.921fb6p+1,
     -0x1.921fb6p-1},

    {-0x1.028f5cp+0,
     -NAN,
     -NAN,
     -0x1.94abcep-1},

    {-0x1.051eb8p+0,
     -NAN,
     -NAN,
     -0x1.973168p-1},
};

MATH_ARC_TANGENT_FLOAT TestArcTangentFloats[] = {
    {NAN,
     NAN,
     NAN,
     NAN},

    {NAN,
     0x1.000000p+0,
     NAN,
     NAN},

    {0x1.000000p+0,
     NAN,
     NAN,
     NAN},

    {0x1.000000p+0,
     0x1.000000p+0,
     0x1.921fb6p-1,
     0x1.921fb6p-1},

    {0x0.000000p+0,
     0x1.19999ap+0,
     0x0.000000p+0,
     0x0.000000p+0},

    {-0x0.000000p+0,
     0x1.19999ap+0,
     -0x0.000000p+0,
     -0x0.000000p+0},

    {0x0.000000p+0,
     -0x1.19999ap+0,
     -0x0.000000p+0,
     0x1.921fb6p+1},

    {-0x0.000000p+0,
     -0x1.19999ap+0,
     0x0.000000p+0,
     -0x1.921fb6p+1},

    {0x1.e66666p-1,
     0x0.000000p+0,
     0x1.921fb6p+0,
     0x1.921fb6p+0},

    {-0x1.e66666p-1,
     -0x0.000000p+0,
     0x1.921fb6p+0,
     -0x1.921fb6p+0},

    {INFINITY,
     INFINITY,
     -NAN,
     0x1.921fb6p-1},

    {-INFINITY,
     INFINITY,
     -NAN,
     -0x1.921fb6p-1},

    {INFINITY,
     -INFINITY,
     -NAN,
     0x1.2d97c8p+1},

    {-INFINITY,
     -INFINITY,
     -NAN,
     -0x1.2d97c8p+1},

    {0x1.000000p-1,
     INFINITY,
     0x0.000000p+0,
     0x0.000000p+0},

    {-0x1.000000p-1,
     INFINITY,
     -0x0.000000p+0,
     -0x0.000000p+0},

    {0x1.000000p-1,
     -INFINITY,
     -0x0.000000p+0,
     0x1.921fb6p+1},

    {-0x1.19999ap-1,
     -INFINITY,
     0x0.000000p+0,
     -0x1.921fb6p+1},

    {INFINITY,
     0x1.70a3d8p-4,
     0x1.921fb6p+0,
     0x1.921fb6p+0},

    {-INFINITY,
     -0x1.70a3d8p-4,
     0x1.921fb6p+0,
     -0x1.921fb6p+0},

    {INFINITY,
     0x1.000000p-1,
     0x1.921fb6p+0,
     0x1.921fb6p+0},

    {0x0.000000p+0,
     0x1.000000p-1,
     0x0.000000p+0,
     0x0.000000p+0},

    {0x1.99999ap-2,
     0x1.000000p+0,
     0x1.85a378p-2,
     0x1.85a378p-2},

    {-0x1.333334p-1,
     0x0.000000p+0,
     -0x1.921fb6p+0,
     -0x1.921fb6p+0},

    {-0x1.99999ap-3,
     0x1.99999ap-3,
     -0x1.921fb6p-1,
     -0x1.921fb6p-1},

    {-0x1.99999ap-1,
     -0x1.99999ap-1,
     0x1.921fb6p-1,
     -0x1.2d97c8p+1},
};

MATH_EXP_FLOAT TestExpFloats[] = {
    {INFINITY, INFINITY, INFINITY},
    {-INFINITY, 0x0.000000p+0, -0x1.000000p+0},
    {NAN, NAN, NAN},
    {0x1.630000p+9, INFINITY, INFINITY},
    {0x1.900000p+9, INFINITY, INFINITY},
    {-0x1.630000p+9, 0x0.000000p+0, -0x1.000000p+0},
    {-0x1.900000p+9, 0x0.000000p+0, -0x1.000000p+0},
    {0x1.6353f8p-2, 0x1.6a316ep+0, 0x1.a8c5b8p-2},
    {0x1.0a3d70p+0, 0x1.6a23c8p+1, 0x1.d44790p+0},
    {0x1.400000p+3, 0x1.5829dcp+14, 0x1.5825dcp+14},
    {0x1.900000p+6, INFINITY, INFINITY},
    {-0x1.733334p+1, 0x1.c2c00cp-5, -0x1.e3d400p-1},
    {0x1.921fb6p+1, 0x1.72404ap+4, 0x1.624048p+4},
    {0x1.921fb6p+0, 0x1.33dedep+2, 0x1.e7bdbcp+1},
    {-0x1.921fb6p+1, 0x1.620226p-5, -0x1.e9dfdep-1},
    {-0x1.921fb6p+0, 0x1.a9bcc4p-3, -0x1.9590d0p-1},
};

MATH_POWER_FLOAT TestPowerFloats[] = {
    {0x1.f40000p+9,
     0x0.000000p+0,
     0x1.000000p+0,
     0x1.f40000p+9},

    {INFINITY,
     0x0.000000p+0,
     0x1.000000p+0,
     INFINITY},

    {INFINITY,
     0x1.000000p+0,
     INFINITY,
     INFINITY},

    {0x1.000000p+2,
     NAN,
     NAN,
     NAN},

    {NAN,
     0x1.800000p+1,
     NAN,
     NAN},

    {NAN,
     0x0.000000p+0,
     0x1.000000p+0,
     NAN},

    {0x1.000000p+1,
     INFINITY,
     INFINITY,
     INFINITY},

    {0x1.000000p+1,
     -INFINITY,
     0x0.000000p+0,
     INFINITY},

    {0x1.000000p-1,
     INFINITY,
     0x0.000000p+0,
     INFINITY},

    {0x1.000000p-1,
     -INFINITY,
     INFINITY,
     INFINITY},

    {0x0.000000p+0,
     0x1.400000p+5,
     0x0.000000p+0,
     0x1.400000p+5},

    {-0x0.000000p+0,
     0x1.400000p+5,
     0x0.000000p+0,
     0x1.400000p+5},

    {0x0.000000p+0,
     -0x1.400000p+5,
     INFINITY,
     0x1.400000p+5},

    {-0x0.000000p+0,
     -0x1.400000p+5,
     INFINITY,
     0x1.400000p+5},

    {-0x0.000000p+0,
     -0x1.800000p+1,
     -INFINITY,
     0x1.800000p+1},

    {INFINITY,
     0x1.388000p+12,
     INFINITY,
     INFINITY},

    {INFINITY,
     0x1.800000p+1,
     INFINITY,
     INFINITY},

    {-INFINITY,
     0x1.000000p+2,
     INFINITY,
     INFINITY},

    {-INFINITY,
     -0x1.400000p+2,
     -0x0.000000p+0,
     INFINITY},

    {-0x1.90ccccp+6,
     0x1.800000p+2,
     0x1.d746f4p+39,
     0x1.918496p+6},

    {-0x1.8f0312p+5,
     0x1.833334p+3,
     -NAN,
     0x1.9a9600p+5},

    {-0x1.6b851ep+5,
     -0x1.99999ap-4,
     -NAN,
     0x1.6b8558p+5},

    {0x1.5957aep+8,
     -0x1.333334p-2,
     0x1.62b1e4p-3,
     0x1.5957b6p+8},

    {0x1.e00000p+4,
     0x1.800000p+2,
     0x1.5b9d42p+29,
     0x1.e98180p+4},

    {0x1.000054p+1,
     0x1.638e36p+2,
     0x1.784328p+5,
     0x1.79e4dep+2},

    {INFINITY,
     0x1.333334p-1,
     INFINITY,
     INFINITY},

    {0x1.59fe00p+16,
     0x1.000000p-1,
     0x1.299d24p+8,
     0x1.59fe00p+16},

    {0x1.340000p+6,
     0x1.000000p+1,
     0x1.729000p+12,
     0x1.341a98p+6},

    {-0x1.d40000p+7,
     0x1.800000p+1,
     -0x1.8704d0p+23,
     0x1.d409d8p+7},

    {-0x1.200000p+3,
     0x1.000000p+2,
     0x1.9a1000p+12,
     0x1.3b29d8p+3},

    {0x1.13d73cp-99,
     0x1.99999ap-4,
     0x1.146e04p-10,
     0x1.99999ap-4},

    {0x1.c8571cp-20,
     0x1.c00000p+2,
     0x1.c98c00p-135,
     0x1.c00000p+2},
};

MATH_LOGARITHM_FLOAT TestLogarithmFloats[] = {
    {0x1.921fb6p-1,
     -0x1.eeb958p-3,
     -0x1.64de30p-2,
     -0x1.adb63ap-4},

    {0x1.921fb6p+1,
     0x1.250d04p+0,
     0x1.a6c874p+0,
     0x1.fd14dcp-2},

    {0x1.f6a7a2p+1,
     0x1.5e2cf4p+0,
     0x1.f93254p+0,
     0x1.3028a0p-1},

    {0x1.921fb6p+2,
     0x1.d67f1cp+0,
     0x1.53643ap+1,
     0x1.98ab08p-1},

    {0x1.f6a7a2p+1,
     0x1.5e2cf4p+0,
     0x1.f93254p+0,
     0x1.3028a0p-1},

    {0x1.5fdbbep+2,
     0x1.b45000p+0,
     0x1.3abba2p+1,
     0x1.7af9e8p-1},

    {0x1.000000p-3,
     -0x1.0a2b24p+1,
     -0x1.800000p+1,
     -0x1.ce61d0p-1},

    {0x1.945b6cp-7,
     -0x1.194632p+2,
     -0x1.95cafap+2,
     -0x1.e89f92p+0},

    {0x1.c560c8p-9,
     -0x1.6aac74p+2,
     -0x1.059d1ep+3,
     -0x1.3b03aap+1},

    {0x1.ba35f2p-11,
     -0x1.c4fe1ep+2,
     -0x1.46c3dep+3,
     -0x1.8976e4p+1},

    {0x1.400000p+3,
     0x1.26bb1cp+1,
     0x1.a934f0p+1,
     0x1.000000p+0},

    {0x1.900000p+6,
     0x1.26bb1cp+2,
     0x1.a934f0p+2,
     0x1.000000p+1},

    {0x1.f40000p+9,
     0x1.ba18aap+2,
     0x1.3ee7b4p+3,
     0x1.800000p+1},

    {0x1.86a000p+16,
     0x1.7069e2p+3,
     0x1.09c116p+4,
     0x1.400000p+2},

    {0x1.24f800p+18,
     0x1.9391b8p+3,
     0x1.231d18p+4,
     0x1.5e8928p+2},

    {0x1.2a05f2p+33,
     0x1.7069e2p+4,
     0x1.09c116p+5,
     0x1.400000p+3},

    {0x1.c6bf52p+49,
     0x1.144f6ap+5,
     0x1.8ea1a2p+5,
     0x1.e00000p+3},

    {0x1.5af1d8p+66,
     0x1.7069e2p+5,
     0x1.09c116p+6,
     0x1.400000p+4},

    {0x1.08b2a2p+83,
     0x1.cc845cp+5,
     0x1.4c315cp+6,
     0x1.900000p+4},

    {0x1.93e594p+99,
     0x1.144f6ap+6,
     0x1.8ea1a2p+6,
     0x1.e00000p+4},

    {0x1.342618p+116,
     0x1.425ca6p+6,
     0x1.d111e8p+6,
     0x1.180000p+5},

    {0x1.4f8b58p-17,
     -0x1.7069e2p+3,
     -0x1.09c116p+4,
     -0x1.400000p+2},

    {0x1.b7cdfep-34,
     -0x1.7069e2p+4,
     -0x1.09c116p+5,
     -0x1.400000p+3},

    {0x1.203afap-50,
     -0x1.144f6ap+5,
     -0x1.8ea1a2p+5,
     -0x1.e00000p+3},

    {0x1.79ca10p-67,
     -0x1.7069e2p+5,
     -0x1.09c116p+6,
     -0x1.400000p+4},

    {0x1.cdef9ep+34,
     0x1.82841cp+4,
     0x1.16cffap+5,
     0x1.4fb93cp+3},

    {0x1.6bcc42p+51,
     0x1.1d9d8cp+5,
     0x1.9c0e52p+5,
     0x1.f02a30p+3},

    {0x1.1e3ab8p+68,
     0x1.79f70ap+5,
     0x1.10a4e4p+6,
     0x1.484bd6p+4},

    {0x1.c1fc7cp+84,
     0x1.d64ea6p+5,
     0x1.534144p+6,
     0x1.9880f0p+4},

    {0x1.6168e2p+101,
     0x1.19523ep+6,
     0x1.95dc5cp+6,
     0x1.e8b480p+4},

    {0x1.15557cp+118,
     0x1.477c54p+6,
     0x1.d87640p+6,
     0x1.1c734ep+5},

    {0x1.000000p+0,
     0x0.000000p+0,
     0x0.000000p+0,
     0x0.000000p+0},

    {0x1.000000p+1,
     0x1.62e430p-1,
     0x1.000000p+0,
     0x1.344136p-2},

    {0x1.800000p+1,
     0x1.193ea8p+0,
     0x1.95c01ap+0,
     0x1.e89278p-2},

    {0x1.22ad96p+27,
     0x1.2d78e2p+4,
     0x1.b2eeb6p+4,
     0x1.05dafep+3},

    {0x1.900000p+4,
     0x1.9c0420p+1,
     0x1.2934f0p+2,
     0x1.65df66p+0},

    {0x1.fae148p-1,
     -0x1.495440p-7,
     -0x1.db1f18p-7,
     -0x1.1e0d38p-8},

    {-0x1.000000p+0,
     -INFINITY,
     -INFINITY,
     -INFINITY},

    {INFINITY,
     INFINITY,
     INFINITY,
     INFINITY},

    {-INFINITY,
     -INFINITY,
     -INFINITY,
     -INFINITY},

    {NAN,
     NAN,
     NAN,
     NAN},
};

MATH_DECOMPOSITION_FLOAT TestDecompositionFloats[] = {
    {0x1.921fb6p-1, 0x0.000000p+0, 0x1.921fb6p-1},
    {0x1.921fb6p+1, 0x1.800000p+1, 0x1.21fb60p-3},
    {0x1.f6a7a2p+1, 0x1.800000p+1, 0x1.da9e88p-1},
    {0x1.921fb6p+2, 0x1.800000p+2, 0x1.21fb60p-2},
    {0x1.f6a7a2p+1, 0x1.800000p+1, 0x1.da9e88p-1},
    {0x1.5fdbbep+2, 0x1.400000p+2, 0x1.fdbbe0p-2},
    {0x1.000000p-3, 0x0.000000p+0, 0x1.000000p-3},
    {0x1.945b6cp-7, 0x0.000000p+0, 0x1.945b6cp-7},
    {0x1.c560c8p-9, 0x0.000000p+0, 0x1.c560c8p-9},
    {0x1.ba35f2p-11, 0x0.000000p+0, 0x1.ba35f2p-11},
    {0x1.400000p+3, 0x1.400000p+3, 0x0.000000p+0},
    {0x1.24f800p+18, 0x1.24f800p+18, 0x0.000000p+0},
    {0x1.4f8b58p-17, 0x0.000000p+0, 0x1.4f8b58p-17},
    {0x1.b7cdfep-34, 0x0.000000p+0, 0x1.b7cdfep-34},
    {0x1.203afap-50, 0x0.000000p+0, 0x1.203afap-50},
    {0x1.79ca10p-67, 0x0.000000p+0, 0x1.79ca10p-67},
    {0x1.cdef9ep+34, 0x1.cdef9ep+34, 0x0.000000p+0},
    {0x1.6bcc42p+51, 0x1.6bcc42p+51, 0x0.000000p+0},
    {0x1.1e3ab8p+68, 0x1.1e3ab8p+68, 0x0.000000p+0},
    {0x1.c1fc7cp+84, 0x1.c1fc7cp+84, 0x0.000000p+0},
    {0x1.6168e2p+101, 0x1.6168e2p+101, 0x0.000000p+0},
    {0x1.15557cp+118, 0x1.15557cp+118, 0x0.000000p+0},
    {INFINITY, INFINITY, 0x0.000000p+0},
    {INFINITY, INFINITY, 0x0.000000p+0},
    {INFINITY, INFINITY, 0x0.000000p+0},
    {INFINITY, INFINITY, 0x0.000000p+0},
    {INFINITY, INFINITY, 0x0.000000p+0},
    {0x1.000000p+0, 0x1.000000p+0, 0x0.000000p+0},
    {0x1.000000p+1, 0x1.000000p+1, 0x0.000000p+0},
    {0x1.800000p+1, 0x1.800000p+1, 0x0.000000p+0},
    {0x1.22ad96p+27, 0x1.22ad96p+27, 0x0.000000p+0},
    {0x1.900000p+4, 0x1.900000p+4, 0x0.000000p+0},
    {0x1.fae148p-1, 0x0.000000p+0, 0x1.fae148p-1},
    {-0x1.000000p+0, -0x1.000000p+0, -0x0.000000p+0},
    {-0x1.99999ap-4, -0x0.000000p+0, -0x1.99999ap-4},
    {-0x1.0bb448p+70, -0x1.0bb448p+70, -0x0.000000p+0},
    {0x1.fe9af6p+89, 0x1.fe9af6p+89, 0x0.000000p+0},
    {0x1.122110p+60, 0x1.122110p+60, 0x0.000000p+0},
    {0x1.7c6e3cp+116, 0x1.7c6e3cp+116, 0x0.000000p+0},
    {0x1.b1ae4ep+69, 0x1.b1ae4ep+69, 0x0.000000p+0},
    {0x1.9d971ep+89, 0x1.9d971ep+89, 0x0.000000p+0},
    {0x1.342618p+116, 0x1.342618p+116, 0x0.000000p+0},
    {INFINITY, INFINITY, 0x0.000000p+0},
    {-INFINITY, -INFINITY, -0x0.000000p+0},
    {NAN, NAN, NAN},
};

MATH_CEILING_FLOOR_FLOAT_VALUE TestCeilingFloorFloats[] = {
    {-0x1.100000p+5,
     -0x1.100000p+5,
     -0x1.100000p+5},

    {-0x1.05999ap+5,
     -0x1.000000p+5,
     -0x1.080000p+5},

    {-0x1.f66666p+4,
     -0x1.f00000p+4,
     -0x1.000000p+5},

    {-0x1.e1999ap+4,
     -0x1.e00000p+4,
     -0x1.f00000p+4},

    {-0x1.ccccccp+4,
     -0x1.c00000p+4,
     -0x1.d00000p+4},

    {-0x1.b80000p+4,
     -0x1.b00000p+4,
     -0x1.c00000p+4},

    {-0x1.a33334p+4,
     -0x1.a00000p+4,
     -0x1.b00000p+4},

    {-0x1.8e6666p+4,
     -0x1.800000p+4,
     -0x1.900000p+4},

    {-0x1.79999ap+4,
     -0x1.700000p+4,
     -0x1.800000p+4},

    {-0x1.64ccccp+4,
     -0x1.600000p+4,
     -0x1.700000p+4},

    {-0x1.500000p+4,
     -0x1.500000p+4,
     -0x1.500000p+4},

    {-0x1.3b3334p+4,
     -0x1.300000p+4,
     -0x1.400000p+4},

    {-0x1.266666p+4,
     -0x1.200000p+4,
     -0x1.300000p+4},

    {-0x1.11999ap+4,
     -0x1.100000p+4,
     -0x1.200000p+4},

    {-0x1.f9999ap+3,
     -0x1.e00000p+3,
     -0x1.000000p+4},

    {-0x1.d00000p+3,
     -0x1.c00000p+3,
     -0x1.e00000p+3},

    {-0x1.a66666p+3,
     -0x1.a00000p+3,
     -0x1.c00000p+3},

    {-0x1.7cccccp+3,
     -0x1.600000p+3,
     -0x1.800000p+3},

    {-0x1.533334p+3,
     -0x1.400000p+3,
     -0x1.600000p+3},

    {-0x1.29999ap+3,
     -0x1.200000p+3,
     -0x1.400000p+3},

    {-0x1.000000p+3,
     -0x1.000000p+3,
     -0x1.000000p+3},

    {-0x1.acccccp+2,
     -0x1.800000p+2,
     -0x1.c00000p+2},

    {-0x1.59999ap+2,
     -0x1.400000p+2,
     -0x1.800000p+2},

    {-0x1.066666p+2,
     -0x1.000000p+2,
     -0x1.400000p+2},

    {-0x1.666666p+1,
     -0x1.000000p+1,
     -0x1.800000p+1},

    {-0x1.800000p+0,
     -0x1.000000p+0,
     -0x1.000000p+1},

    {-0x1.99999ap-3,
     -0x0.000000p+0,
     -0x1.000000p+0},

    {0x1.19999ap+0,
     0x1.000000p+1,
     0x1.000000p+0},

    {0x1.333334p+1,
     0x1.800000p+1,
     0x1.000000p+1},

    {0x1.d9999ap+1,
     0x1.000000p+2,
     0x1.800000p+1},

    {0x1.400000p+2,
     0x1.400000p+2,
     0x1.400000p+2},

    {0x1.933334p+2,
     0x1.c00000p+2,
     0x1.800000p+2},

    {0x1.e66666p+2,
     0x1.000000p+3,
     0x1.c00000p+2},

    {0x1.1cccccp+3,
     0x1.200000p+3,
     0x1.000000p+3},

    {0x1.466666p+3,
     0x1.600000p+3,
     0x1.400000p+3},

    {0x1.700000p+3,
     0x1.800000p+3,
     0x1.600000p+3},

    {0x1.99999ap+3,
     0x1.a00000p+3,
     0x1.800000p+3},

    {0x1.c33334p+3,
     0x1.e00000p+3,
     0x1.c00000p+3},

    {0x1.ecccccp+3,
     0x1.000000p+4,
     0x1.e00000p+3},

    {0x1.0b3334p+4,
     0x1.100000p+4,
     0x1.000000p+4},

    {0x1.200000p+4,
     0x1.200000p+4,
     0x1.200000p+4},

    {0x1.34ccccp+4,
     0x1.400000p+4,
     0x1.300000p+4},

    {0x1.49999ap+4,
     0x1.500000p+4,
     0x1.400000p+4},

    {0x1.5e6666p+4,
     0x1.600000p+4,
     0x1.500000p+4},

    {0x1.733334p+4,
     0x1.800000p+4,
     0x1.700000p+4},

    {0x1.880000p+4,
     0x1.900000p+4,
     0x1.800000p+4},

    {0x1.9cccccp+4,
     0x1.a00000p+4,
     0x1.900000p+4},

    {0x1.b1999ap+4,
     0x1.c00000p+4,
     0x1.b00000p+4},

    {0x1.c66666p+4,
     0x1.d00000p+4,
     0x1.c00000p+4},

    {0x1.db3334p+4,
     0x1.e00000p+4,
     0x1.d00000p+4},

    {0x1.f00000p+4,
     0x1.f00000p+4,
     0x1.f00000p+4},

    {0x1.026666p+5,
     0x1.080000p+5,
     0x1.000000p+5},

    {0x1.0cccccp+5,
     0x1.100000p+5,
     0x1.080000p+5},
};

MATH_MODULO_FLOAT_VALUE TestModuloFloats[] = {
    {0x0.000000p+0,
     -0x0.000000p+0,
     -INFINITY},

    {0x0.000000p+0,
     -0x1.000000p+0,
     0x0.000000p+0},

    {-0x0.000000p+0,
     -0x1.000000p+0,
     -0x0.000000p+0},

    {0x1.19999ap+0,
     0x1.842274p-114,
     0x1.5c3558p-115},

    {-0x1.880000p+6,
     0x1.8c0000p+6,
     -0x1.880000p+6},

    {0x1.900000p+6,
     0x1.400000p+3,
     0x0.000000p+0},

    {0x1.400000p+3,
     0x1.99999ap-4,
     0x1.999972p-4},

    {0x1.9d3f42p+110,
     -0x1.151b07p+120,
     0x1.9d3f42p+110},

    {0x1.151b07p+120,
     0x1.000000p+2,
     0x0.000000p+0},

    {0x1.0624dep-10,
     0x1.0624dep-10,
     0x0.000000p+0},

    {0x1.800000p+2,
     0x1.000000p+1,
     0x0.000000p+0},

    {-0x1.921fb6p+1,
     -0x1.000000p+1,
     -0x1.243f6ap+0},

    {INFINITY,
     -INFINITY,
     -INFINITY},

    {NAN,
     INFINITY,
     NAN},

    {NAN,
     NAN,
     NAN},

    {INFINITY,
     NAN,
     NAN},
};

//
// ------------------------------------------------------------------ Functions
//

ULONG
TestMathFloat (
    VOID
    )

/*++

Routine Description:

    This routine implements the entry point for the C library single-precision
    floating point math test.

Arguments:

    None.

Return Value:

    Returns the count of test failures.

--*/

{

    ULONG Failures;

    Failures = 0;
    Failures += TestBasicTrigonometryFloat();
    Failures += TestSquareRootFloat();
    Failures += TestArcTrigonometryFloat();
    Failures += TestExponentiationFloat();
    Failures += TestPowerFloat();
    Failures += TestLogarithmFloat();
    Failures += TestDecompositionFloat();
    Failures += TestCeilingAndFloorFloat();
    Failures += TestModuloFloat();
    return Failures;
}

//
// --------------------------------------------------------- Internal Functions
//

ULONG
TestBasicTrigonometryFloat (
    VOID
    )

/*++

Routine Description:

    This routine test the basic trig routines: sine, cosine, and tangent.

Arguments:

    None.

Return Value:

    Returns the count of test failures.

--*/

{

    volatile float Cosine;
    ULONG Failures;
    volatile float HyperbolicCosine;
    volatile float HyperbolicSine;
    volatile float HyperbolicTangent;
    volatile float Sine;
    volatile float Tangent;
    PMATH_TRIG_FLOAT_VALUE Test;
    ULONG TestIndex;
    ULONG ValueCount;

    Failures = 0;
    ValueCount = sizeof(TestBasicTrigonometryFloats) /
                 sizeof(TestBasicTrigonometryFloats[0]);

    for (TestIndex = 0; TestIndex < ValueCount; TestIndex += 1) {
        Test = &(TestBasicTrigonometryFloats[TestIndex]);
        Sine = sinf(Test->Argument);
        Cosine = cosf(Test->Argument);
        if (TestCompareResultsFloat(Sine, Test->Sine) == FALSE) {
            printf("sinf(%.6a) was %.6a, should have been %.6a. "
                   "Diff %.6a\n",
                   Test->Argument,
                   Sine,
                   Test->Sine,
                   Sine - Test->Sine);

            Failures += 1;
        }

        if (TestCompareResultsFloat(Cosine, Test->Cosine) == FALSE) {
            printf("cosf(%.6a) was %.6a, should have been %.6a.\n",
                   Test->Argument,
                   Cosine,
                   Test->Cosine);

            Failures += 1;
        }

        Tangent = tanf(Test->Argument);
        if (TestCompareResultsFloat(Tangent, Test->Tangent) == FALSE) {
            printf("tanf(%.6a) was %.6a, should have been %.6a.\n",
                   Test->Argument,
                   Tangent,
                   Test->Tangent);

            Failures += 1;
        }

        HyperbolicSine = sinhf(Test->Argument);
        HyperbolicCosine = coshf(Test->Argument);
        HyperbolicTangent = tanhf(Test->Argument);
        if (TestCompareResultsFloat(HyperbolicSine,
                                    Test->HyperbolicSine) == FALSE) {

            printf("sinhf(%.6a) was %.6a, should have been %.6a. "
                   "Diff %.6a\n",
                   Test->Argument,
                   HyperbolicSine,
                   Test->HyperbolicSine,
                   HyperbolicSine - Test->HyperbolicSine);

            Failures += 1;
        }

        if (TestCompareResultsFloat(HyperbolicCosine,
                                    Test->HyperbolicCosine) == FALSE) {

            printf("coshf(%.6a) was %.6a, should have been %.6a.\n",
                   Test->Argument,
                   HyperbolicCosine,
                   Test->HyperbolicCosine);

            Failures += 1;
        }

        if (TestCompareResultsFloat(HyperbolicTangent,
                                    Test->HyperbolicTangent) == FALSE) {

            printf("tanhf(%.6a) was %.6a, should have been %.6a.\n",
                   Test->Argument,
                   HyperbolicTangent,
                   Test->HyperbolicTangent);

            Failures += 1;
        }

    }

    return Failures;
}

ULONG
TestSquareRootFloat (
    VOID
    )

/*++

Routine Description:

    This routine test the square root function.

Arguments:

    None.

Return Value:

    Returns the count of test failures.

--*/

{

    ULONG Failures;
    volatile float SquareRoot;
    PMATH_TEST_SQUARE_ROOT_FLOAT_VALUE Test;
    ULONG TestIndex;
    ULONG ValueCount;

    Failures = 0;
    ValueCount = sizeof(TestSquareRootFloats) /
                 sizeof(TestSquareRootFloats[0]);

    for (TestIndex = 0; TestIndex < ValueCount; TestIndex += 1) {
        Test = &(TestSquareRootFloats[TestIndex]);
        SquareRoot = sqrtf(Test->Argument);
        if (TestCompareResultsFloat(SquareRoot, Test->SquareRoot) == FALSE) {
            printf("sqrt(%.6a) was %.6a, should have been %.6a.\n",
                   Test->Argument,
                   SquareRoot,
                   Test->SquareRoot);

            Failures += 1;
        }
    }

    return Failures;
}

ULONG
TestArcTrigonometryFloat (
    VOID
    )

/*++

Routine Description:

    This routine test the inverse trig routines: arc sine, arc cosine, and
    arc tangent.

Arguments:

    None.

Return Value:

    Returns the count of test failures.

--*/

{

    volatile float ArcCosine;
    volatile float ArcSine;
    volatile float ArcTangent;
    PMATH_ARC_TANGENT_FLOAT ArcTangentTest;
    ULONG Failures;
    float Quotient;
    PMATH_ARC_FLOAT_VALUE Test;
    ULONG TestIndex;
    ULONG ValueCount;

    Failures = 0;
    ValueCount = sizeof(TestArcFloats) / sizeof(TestArcFloats[0]);
    for (TestIndex = 0; TestIndex < ValueCount; TestIndex += 1) {
        Test = &(TestArcFloats[TestIndex]);
        ArcSine = asinf(Test->Argument);
        if (TestCompareResultsFloat(ArcSine, Test->ArcSine) == FALSE) {
            printf("asinf(%.6a) was %.6a, should have been %.6a.\n",
                   Test->Argument,
                   ArcSine,
                   Test->ArcSine);

            Failures += 1;
        }

        ArcCosine = acosf(Test->Argument);
        if (TestCompareResultsFloat(ArcCosine, Test->ArcCosine) == FALSE) {
            printf("acosf(%.6a) was %.6a, should have been %.6a.\n",
                   Test->Argument,
                   ArcCosine,
                   Test->ArcCosine);

            Failures += 1;
        }

        ArcTangent = atanf(Test->Argument);
        if (TestCompareResultsFloat(ArcTangent, Test->ArcTangent) == FALSE) {
            printf("atanf(%.6a) was %.6a, should have been %.6a.\n",
                   Test->Argument,
                   ArcTangent,
                   Test->ArcTangent);

            Failures += 1;
        }
    }

    //
    // Test arc tangent 2.
    //

    ValueCount = sizeof(TestArcTangentFloats) / sizeof(TestArcTangentFloats[0]);
    for (TestIndex = 0; TestIndex < ValueCount; TestIndex += 1) {
        ArcTangentTest = &(TestArcTangentFloats[TestIndex]);
        Quotient = ArcTangentTest->Numerator /
                   ArcTangentTest->Denominator;

        ArcTangent = atanf(Quotient);
        if (TestCompareResultsFloat(ArcTangent,
                                    ArcTangentTest->ArcTangent) == FALSE) {

            printf("atanf(%.6a) was %.6a, should have been %.6a.\n",
                   Quotient,
                   ArcTangent,
                   ArcTangentTest->ArcTangent);

            Failures += 1;
        }

        ArcTangent = atan2f(ArcTangentTest->Numerator,
                            ArcTangentTest->Denominator);

        if (TestCompareResultsFloat(ArcTangent,
                                    ArcTangentTest->ArcTangent2) == FALSE) {

            printf("atan2f(%.6a, %.6a) was %.6a, should have been %.6a.\n",
                   ArcTangentTest->Numerator,
                   ArcTangentTest->Denominator,
                   ArcTangent,
                   ArcTangentTest->ArcTangent2);

            Failures += 1;
        }
    }

    return Failures;
}

ULONG
TestExponentiationFloat (
    VOID
    )

/*++

Routine Description:

    This routine test the exponentiation (exp) function.

Arguments:

    None.

Return Value:

    Returns the count of test failures.

--*/

{

    BOOL Equal;
    volatile float Exponentiation;
    volatile float ExponentiationMinusOne;
    ULONG Failures;
    PMATH_EXP_FLOAT Test;
    ULONG TestIndex;
    ULONG ValueCount;

    Failures = 0;
    ValueCount = sizeof(TestExpFloats) / sizeof(TestExpFloats[0]);
    for (TestIndex = 0; TestIndex < ValueCount; TestIndex += 1) {
        Test = &(TestExpFloats[TestIndex]);
        Exponentiation = expf(Test->Argument);
        if (TestCompareResultsFloat(Exponentiation,
                                    Test->Exponentiation) == FALSE) {

            printf("expf(%.6a) was %.6a, should have been %.6a.\n",
                   Test->Argument,
                   Exponentiation,
                   Test->Exponentiation);

            Failures += 1;
        }

        ExponentiationMinusOne = expm1f(Test->Argument);
        Equal = TestCompareResultsFloat(ExponentiationMinusOne,
                                        Test->ExponentiationMinusOne);

        if (Equal == FALSE) {
            printf("expm1f(%.6a) was %.6a, should have been %.6a.\n",
                   Test->Argument,
                   ExponentiationMinusOne,
                   Test->ExponentiationMinusOne);

            Failures += 1;
        }
    }

    return Failures;
}

ULONG
TestPowerFloat (
    VOID
    )

/*++

Routine Description:

    This routine test the power (pow) function.

Arguments:

    None.

Return Value:

    Returns the count of test failures.

--*/

{

    ULONG Failures;
    volatile float Hypotenuse;
    volatile float RaisedValue;
    PMATH_POWER_FLOAT Test;
    ULONG TestIndex;
    ULONG ValueCount;

    Failures = 0;
    ValueCount = sizeof(TestPowerFloats) / sizeof(TestPowerFloats[0]);
    for (TestIndex = 0; TestIndex < ValueCount; TestIndex += 1) {
        Test = &(TestPowerFloats[TestIndex]);
        RaisedValue = powf(Test->Value, Test->Exponent);
        Hypotenuse = hypotf(Test->Value, Test->Exponent);
        if (TestCompareResultsFloat(RaisedValue, Test->Result) == FALSE) {
            printf("powf(%.6a, %.6a) was %.6a, should have been %.6a.\n",
                   Test->Value,
                   Test->Exponent,
                   RaisedValue,
                   Test->Result);

            Failures += 1;
        }

        if (TestCompareResultsFloat(Hypotenuse, Test->Hypotenuse) == FALSE) {
            printf("hypotf(%.6a, %.6a) was %.6a, should have been %.6a.\n",
                   Test->Value,
                   Test->Exponent,
                   Hypotenuse,
                   Test->Hypotenuse);

            Failures += 1;
        }
    }

    return Failures;
}

ULONG
TestLogarithmFloat (
    VOID
    )

/*++

Routine Description:

    This routine test the logarithm (log) function.

Arguments:

    None.

Return Value:

    Returns the count of test failures.

--*/

{

    ULONG Failures;
    volatile float Logarithm;
    PMATH_LOGARITHM_FLOAT Test;
    ULONG TestIndex;
    ULONG ValueCount;

    Failures = 0;
    ValueCount = sizeof(TestLogarithmFloats) / sizeof(TestLogarithmFloats[0]);
    for (TestIndex = 0; TestIndex < ValueCount; TestIndex += 1) {
        Test = &(TestLogarithmFloats[TestIndex]);
        Logarithm = logf(Test->Argument);
        if (TestCompareResultsFloat(Logarithm, Test->Logarithm) == FALSE) {
            printf("logf(%.6a) was %.6a, should have been %.6a.\n",
                   Test->Argument,
                   Logarithm,
                   Test->Logarithm);

            Failures += 1;
        }

        Logarithm = log2f(Test->Argument);
        if (TestCompareResultsFloat(Logarithm, Test->Log2) == FALSE) {
            printf("log2f(%.6a) was %.6a, should have been %.6a.\n",
                   Test->Argument,
                   Logarithm,
                   Test->Log2);

            Failures += 1;
        }

        Logarithm = log10f(Test->Argument);
        if (TestCompareResultsFloat(Logarithm, Test->Log10) == FALSE) {
            printf("log10f(%.6a) was %.6a, should have been %.6a.\n",
                   Test->Argument,
                   Logarithm,
                   Test->Log10);

            Failures += 1;
        }
    }

    return Failures;
}

ULONG
TestDecompositionFloat (
    VOID
    )

/*++

Routine Description:

    This routine test the logarithm (log) function.

Arguments:

    None.

Return Value:

    Returns the count of test failures.

--*/

{

    ULONG Failures;
    BOOL FractionEqual;
    float FractionalPart;
    BOOL IntegerEqual;
    float IntegerPart;
    PMATH_DECOMPOSITION_FLOAT Test;
    ULONG TestIndex;
    ULONG ValueCount;

    Failures = 0;
    ValueCount = sizeof(TestDecompositionFloats) /
                 sizeof(TestDecompositionFloats[0]);

    for (TestIndex = 0; TestIndex < ValueCount; TestIndex += 1) {
        Test = &(TestDecompositionFloats[TestIndex]);
        FractionalPart = modff(Test->Argument, &IntegerPart);
        IntegerEqual = TestCompareResultsFloat(IntegerPart, Test->IntegerPart);
        FractionEqual = TestCompareResultsFloat(FractionalPart,
                                                Test->FractionalPart);

        if ((IntegerEqual == FALSE) || (FractionEqual == FALSE)) {
            printf("modff(%.6a) was {%.6a, %.6a} should have been "
                   "{%.6a, %.6a}.\n",
                   Test->Argument,
                   IntegerPart,
                   FractionalPart,
                   Test->IntegerPart,
                   Test->FractionalPart);

            Failures += 1;
        }
    }

    return Failures;
}

ULONG
TestCeilingAndFloorFloat (
    VOID
    )

/*++

Routine Description:

    This routine tests the ceiling (ceil) and floor functions.

Arguments:

    None.

Return Value:

    Returns the count of test failures.

--*/

{

    volatile float Ceiling;
    ULONG Failures;
    volatile float Floor;
    PMATH_CEILING_FLOOR_FLOAT_VALUE Test;
    ULONG TestIndex;
    ULONG ValueCount;

    Failures = 0;
    ValueCount = sizeof(TestCeilingFloorFloats) /
                 sizeof(TestCeilingFloorFloats[0]);

    for (TestIndex = 0; TestIndex < ValueCount; TestIndex += 1) {
        Test = &(TestCeilingFloorFloats[TestIndex]);
        Ceiling = ceilf(Test->Argument);
        Floor = floorf(Test->Argument);
        if (TestCompareResultsFloat(Ceiling, Test->Ceiling) == FALSE) {
            printf("ceilf(%.6a) was %.6a, should have been %.6a.\n",
                   Test->Argument,
                   Ceiling,
                   Test->Ceiling);

            Failures += 1;
        }

        if (TestCompareResultsFloat(Floor, Test->Floor) == FALSE) {
            printf("floorf(%.6a) was %.6a, should have been %.6a.\n",
                   Test->Argument,
                   Floor,
                   Test->Floor);

            Failures += 1;
        }
    }

    return Failures;
}

ULONG
TestModuloFloat (
    VOID
    )

/*++

Routine Description:

    This routine tests the module (fmod) function.

Arguments:

    None.

Return Value:

    Returns the count of test failures.

--*/

{

    ULONG Failures;
    volatile float Remainder;
    PMATH_MODULO_FLOAT_VALUE Test;
    ULONG TestIndex;
    ULONG ValueCount;

    Failures = 0;
    ValueCount = sizeof(TestModuloFloats) / sizeof(TestModuloFloats[0]);
    for (TestIndex = 0; TestIndex < ValueCount; TestIndex += 1) {
        Test = &(TestModuloFloats[TestIndex]);
        Remainder = fmodf(Test->Numerator, Test->Denominator);
        if (TestCompareResultsFloat(Remainder, Test->Remainder) == FALSE) {
            printf("fmodf(%.6a, %.6a) was %.6a, should have been %.6a.\n",
                   Test->Numerator,
                   Test->Denominator,
                   Remainder,
                   Test->Remainder);

            Failures += 1;
        }
    }

    return Failures;
}

BOOL
TestCompareResultsFloat (
    float Value1,
    float Value2
    )

/*++

Routine Description:

    This routine compares two float values, and returns whether or not they
    are almost equal.

Arguments:

    Value1 - Supplies the first value.

    Value2 - Supplies the second value.

Return Value:

    TRUE if the values are nearly equal.

    FALSE if they are not equal.

--*/

{

    LONG Word1;
    LONG Word2;
    LONG LowDifference;
    FLOAT_PARTS Parts1;
    FLOAT_PARTS Parts2;

    Parts1.Float = Value1;
    Parts2.Float = Value2;
    Word1 = Parts1.Ulong;
    Word2 = Parts2.Ulong;
    if (((Word1 ^ Word2) & FLOAT_SIGN_BIT) != 0) {
        return FALSE;
    }

    if ((Word1 & ~FLOAT_SIGN_BIT) >= FLOAT_NAN) {
        if ((Word2 & ~FLOAT_SIGN_BIT) >= FLOAT_NAN) {
            return TRUE;
        }

        return FALSE;
    }

    //
    // Check the low twelve bits and allow for somem slop.
    //

    Word1 &= ~FLOAT_TRUNCATE_VALUE_MASK;
    Word2 &= ~FLOAT_TRUNCATE_VALUE_MASK;
    LowDifference = Word1 - Word2;
    if (LowDifference < 0) {
        LowDifference = -LowDifference;
    }

    if (LowDifference < MATH_RESULT_SLOP) {
        return TRUE;
    }

    return FALSE;
}

