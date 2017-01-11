/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    mathtst.c

Abstract:

    This module implements the tests for the math portion of the C library.

Author:

    Evan Green 23-Jul-2013

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

typedef struct _MATH_TRIG_DOUBLE_VALUE {
    double Argument;
    double Sine;
    double Cosine;
    double Tangent;
    double HyperbolicSine;
    double HyperbolicCosine;
    double HyperbolicTangent;
} MATH_TRIG_DOUBLE_VALUE, *PMATH_TRIG_DOUBLE_VALUE;

typedef struct _MATH_TEST_SQUARE_ROOT_DOUBLE_VALUE {
    double Argument;
    double SquareRoot;
} MATH_TEST_SQUARE_ROOT_DOUBLE_VALUE, *PMATH_TEST_SQUARE_ROOT_DOUBLE_VALUE;

typedef struct _MATH_ARC_DOUBLE_VALUE {
    double Argument;
    double ArcSine;
    double ArcCosine;
    double ArcTangent;
} MATH_ARC_DOUBLE_VALUE, *PMATH_ARC_DOUBLE_VALUE;

typedef struct _MATH_ARC_TANGENT_DOUBLE {
    double Numerator;
    double Denominator;
    double ArcTangent;
    double ArcTangent2;
} MATH_ARC_TANGENT_DOUBLE, *PMATH_ARC_TANGENT_DOUBLE;

typedef struct _MATH_EXP_DOUBLE {
    double Argument;
    double Exponentiation;
    double ExponentiationMinusOne;
} MATH_EXP_DOUBLE, *PMATH_EXP_DOUBLE;

typedef struct _MATH_POWER_DOUBLE {
    double Value;
    double Exponent;
    double Result;
    double Hypotenuse;
} MATH_POWER_DOUBLE, *PMATH_POWER_DOUBLE;

typedef struct _MATH_LOGARITHM_DOUBLE {
    double Argument;
    double Logarithm;
    double Log2;
    double Log10;
} MATH_LOGARITHM_DOUBLE, *PMATH_LOGARITHM_DOUBLE;

typedef struct _MATH_DECOMPOSITION_DOUBLE {
    double Argument;
    double IntegerPart;
    double FractionalPart;
} MATH_DECOMPOSITION_DOUBLE, *PMATH_DECOMPOSITION_DOUBLE;

typedef struct _MATH_CEILING_FLOOR_DOUBLE_VALUE {
    double Argument;
    double Ceiling;
    double Floor;
} MATH_CEILING_FLOOR_DOUBLE_VALUE, *PMATH_CEILING_FLOOR_DOUBLE_VALUE;

typedef struct _MATH_MODULO_DOUBLE_VALUE {
    double Numerator;
    double Denominator;
    double Remainder;
} MATH_MODULO_DOUBLE_VALUE, *PMATH_MODULO_DOUBLE_VALUE;

//
// ----------------------------------------------- Internal Function Prototypes
//

ULONG
TestBasicTrigonometry (
    VOID
    );

ULONG
TestSquareRoot (
    VOID
    );

ULONG
TestArcTrigonometry (
    VOID
    );

ULONG
TestExponentiation (
    VOID
    );

ULONG
TestPower (
    VOID
    );

ULONG
TestLogarithm (
    VOID
    );

ULONG
TestDecomposition (
    VOID
    );

ULONG
TestCeilingAndFloor (
    VOID
    );

ULONG
TestModulo (
    VOID
    );

BOOL
TestCompareResults (
    double Value1,
    double Value2
    );

//
// -------------------------------------------------------------------- Globals
//

MATH_TRIG_DOUBLE_VALUE TestBasicTrigonometryDoubles[] = {
    {-0x1.921fb54442d18p-1,
     -0x1.6a09e667f3bccp-1,
     0x1.6a09e667f3bcdp-1,
     -0x1.fffffffffffffp-1,
     -0x1.bcc270b522737p-1,
     0x1.531994ce525b9p+0,
     -0x1.4fc441fa6d6d6p-1},

    {0x1.921fb54442d18p-1,
     0x1.6a09e667f3bccp-1,
     0x1.6a09e667f3bcdp-1,
     0x1.fffffffffffffp-1,
     0x1.bcc270b522737p-1,
     0x1.531994ce525b9p+0,
     0x1.4fc441fa6d6d6p-1},

    {0x1.921fb54442d18p+1,
     0x1.1a62633145c07p-53,
     -0x1.0000000000000p+0,
     -0x1.1a62633145c07p-53,
     0x1.718f45d72e672p+3,
     0x1.72f147fee4000p+3,
     0x1.fe175fa292810p-1},

    {-0x1.921fb54442d18p+1,
     -0x1.1a62633145c07p-53,
     -0x1.0000000000000p+0,
     0x1.1a62633145c07p-53,
     -0x1.718f45d72e672p+3,
     0x1.72f147fee4000p+3,
     -0x1.fe175fa292810p-1},

    {0x1.f6a7a2955385ep+1,
     -0x1.6a09e667f3bccp-1,
     -0x1.6a09e667f3bcep-1,
     0x1.ffffffffffffdp-1,
     0x1.95dfe166e2a93p+4,
     0x1.9630955c947f5p+4,
     0x1.ff9a463bc5610p-1},

    {-0x1.f6a7a2955385ep+1,
     0x1.6a09e667f3bccp-1,
     -0x1.6a09e667f3bcep-1,
     -0x1.ffffffffffffdp-1,
     -0x1.95dfe166e2a93p+4,
     0x1.9630955c947f5p+4,
     -0x1.ff9a463bc5610p-1},

    {0x1.921fb54442d18p+2,
     -0x1.1a62633145c07p-52,
     0x1.0000000000000p+0,
     -0x1.1a62633145c07p-52,
     0x1.0bbeb1603926ap+8,
     0x1.0bbf2bc2b69c6p+8,
     0x1.ffff15f81f9abp-1},

    {-0x1.921fb54442d18p+2,
     0x1.1a62633145c07p-52,
     0x1.0000000000000p+0,
     0x1.1a62633145c07p-52,
     -0x1.0bbeb1603926ap+8,
     0x1.0bbf2bc2b69c6p+8,
     -0x1.ffff15f81f9abp-1},

    {0x1.f6a7a2955385ep+1,
     -0x1.6a09e667f3bccp-1,
     -0x1.6a09e667f3bcep-1,
     0x1.ffffffffffffdp-1,
     0x1.95dfe166e2a93p+4,
     0x1.9630955c947f5p+4,
     0x1.ff9a463bc5610p-1},

    {-0x1.f6a7a2955385ep+1,
     0x1.6a09e667f3bccp-1,
     -0x1.6a09e667f3bcep-1,
     -0x1.ffffffffffffdp-1,
     -0x1.95dfe166e2a93p+4,
     0x1.9630955c947f5p+4,
     -0x1.ff9a463bc5610p-1},

    {0x1.5fdbbe9bba775p+2,
     -0x1.6a09e667f3bcep-1,
     0x1.6a09e667f3bcbp-1,
     -0x1.0000000000002p+0,
     0x1.e84b3f43319a6p+6,
     0x1.e84f70f559672p+6,
     0x1.fffb9a371a4ddp-1},

    {-0x1.5fdbbe9bba775p+2,
     0x1.6a09e667f3bcep-1,
     0x1.6a09e667f3bcbp-1,
     0x1.0000000000002p+0,
     -0x1.e84b3f43319a6p+6,
     0x1.e84f70f559672p+6,
     -0x1.fffb9a371a4ddp-1},

    {0x1.86a0000000000p+16,
     0x1.24daa9c527e96p-5,
     -0x1.ffac3841b3da7p-1,
     -0x1.250a9d503313dp-5,
     INFINITY,
     INFINITY,
     0x1.0000000000000p+0},

    {0x1.2a05f20000000p+33,
     -0x1.f334c7896a4e3p-2,
     0x1.bf098901c931ap-1,
     -0x1.1de000f443f50p-1,
     INFINITY,
     INFINITY,
     0x1.0000000000000p+0},

    {0x1.c6bf526340000p+49,
     0x1.b76f88136cebap-1,
     -0x1.06c154609d33fp-1,
     -0x1.ac23600a95be4p+0,
     INFINITY,
     INFINITY,
     0x1.0000000000000p+0},

    {0x1.5af1d78b58c40p+66,
     -0x1.4a5e605fd6450p-1,
     0x1.872720fc60d3dp-1,
     -0x1.b06fbbe995394p-1,
     INFINITY,
     INFINITY,
     0x1.0000000000000p+0},

    {0x1.08b2a2c280291p+83,
     -0x1.38958031b7978p-2,
     0x1.e78fe68c89fadp-1,
     -0x1.4840633f93616p-2,
     INFINITY,
     INFINITY,
     0x1.0000000000000p+0},

    {0x1.93e5939a08ceap+99,
     0x1.31c608f107767p-7,
     -0x1.fffa4b11f1b45p-1,
     -0x1.31c97177a2330p-7,
     INFINITY,
     INFINITY,
     0x1.0000000000000p+0},

    {0x1.3426172c74d82p+116,
     0x1.b116c776f2d43p-1,
     0x1.1116f09af4b68p-1,
     0x1.95fc9f95b7229p+0,
     INFINITY,
     INFINITY,
     0x1.0000000000000p+0},

    {0x1.d6329f1c35ca5p+132,
     0x1.4b27597db33cep-1,
     -0x1.867d0a5330fd0p-1,
     -0x1.b2339b1756c72p-1,
     INFINITY,
     INFINITY,
     0x1.0000000000000p+0},

    {0x1.11b0ec57e649ap+166,
     -0x1.ec083ac81db28p-2,
     0x1.c105714551003p-1,
     -0x1.1885916b6549dp-1,
     INFINITY,
     INFINITY,
     0x1.0000000000000p+0},

    {0x1.1afd6ec0e1411p+249,
     0x1.53a921e12790ep-1,
     -0x1.7f1c9cda5213dp-1,
     -0x1.c5ee4e954e803p-1,
     INFINITY,
     INFINITY,
     0x1.0000000000000p+0},

    {0x1.249ad2594c37dp+332,
     -0x1.85c5e5b929359p-2,
     0x1.d9757496841f5p-1,
     -0x1.a5807d6f76f7dp-2,
     INFINITY,
     INFINITY,
     0x1.0000000000000p+0},

    {0x1.38d352e5096afp+498,
     0x1.619a65c3554bdp-1,
     -0x1.724837ff05667p-1,
     -0x1.e8effdfae250dp-1,
     INFINITY,
     INFINITY,
     0x1.0000000000000p+0},

    {-0x1.24f8000000000p+18,
     -0x1.b6885f8d1df45p-4,
     -0x1.fd0e9ec921065p-1,
     0x1.b911615bdd6c4p-4,
     -INFINITY,
     INFINITY,
     -0x1.0000000000000p+0},

    {-0x1.550f7dca70000p+51,
     -0x1.77f46d55870efp-5,
     0x1.ff75e5df884a4p-1,
     -0x1.7859f0c7dbc4ep-5,
     -INFINITY,
     INFINITY,
     -0x1.0000000000000p+0},

    {-0x1.8d0bf423c03d9p+84,
     -0x1.8ed03bfc145a6p-2,
     0x1.d792a723383ecp-1,
     -0x1.b100c5350e6fcp-2,
     -INFINITY,
     INFINITY,
     -0x1.0000000000000p+0},

    {-0x1.9a896283d96e6p+167,
     -0x1.696b9d610b33fp-3,
     0x1.f7f6d890ee3a7p-1,
     -0x1.6f2ee9a26a6ecp-3,
     -INFINITY,
     INFINITY,
     -0x1.0000000000000p+0},

    {-0x1.a87c262151e1ap+250,
     -0x1.2fd0478111051p-1,
     0x1.9c1e39b28692fp-1,
     -0x1.797253994412fp-1,
     -INFINITY,
     INFINITY,
     -0x1.0000000000000p+0},

    {-0x1.b6e83b85f253bp+333,
     -0x1.ff8d25c220595p-1,
     -0x1.56ddd3281e346p-5,
     0x1.7df2aad5eaf57p+4,
     -INFINITY,
     INFINITY,
     -0x1.0000000000000p+0},

    {-0x1.d53cfc578e207p+499,
     -0x1.a6f0516b9968dp-2,
     0x1.d249c8da1556cp-1,
     -0x1.d0669cb032948p-2,
     -INFINITY,
     INFINITY,
     -0x1.0000000000000p+0},

    {0x1.4f8b588e368f1p-17,
     0x1.4f8b588e1e8a2p-17,
     0x1.ffffffff920c8p-1,
     0x1.4f8b588e6698ep-17,
     0x1.4f8b588e4e940p-17,
     0x1.0000000036f9cp+0,
     0x1.4f8b588e06854p-17},

    {0x1.b7cdfd9d7bdbbp-34,
     0x1.b7cdfd9d7bdbbp-34,
     0x1.0000000000000p+0,
     0x1.b7cdfd9d7bdbbp-34,
     0x1.b7cdfd9d7bdbbp-34,
     0x1.0000000000000p+0,
     0x1.b7cdfd9d7bdbbp-34},

    {0x1.203af9ee75616p-50,
     0x1.203af9ee75616p-50,
     0x1.0000000000000p+0,
     0x1.203af9ee75616p-50,
     0x1.203af9ee75616p-50,
     0x1.0000000000000p+0,
     0x1.203af9ee75616p-50},

    {0x1.79ca10c924223p-67,
     0x1.79ca10c924223p-67,
     0x1.0000000000000p+0,
     0x1.79ca10c924223p-67,
     0x1.79ca10c924223p-67,
     0x1.0000000000000p+0,
     0x1.79ca10c924223p-67},

    {-0x1.f75104d551d69p-16,
     -0x1.f75104d40d943p-16,
     0x1.fffffffc22708p-1,
     -0x1.f75104d7da5b4p-16,
     -0x1.f75104d69618ep-16,
     0x1.00000001eec7cp+0,
     -0x1.f75104d2c951ep-16},

    {-0x1.07e1fe91b0b70p-35,
     -0x1.07e1fe91b0b70p-35,
     0x1.0000000000000p+0,
     -0x1.07e1fe91b0b70p-35,
     -0x1.07e1fe91b0b70p-35,
     0x1.0000000000000p+0,
     -0x1.07e1fe91b0b70p-35},

    {-0x1.59e05f1e2674dp-52,
     -0x1.59e05f1e2674dp-52,
     0x1.0000000000000p+0,
     -0x1.59e05f1e2674dp-52,
     -0x1.59e05f1e2674dp-52,
     0x1.0000000000000p+0,
     -0x1.59e05f1e2674dp-52},

    {-0x1.1b578c96db19bp-65,
     -0x1.1b578c96db19bp-65,
     0x1.0000000000000p+0,
     -0x1.1b578c96db19bp-65,
     -0x1.1b578c96db19bp-65,
     0x1.0000000000000p+0,
     -0x1.1b578c96db19bp-65},

    {-0x1.672dbb81f8a17p-165,
     -0x1.672dbb81f8a17p-165,
     0x1.0000000000000p+0,
     -0x1.672dbb81f8a17p-165,
     -0x1.672dbb81f8a17p-165,
     0x1.0000000000000p+0,
     -0x1.672dbb81f8a17p-165},

    {-0x1.5b605314ad5c2p-248,
     -0x1.5b605314ad5c2p-248,
     0x1.0000000000000p+0,
     -0x1.5b605314ad5c2p-248,
     -0x1.5b605314ad5c2p-248,
     0x1.0000000000000p+0,
     -0x1.5b605314ad5c2p-248},

    {INFINITY,
     -INFINITY,
     -INFINITY,
     -INFINITY,
     INFINITY,
     INFINITY,
     0x1.0000000000000p+0},

    {-INFINITY,
     -INFINITY,
     -INFINITY,
     -INFINITY,
     -INFINITY,
     INFINITY,
     -0x1.0000000000000p+0},

    {NAN,
     NAN,
     NAN,
     NAN,
     NAN,
     NAN,
     NAN},
};

MATH_TEST_SQUARE_ROOT_DOUBLE_VALUE TestSquareRootDoubles[] = {
    {0x1.921fb54442d18p-1, 0x1.c5bf891b4ef6ap-1},
    {0x1.921fb54442d18p+1, 0x1.c5bf891b4ef6ap+0},
    {0x1.f6a7a2955385ep+1, 0x1.fb4e4f1347eb9p+0},
    {0x1.921fb54442d18p+2, 0x1.40d931ff62705p+1},
    {0x1.f6a7a2955385ep+1, 0x1.fb4e4f1347eb9p+0},
    {0x1.5fdbbe9bba775p+2, 0x1.2c204f9612dc7p+1},
    {0x1.86a0000000000p+16, 0x1.3c3a4edfa9759p+8},
    {0x1.2a05f20000000p+33, 0x1.86a0000000000p+16},
    {0x1.c6bf526340000p+49, 0x1.e286789a07f2fp+24},
    {0x1.5af1d78b58c40p+66, 0x1.2a05f20000000p+33},
    {0x1.08b2a2c280291p+83, 0x1.702337e304309p+41},
    {0x1.93e5939a08ceap+99, 0x1.c6bf526340000p+49},
    {0x1.3426172c74d82p+116, 0x1.18ddde9363225p+58},
    {0x1.d6329f1c35ca5p+132, 0x1.5af1d78b58c40p+66},
    {0x1.11b0ec57e649ap+166, 0x1.08b2a2c280291p+83},
    {0x1.1afd6ec0e1411p+249, 0x1.7ca5342cf485ap+124},
    {0x1.249ad2594c37dp+332, 0x1.11b0ec57e649ap+166},
    {0x1.38d352e5096afp+498, 0x1.1afd6ec0e1411p+249},
    {0x1.4f8b588e368f1p-17, 0x1.9e7c6e43390b7p-9},
    {0x1.b7cdfd9d7bdbbp-34, 0x1.4f8b588e368f1p-17},
    {0x1.203af9ee75616p-50, 0x1.0fa3389d6eb40p-25},
    {0x1.79ca10c924223p-67, 0x1.b7cdfd9d7bdbbp-34},
    {0x1.0000000000000p+0, 0x1.0000000000000p+0},
    {0x1.0000000000000p+1, 0x1.6a09e667f3bcdp+0},
    {0x1.8000000000000p+1, 0x1.bb67ae8584caap+0},
    {0x1.22ad962000000p+27, 0x1.81c8000000000p+13},
    {0x1.9000000000000p+4, 0x1.4000000000000p+2},
    {0x1.fae147ae147aep-1, 0x1.fd6efe4c9b8a5p-1},
    {-0x1.0000000000000p+0, -INFINITY},
    {INFINITY, INFINITY},
    {-INFINITY, -INFINITY},
    {NAN, NAN},
};

MATH_ARC_DOUBLE_VALUE TestArcDoubles[] = {
    {-0x0.0000000000000p+0,
     -0x0.0000000000000p+0,
     0x1.921fb54442d18p+0,
     -0x0.0000000000000p+0},

    {-0x1.47ae147ae147bp-7,
     -0x1.47af7a68f8eefp-7,
     0x1.94af143914c36p+0,
     -0x1.47ab48b1efb5dp-7},

    {-0x1.47ae147ae147bp-6,
     -0x1.47b3ac64be09ap-6,
     0x1.973e83f5d5c9bp+0,
     -0x1.47a2e5dafff07p-6},

    {-0x1.eb851eb851eb8p-6,
     -0x1.eb98008422806p-6,
     0x1.99ce1546535b8p+0,
     -0x1.eb5f644234b83p-6},

    {-0x1.47ae147ae147bp-5,
     -0x1.47c4773aebd59p-5,
     0x1.9c5dd8fe1a303p+0,
     -0x1.4781623768d56p-5},

    {-0x1.999999999999ap-5,
     -0x1.99c5570328659p-5,
     0x1.9eeddffc5c14bp+0,
     -0x1.9942597929f27p-5},

    {-0x1.eb851eb851eb8p-5,
     -0x1.ebd0bd734d8a7p-5,
     0x1.a17e3b2fdd3ddp+0,
     -0x1.eaee734b5dd23p-5},

    {-0x1.1eb851eb851ecp-4,
     -0x1.1ef4656a8d28cp-4,
     0x1.a40efb9aeba41p+0,
     -0x1.1e40c8b780fdep-4},

    {-0x1.47ae147ae147bp-4,
     -0x1.4807d1320318cp-4,
     0x1.a6a0325763031p+0,
     -0x1.46fbce9dfcc00p-4},

    {-0x1.70a3d70a3d70ap-4,
     -0x1.7123b567d5638p-4,
     0x1.a931f09ac027cp+0,
     -0x1.6fa6446b1cb54p-4},

    {-0x1.999999999999ap-4,
     -0x1.9a49276037884p-4,
     0x1.abc447ba464a1p+0,
     -0x1.983e282e2cc4dp-4},

    {-0x1.c28f5c28f5c29p-4,
     -0x1.c3793eaf67675p-4,
     0x1.ae57492f39480p+0,
     -0x1.c0c17d875967ap-4},

    {-0x1.eb851eb851eb8p-4,
     -0x1.ecb5156ece085p-4,
     0x1.b0eb069b2fb21p+0,
     -0x1.e92e4e371f679p-4},

    {-0x1.0a3d70a3d70a4p-3,
     -0x1.0afee441e736fp-3,
     0x1.b37f91cc7fb86p+0,
     -0x1.08c155549dc84p-3},

    {-0x1.1eb851eb851ecp-3,
     -0x1.1faa3bf43ad3bp-3,
     0x1.b614fcc2ca2c0p+0,
     -0x1.1cde553d5fe66p-3},

    {-0x1.3333333333333p-3,
     -0x1.345d237b20eb3p-3,
     0x1.b8ab59b3a6eefp+0,
     -0x1.30ed367d7cd04p-3},

    {-0x1.47ae147ae147bp-3,
     -0x1.49182e599c592p-3,
     0x1.bb42bb0f765cbp+0,
     -0x1.44ed0cd36e62ep-3},

    {-0x1.5c28f5c28f5c3p-3,
     -0x1.5ddbf210c49e2p-3,
     0x1.bddb33865b654p+0,
     -0x1.58dcf04f55e58p-3},

    {-0x1.70a3d70a3d70ap-3,
     -0x1.72a90648fbf25p-3,
     0x1.c074d60d624fdp+0,
     -0x1.6cbbfd8acff50p-3},

    {-0x1.851eb851eb852p-3,
     -0x1.878004fcac282p-3,
     0x1.c30fb5e3d8568p+0,
     -0x1.808955ddac693p-3},

    {-0x1.999999999999ap-3,
     -0x1.9c618aa4ae23dp-3,
     0x1.c5abe698d8960p+0,
     -0x1.94441f8f7260cp-3},

    {-0x1.ae147ae147ae1p-3,
     -0x1.b14e3666821f8p-3,
     0x1.c8497c1113157p+0,
     -0x1.a7eb86059c7dap-3},

    {-0x1.c28f5c28f5c29p-3,
     -0x1.c646aa44819dap-3,
     0x1.cae88a8cd3053p+0,
     -0x1.bb7eb9ee7d317p-3},

    {-0x1.d70a3d70a3d71p-3,
     -0x1.db4b8b5036e9ap-3,
     0x1.cd8926ae49aecp+0,
     -0x1.cefcf168bed02p-3},

    {-0x1.eb851eb851eb8p-3,
     -0x1.f05d81df0953fp-3,
     0x1.d02b658023fc0p+0,
     -0x1.e265682776e76p-3},

    {-0x1.0000000000000p-2,
     -0x1.02be9ce0b87cdp-2,
     0x1.d2cf5c7c70f0cp+0,
     -0x1.f5b75f92c80ddp-3},

    {-0x1.0a3d70a3d70a4p-2,
     -0x1.0d55b13e747aep-2,
     0x1.d5752193dff04p+0,
     -0x1.04790f72887d4p-2},

    {-0x1.147ae147ae148p-2,
     -0x1.17f457c46daabp-2,
     0x1.d81ccb355e3c3p+0,
     -0x1.0e0a79a2559afp-2},

    {-0x1.1eb851eb851ecp-2,
     -0x1.229aec47638ddp-2,
     0x1.dac670561bb50p+0,
     -0x1.178f97ed1f891p-2},

    {-0x1.28f5c28f5c28fp-2,
     -0x1.2d49ccd6f3133p-2,
     0x1.dd722879ff965p+0,
     -0x1.210816f1dae81p-2},

    {-0x1.3333333333333p-2,
     -0x1.380159e14f6ffp-2,
     0x1.e0200bbc96ad8p+0,
     -0x1.2a73a661eaf06p-2},

    {-0x1.3d70a3d70a3d7p-2,
     -0x1.42c1f6590a37dp-2,
     0x1.e2d032da855f8p+0,
     -0x1.33d1f9074c099p-2},

    {-0x1.47ae147ae147bp-2,
     -0x1.4d8c07dd17d7dp-2,
     0x1.e582b73b88c77p+0,
     -0x1.3d22c4c92395dp-2},

    {-0x1.51eb851eb851fp-2,
     -0x1.585ff6e341c3ep-2,
     0x1.e837b2fd13428p+0,
     -0x1.4665c2aebeed4p-2},

    {-0x1.5c28f5c28f5c3p-2,
     -0x1.633e2ee53c5b9p-2,
     0x1.eaef40fd91e86p+0,
     -0x1.4f9aaee10ca82p-2},

    {-0x1.6666666666667p-2,
     -0x1.6e271e909bbe5p-2,
     0x1.eda97ce869c12p+0,
     -0x1.58c148aa9c5d7p-2},

    {-0x1.70a3d70a3d70ap-2,
     -0x1.791b37f9e8a25p-2,
     0x1.f0668342bcfa2p+0,
     -0x1.61d9527631ea9p-2},

    {-0x1.7ae147ae147aep-2,
     -0x1.841af0d31cc53p-2,
     0x1.f32671790a02dp+0,
     -0x1.6ae291cbfa274p-2},

    {-0x1.851eb851eb852p-2,
     -0x1.8f26c2a5d5e1dp-2,
     0x1.f5e965edb84a0p+0,
     -0x1.73dccf4d6fa15p-2},

    {-0x1.8f5c28f5c28f6p-2,
     -0x1.9a3f2b11964ecp-2,
     0x1.f8af8008a8653p+0,
     -0x1.7cc7d6affe959p-2},

    {-0x1.999999999999ap-2,
     -0x1.a564ac0e73a34p-2,
     0x1.fb78e047dfba5p+0,
     -0x1.85a376b677dc0p-2},

    {-0x1.a3d70a3d70a3ep-2,
     -0x1.b097cc349e2c7p-2,
     0x1.fe45a8516a5cap+0,
     -0x1.8e6f812962e4fp-2},

    {-0x1.ae147ae147ae1p-2,
     -0x1.bbd9170937b7fp-2,
     0x1.008afd83485fcp+1,
     -0x1.972bcace3f311p-2},

    {-0x1.b851eb851eb85p-2,
     -0x1.c7291d50fd81ep-2,
     0x1.01f4fe4c41190p+1,
     -0x1.9fd82b5dc5e5fp-2},

    {-0x1.c28f5c28f5c29p-2,
     -0x1.d2887569581cap-2,
     0x1.0360e94f4c6c5p+1,
     -0x1.a8747d793c3dbp-2},

    {-0x1.ccccccccccccdp-2,
     -0x1.ddf7bba8753cdp-2,
     0x1.04ced21730106p+1,
     -0x1.b1009e9ee79bcp-2},

    {-0x1.d70a3d70a3d71p-2,
     -0x1.e97792c522be1p-2,
     0x1.063eccfac5c08p+1,
     -0x1.b97c6f1db4032p-2},

    {-0x1.e147ae147ae15p-2,
     -0x1.f508a4473857fp-2,
     0x1.07b0ef2b0873cp+1,
     -0x1.c1e7d2081d8e2p-2},

    {-0x1.eb851eb851eb8p-2,
     -0x1.0055d080bb634p-1,
     0x1.09254ec250419p+1,
     -0x1.ca42ad266d56fp-2},

    {-0x1.f5c28f5c28f5cp-2,
     -0x1.0630a0caf011ap-1,
     0x1.0a9c02d4dd6d3p+1,
     -0x1.d28ce8e859fefp-2},

    {-0x1.0000000000000p-1,
     -0x1.0c152382d7366p-1,
     0x1.0c152382d7366p+1,
     -0x1.dac670561bb4fp-2},

    {-0x1.051eb851eb852p-1,
     -0x1.1203bda719c39p-1,
     0x1.0d90ca0be7d9ap+1,
     -0x1.e2ef3101033dfp-2},

    {-0x1.0a3d70a3d70a4p-1,
     -0x1.17fcd90a0d19fp-1,
     0x1.0f0f10e4a4af4p+1,
     -0x1.eb071af3a3191p-2},

    {-0x1.0f5c28f5c28f6p-1,
     -0x1.1e00e4af59504p-1,
     0x1.109013cdf7bcdp+1,
     -0x1.f30e20a199675p-2},

    {-0x1.147ae147ae148p-1,
     -0x1.241055329849fp-1,
     0x1.1213efeec77b4p+1,
     -0x1.fb0436d708c0ap-2},

    {-0x1.199999999999ap-1,
     -0x1.2a2ba538032f0p-1,
     0x1.139ac3f022348p+1,
     -0x1.0174aa53e6ce4p-1},

    {-0x1.1eb851eb851ecp-1,
     -0x1.305355e86c374p-1,
     0x1.1524b01c3c769p+1,
     -0x1.055eb9af3eb4bp-1},

    {-0x1.23d70a3d70a3ep-1,
     -0x1.3687ef79f2085p-1,
     0x1.16b1d6809deadp+1,
     -0x1.094047359deecp-1},

    {-0x1.28f5c28f5c28fp-1,
     -0x1.3cca01c711851p-1,
     0x1.18425b13e5ca0p+1,
     -0x1.0d1951a9393b7p-1},

    {-0x1.2e147ae147ae1p-1,
     -0x1.431a24f5fc8e4p-1,
     0x1.19d663dfa08c5p+1,
     -0x1.10e9d8cdbac48p-1},

    {-0x1.3333333333333p-1,
     -0x1.4978fa3269ee1p-1,
     0x1.1b6e192ebbe44p+1,
     -0x1.14b1dd5f90ce1p-1},

    {-0x1.3851eb851eb85p-1,
     -0x1.4fe72c7c6f17ap-1,
     0x1.1d09a5c13d2ebp+1,
     -0x1.1871610b3da41p-1},

    {-0x1.3d70a3d70a3d7p-1,
     -0x1.5665718f62b97p-1,
     0x1.1ea93705fa172p+1,
     -0x1.1c286664ad8c9p-1},

    {-0x1.428f5c28f5c29p-1,
     -0x1.5cf48ae44b724p-1,
     0x1.204cfd5b34455p+1,
     -0x1.1fd6f0de97388p-1},

    {-0x1.47ae147ae147bp-1,
     -0x1.639546d3fd53bp-1,
     0x1.21f52c5720bdbp+1,
     -0x1.237d04c1eae02p-1},

    {-0x1.4cccccccccccdp-1,
     -0x1.6a4881ddc9b84p-1,
     0x1.23a1fb1993d6dp+1,
     -0x1.271aa72553ed5p-1},

    {-0x1.51eb851eb851fp-1,
     -0x1.710f28188f80dp-1,
     0x1.2553a4a84548fp+1,
     -0x1.2aafdde4d0c9fp-1},

    {-0x1.570a3d70a3d71p-1,
     -0x1.77ea36d518926p-1,
     0x1.270a6857678d6p+1,
     -0x1.2e3caf996421ep-1},

    {-0x1.5c28f5c28f5c3p-1,
     -0x1.7edabe7a11ffep-1,
     0x1.28c68a40a5e8cp+1,
     -0x1.31c12390e29fap-1},

    {-0x1.6147ae147ae15p-1,
     -0x1.85e1e4a3a04f3p-1,
     0x1.2a8853cb097c9p+1,
     -0x1.353d41c5dfe68p-1},

    {-0x1.6666666666667p-1,
     -0x1.8d00e692afd97p-1,
     0x1.2c501446cd5f2p+1,
     -0x1.38b112d7bd4aep-1},

    {-0x1.6b851eb851eb8p-1,
     -0x1.94391bfac8e4bp-1,
     0x1.2e1e21a0d3a1fp+1,
     -0x1.3c1ca002dc877p-1},

    {-0x1.70a3d70a3d70ap-1,
     -0x1.9b8bfa40885bdp-1,
     0x1.2ff2d932437fbp+1,
     -0x1.3f7ff318f8723p-1},

    {-0x1.75c28f5c28f5cp-1,
     -0x1.a2fb183f1f79cp-1,
     0x1.31cea0b1e9473p+1,
     -0x1.42db1679a576fp-1},

    {-0x1.7ae147ae147aep-1,
     -0x1.aa8832b0b0525p-1,
     0x1.33b1e74e4d7d5p+1,
     -0x1.462e150afb65fp-1},

    {-0x1.8000000000000p-1,
     -0x1.b235315c680dcp-1,
     0x1.359d26f93b6c3p+1,
     -0x1.4978fa3269ee1p-1},

    {-0x1.851eb851eb852p-1,
     -0x1.ba042d36663dbp-1,
     0x1.3790e5efbaf83p+1,
     -0x1.4cbbd1cdb8e70p-1},

    {-0x1.8a3d70a3d70a4p-1,
     -0x1.c1f777a9974f2p-1,
     0x1.398db88c873c9p+1,
     -0x1.4ff6a82c35600p-1},

    {-0x1.8f5c28f5c28f6p-1,
     -0x1.ca11a353bd84cp-1,
     0x1.3b94437710c9fp+1,
     -0x1.53298a080c38dp-1},

    {-0x1.947ae147ae148p-1,
     -0x1.d2558e9188decp-1,
     0x1.3da53e4683a07p+1,
     -0x1.5654847fd2e00p-1},

    {-0x1.999999999999ap-1,
     -0x1.dac670561bb50p-1,
     0x1.3fc176b7a8560p+1,
     -0x1.5977a5103ea93p-1},

    {-0x1.9eb851eb851ecp-1,
     -0x1.e367e7f212996p-1,
     0x1.41e9d49ea60f2p+1,
     -0x1.5c92f98e0b072p-1},

    {-0x1.a3d70a3d70a3ep-1,
     -0x1.ec3e10a736333p-1,
     0x1.441f5ecbeef59p+1,
     -0x1.5fa690200ed46p-1},

    {-0x1.a8f5c28f5c290p-1,
     -0x1.f54d9a373facep-1,
     0x1.4663412ff1540p+1,
     -0x1.62b2773980b12p-1},

    {-0x1.ae147ae147ae1p-1,
     -0x1.fe9be811df6cap-1,
     0x1.48b6d4a69943fp+1,
     -0x1.65b6bd946a61bp-1},

    {-0x1.b333333333333p-1,
     -0x1.04179cba26d0ep+0,
     0x1.4b1ba8ff34d13p+1,
     -0x1.68b3722c4afb4p-1},

    {-0x1.b851eb851eb85p-1,
     -0x1.09076ee9d82bbp+0,
     0x1.4d9392170d7e9p+1,
     -0x1.6ba8a438e7926p-1},

    {-0x1.bd70a3d70a3d7p-1,
     -0x1.0e21bd416ba16p+0,
     0x1.5020b942d7397p+1,
     -0x1.6e9663294a097p-1},

    {-0x1.c28f5c28f5c29p-1,
     -0x1.136bb485f3d91p+0,
     0x1.52c5b4e51b555p+1,
     -0x1.717cbe9eed853p-1},

    {-0x1.c7ae147ae147bp-1,
     -0x1.18eb9cef862d9p+0,
     0x1.5585a919e47f9p+1,
     -0x1.745bc66917fb0p-1},

    {-0x1.ccccccccccccdp-1,
     -0x1.1ea93705fa172p+0,
     0x1.586476251e745p+1,
     -0x1.77338a80603bep-1},

    {-0x1.d1eb851eb851fp-1,
     -0x1.24ae43a7af149p+0,
     0x1.5b66fc75f8f31p+1,
     -0x1.7a041b025fce9p-1},

    {-0x1.d70a3d70a3d71p-1,
     -0x1.2b07529b1748ap+0,
     0x1.5e9383efad0d1p+1,
     -0x1.7ccd882d8fdbep-1},

    {-0x1.dc28f5c28f5c3p-1,
     -0x1.31c50a48f3a76p+0,
     0x1.61f25fc69b3c7p+1,
     -0x1.7f8fe25d5067bp-1},

    {-0x1.e147ae147ae15p-1,
     -0x1.38fe4cb950b12p+0,
     0x1.658f00fec9c15p+1,
     -0x1.824b3a061901ap-1},

    {-0x1.e666666666667p-1,
     -0x1.40d41159f340bp+0,
     0x1.6979e34f1b092p+1,
     -0x1.84ff9fb1d212ep-1},

    {-0x1.eb851eb851eb8p-1,
     -0x1.4978fa3269ee1p+0,
     0x1.6dcc57bb565fcp+1,
     -0x1.87ad23fc55e47p-1},

    {-0x1.f0a3d70a3d70ap-1,
     -0x1.5342538981ec8p+0,
     0x1.72b10466e25f0p+1,
     -0x1.8a53d7901872ap-1},

    {-0x1.f5c28f5c28f5cp-1,
     -0x1.5ed690583be07p+0,
     0x1.787b22ce3f590p+1,
     -0x1.8cf3cb22f51e5p-1},

    {-0x1.fae147ae147aep-1,
     -0x1.6de3c6f33d51dp+0,
     0x1.8001be1bc011bp+1,
     -0x1.8f8d0f7321467p-1},

    {-0x1.0000000000000p+0,
     -0x1.921fb54442d18p+0,
     0x1.921fb54442d18p+1,
     -0x1.921fb54442d18p-1},

    {-0x1.028f5c28f5c29p+0,
     -NAN,
     -NAN,
     -0x1.94abcd5ca9acfp-1},

    {-0x1.051eb851eb852p+0,
     -NAN,
     -NAN,
     -0x1.97316882ab45ap-1},
};

MATH_ARC_TANGENT_DOUBLE TestArcTangentDoubles[] = {
    {NAN,
     NAN,
     NAN,
     NAN},

    {NAN,
     0x1.0000000000000p+0,
     NAN,
     NAN},

    {0x1.0000000000000p+0,
     NAN,
     NAN,
     NAN},

    {0x1.0000000000000p+0,
     0x1.0000000000000p+0,
     0x1.921fb54442d18p-1,
     0x1.921fb54442d18p-1},

    {0x0.0000000000000p+0,
     0x1.199999999999ap+0,
     0x0.0000000000000p+0,
     0x0.0000000000000p+0},

    {-0x0.0000000000000p+0,
     0x1.199999999999ap+0,
     -0x0.0000000000000p+0,
     -0x0.0000000000000p+0},

    {0x0.0000000000000p+0,
     -0x1.199999999999ap+0,
     -0x0.0000000000000p+0,
     0x1.921fb54442d18p+1},

    {-0x0.0000000000000p+0,
     -0x1.199999999999ap+0,
     0x0.0000000000000p+0,
     -0x1.921fb54442d18p+1},

    {0x1.e666666666666p-1,
     0x0.0000000000000p+0,
     0x1.921fb54442d18p+0,
     0x1.921fb54442d18p+0},

    {-0x1.e666666666666p-1,
     -0x0.0000000000000p+0,
     0x1.921fb54442d18p+0,
     -0x1.921fb54442d18p+0},

    {INFINITY,
     INFINITY,
     -INFINITY,
     0x1.921fb54442d18p-1},

    {-INFINITY,
     INFINITY,
     -INFINITY,
     -0x1.921fb54442d18p-1},

    {INFINITY,
     -INFINITY,
     -INFINITY,
     0x1.2d97c7f3321d2p+1},

    {-INFINITY,
     -INFINITY,
     -INFINITY,
     -0x1.2d97c7f3321d2p+1},

    {0x1.0000000000000p-1,
     INFINITY,
     0x0.0000000000000p+0,
     0x0.0000000000000p+0},

    {-0x1.0000000000000p-1,
     INFINITY,
     -0x0.0000000000000p+0,
     -0x0.0000000000000p+0},

    {0x1.0000000000000p-1,
     -INFINITY,
     -0x0.0000000000000p+0,
     0x1.921fb54442d18p+1},

    {-0x1.199999999999ap-1,
     -INFINITY,
     0x0.0000000000000p+0,
     -0x1.921fb54442d18p+1},

    {INFINITY,
     0x1.70a3d70a3d70ap-4,
     0x1.921fb54442d18p+0,
     0x1.921fb54442d18p+0},

    {-INFINITY,
     -0x1.70a3d70a3d70ap-4,
     0x1.921fb54442d18p+0,
     -0x1.921fb54442d18p+0},

    {0x1.8e45e1df3b015p+201,
     0x1.0000000000000p-1,
     0x1.921fb54442d18p+0,
     0x1.921fb54442d18p+0},

    {0x1.011c2eaabe7d8p-197,
     0x1.0000000000000p-1,
     0x1.011c2eaabe7d8p-196,
     0x1.011c2eaabe7d8p-196},

    {0x1.999999999999ap-2,
     0x1.0000000000000p+0,
     0x1.85a376b677dc0p-2,
     0x1.85a376b677dc0p-2},

    {-0x1.3333333333333p-1,
     0x0.0000000000000p+0,
     -0x1.921fb54442d18p+0,
     -0x1.921fb54442d18p+0},

    {-0x1.999999999999ap-3,
     0x1.999999999999ap-3,
     -0x1.921fb54442d18p-1,
     -0x1.921fb54442d18p-1},

    {-0x1.999999999999ap-1,
     -0x1.999999999999ap-1,
     0x1.921fb54442d18p-1,
     -0x1.2d97c7f3321d2p+1},
};

MATH_EXP_DOUBLE TestExpDoubles[] = {
    {INFINITY, INFINITY, INFINITY},
    {-INFINITY, 0x0.0000000000000p+0, -0x1.0000000000000p+0},
    {NAN, NAN, NAN},
    {0x1.6300000000000p+9, INFINITY, INFINITY},
    {0x1.9000000000000p+9, INFINITY, INFINITY},
    {-0x1.6300000000000p+9, 0x0.33802fd28b3c3p-1022, -0x1.0000000000000p+0},
    {-0x1.9000000000000p+9, 0x0.0000000000000p+0, -0x1.0000000000000p+0},
    {0x1.6353f7ced9168p-2, 0x1.6a316dcd4cc00p+0, 0x1.a8c5b73532fffp-2},
    {0x1.0a3d70a3d70a4p+0, 0x1.6a23c87af69e8p+1, 0x1.d44790f5ed3cfp+0},
    {0x1.4000000000000p+3, 0x1.5829dcf950560p+14, 0x1.5825dcf950560p+14},
    {0x1.9000000000000p+6, 0x1.3494a9b171bf5p+144, 0x1.3494a9b171bf5p+144},
    {-0x1.7333333333334p+1, 0x1.c2c00e553650ap-5, -0x1.e3d3ff1aac9afp-1},
    {0x1.921fb54442d18p+1, 0x1.724046eb09339p+4, 0x1.624046eb09339p+4},
    {0x1.921fb54442d18p+0, 0x1.33dedc855935fp+2, 0x1.e7bdb90ab26bep+1},
    {-0x1.921fb54442d18p+1, 0x1.620227b598efap-5, -0x1.e9dfdd84a6710p-1},
    {-0x1.921fb54442d18p+0, 0x1.a9bcc46f767e0p-3, -0x1.9590cee422608p-1},
};

MATH_POWER_DOUBLE TestPowerDoubles[] = {
    {0x1.f400000000000p+9,
     0x0.0000000000000p+0,
     0x1.0000000000000p+0,
     0x1.f400000000000p+9},

    {INFINITY,
     0x0.0000000000000p+0,
     0x1.0000000000000p+0,
     INFINITY},

    {0x1.cb63b5c484765p+333,
     0x1.0000000000000p+0,
     0x1.cb63b5c484765p+333,
     0x1.cb63b5c484765p+333},

    {0x1.0000000000000p+2,
     NAN,
     NAN,
     NAN},

    {NAN,
     0x1.8000000000000p+1,
     NAN,
     NAN},

    {NAN,
     0x0.0000000000000p+0,
     0x1.0000000000000p+0,
     NAN},

    {0x1.0000000000000p+1,
     INFINITY,
     INFINITY,
     INFINITY},

    {0x1.0000000000000p+1,
     -INFINITY,
     0x0.0000000000000p+0,
     INFINITY},

    {0x1.0000000000000p-1,
     INFINITY,
     0x0.0000000000000p+0,
     INFINITY},

    {0x1.0000000000000p-1,
     -INFINITY,
     INFINITY,
     INFINITY},

    {0x0.0000000000000p+0,
     0x1.4000000000000p+5,
     0x0.0000000000000p+0,
     0x1.4000000000000p+5},

    {-0x0.0000000000000p+0,
     0x1.4000000000000p+5,
     0x0.0000000000000p+0,
     0x1.4000000000000p+5},

    {0x0.0000000000000p+0,
     -0x1.4000000000000p+5,
     INFINITY,
     0x1.4000000000000p+5},

    {-0x0.0000000000000p+0,
     -0x1.4000000000000p+5,
     INFINITY,
     0x1.4000000000000p+5},

    {-0x0.0000000000000p+0,
     -0x1.8000000000000p+1,
     -INFINITY,
     0x1.8000000000000p+1},

    {INFINITY,
     0x1.3880000000000p+12,
     INFINITY,
     INFINITY},

    {INFINITY,
     0x1.8000000000000p+1,
     INFINITY,
     INFINITY},

    {-INFINITY,
     0x1.0000000000000p+2,
     INFINITY,
     INFINITY},

    {-INFINITY,
     -0x1.4000000000000p+2,
     -0x0.0000000000000p+0,
     INFINITY},

    {-0x1.90ccccccccccdp+6,
     0x1.8000000000000p+2,
     0x1.d746f901e0627p+39,
     0x1.91849666040c0p+6},

    {-0x1.8f03126e978d5p+5,
     0x1.8333333333333p+3,
     -INFINITY,
     0x1.9a9600532dcc8p+5},

    {-0x1.6b851eb851eb8p+5,
     -0x1.999999999999ap-4,
     -INFINITY,
     0x1.6b855868fa69ap+5},

    {0x1.5957ae147ae14p+8,
     -0x1.3333333333333p-2,
     0x1.62b1e4f3a7a67p-3,
     0x1.5957b69ea44c4p+8},

    {0x1.e000000000000p+4,
     0x1.8000000000000p+2,
     0x1.5b9d420000000p+29,
     0x1.e98180e9b47f2p+4},

    {0x1.000053e2d6239p+1,
     0x1.638e368f08462p+2,
     0x1.7843287f61601p+5,
     0x1.79e4de9b4f1a1p+2},

    {0x1.a87c262151e1ap+250,
     0x1.3333333333333p-1,
     0x1.5abf5dbab253dp+150,
     0x1.a87c262151e1ap+250},

    {0x1.59fe000000000p+16,
     0x1.0000000000000p-1,
     0x1.299d24dd6b5bfp+8,
     0x1.59fe000017ad4p+16},

    {0x1.3400000000000p+6,
     0x1.0000000000000p+1,
     0x1.7290000000000p+12,
     0x1.341a97c97b75ep+6},

    {-0x1.d400000000000p+7,
     0x1.8000000000000p+1,
     -0x1.8704d00000000p+23,
     0x1.d409d88306798p+7},

    {-0x1.2000000000000p+3,
     0x1.0000000000000p+2,
     0x1.9a10000000000p+12,
     0x1.3b29d7d635662p+3},

    {0x1.13d73cbe1ff08p-99,
     0x1.999999999999ap-4,
     0x1.146e057a2535ep-10,
     0x1.999999999999ap-4},

    {0x1.c8571c4687a3dp-20,
     0x1.c000000000000p+2,
     0x1.c98afc1f5b24fp-135,
     0x1.c0000000000e9p+2},
};

MATH_LOGARITHM_DOUBLE TestLogarithmDoubles[] = {
    {0x1.921fb54442d18p-1,
     -0x1.eeb95b094c193p-3,
     -0x1.64de32d9c8824p-2,
     -0x1.adb63b88d410dp-4},

    {0x1.921fb54442d18p+1,
     0x1.250d048e7a1bdp+0,
     0x1.a6c873498ddf7p+0,
     0x1.fd14db31ba3bbp-2},

    {0x1.f6a7a2955385ep+1,
     0x1.5e2cf41daf501p+0,
     0x1.f9325478c24dap+0,
     0x1.30289e09e9adfp-1},

    {0x1.921fb54442d18p+2,
     0x1.d67f1c864beb4p+0,
     0x1.536439a4c6efcp+1,
     0x1.98ab081dd8eddp-1},

    {0x1.f6a7a2955385ep+1,
     0x1.5e2cf41daf501p+0,
     0x1.f9325478c24dap+0,
     0x1.30289e09e9adfp-1},

    {0x1.5fdbbe9bba775p+2,
     0x1.b44fff81fc225p+0,
     0x1.3abba19a07328p+1,
     0x1.7af9e9467241ep-1},

    {0x1.0000000000000p-3,
     -0x1.0a2b23f3bab73p+1,
     -0x1.8000000000000p+1,
     -0x1.ce61cf8ef36fep-1},

    {0x1.945b6c3760bf6p-7,
     -0x1.1946317db0637p+2,
     -0x1.95cafa5fa6281p+2,
     -0x1.e89f91d778ae6p+0},

    {0x1.c560c7c0f4517p-9,
     -0x1.6aac7430f596bp+2,
     -0x1.059d1dd8afe53p+3,
     -0x1.3b03ab00fe665p+1},

    {0x1.ba35f15394d29p-11,
     -0x1.c4fe1d616c20fp+2,
     -0x1.46c3de8d48d75p+3,
     -0x1.8976e330c38bcp+1},

    {0x1.4000000000000p+3,
     0x1.26bb1bbb55516p+1,
     0x1.a934f0979a371p+1,
     0x1.0000000000000p+0},

    {0x1.9000000000000p+6,
     0x1.26bb1bbb55516p+2,
     0x1.a934f0979a371p+2,
     0x1.0000000000000p+1},

    {0x1.f400000000000p+9,
     0x1.ba18a998fffa0p+2,
     0x1.3ee7b471b3a95p+3,
     0x1.8000000000000p+1},

    {0x1.86a0000000000p+16,
     0x1.7069e2aa2aa5bp+3,
     0x1.09c1165ec0627p+4,
     0x1.4000000000000p+2},

    {0x1.24f8000000000p+18,
     0x1.9391b79f84abcp+3,
     0x1.231d1802601fdp+4,
     0x1.5e8927964fd60p+2},

    {0x1.2a05f20000000p+33,
     0x1.7069e2aa2aa5bp+4,
     0x1.09c1165ec0627p+5,
     0x1.4000000000000p+3},

    {0x1.c6bf526340000p+49,
     0x1.144f69ff9ffc4p+5,
     0x1.8ea1a18e2093ap+5,
     0x1.e000000000000p+3},

    {0x1.5af1d78b58c40p+66,
     0x1.7069e2aa2aa5bp+5,
     0x1.09c1165ec0627p+6,
     0x1.4000000000000p+4},

    {0x1.08b2a2c280291p+83,
     0x1.cc845b54b54f2p+5,
     0x1.4c315bf6707b1p+6,
     0x1.9000000000000p+4},

    {0x1.93e5939a08ceap+99,
     0x1.144f69ff9ffc4p+6,
     0x1.8ea1a18e2093ap+6,
     0x1.e000000000000p+4},

    {0x1.3426172c74d82p+116,
     0x1.425ca654e5510p+6,
     0x1.d111e725d0ac4p+6,
     0x1.1800000000000p+5},

    {0x1.d6329f1c35ca5p+132,
     0x1.7069e2aa2aa5bp+6,
     0x1.09c1165ec0627p+7,
     0x1.4000000000000p+5},

    {0x1.11b0ec57e649ap+166,
     0x1.cc845b54b54f2p+6,
     0x1.4c315bf6707b1p+7,
     0x1.9000000000000p+5},

    {0x1.1afd6ec0e1411p+249,
     0x1.5963447f87fb5p+7,
     0x1.f24a09f1a8b89p+7,
     0x1.2c00000000000p+6},

    {0x1.249ad2594c37dp+332,
     0x1.cc845b54b54f2p+7,
     0x1.4c315bf6707b1p+8,
     0x1.9000000000000p+6},

    {0x1.38d352e5096afp+498,
     0x1.5963447f87fb5p+8,
     0x1.f24a09f1a8b89p+8,
     0x1.2c00000000000p+7},

    {0x1.4f8b588e368f1p-17,
     -0x1.7069e2aa2aa5bp+3,
     -0x1.09c1165ec0627p+4,
     -0x1.4000000000000p+2},

    {0x1.b7cdfd9d7bdbbp-34,
     -0x1.7069e2aa2aa5bp+4,
     -0x1.09c1165ec0627p+5,
     -0x1.4000000000000p+3},

    {0x1.203af9ee75616p-50,
     -0x1.144f69ff9ffc4p+5,
     -0x1.8ea1a18e2093ap+5,
     -0x1.e000000000000p+3},

    {0x1.79ca10c924223p-67,
     -0x1.7069e2aa2aa5bp+5,
     -0x1.09c1165ec0627p+6,
     -0x1.4000000000000p+4},

    {0x1.cdef9d8000000p+34,
     0x1.82841bc3e2523p+4,
     0x1.16cff9021f59ap+5,
     0x1.4fb93c28b0cfdp+3},

    {0x1.6bcc41e900000p+51,
     0x1.1d9d8c02a36fap+5,
     0x1.9c0e5284a6f03p+5,
     0x1.f02a30498eb10p+3},

    {0x1.1e3ab8395c6e8p+68,
     0x1.79f709e34b097p+5,
     0x1.10a4e480eeaebp+6,
     0x1.484bd545e4bafp+4},

    {0x1.c1fc7b177378fp+84,
     0x1.d64ea61ab9080p+5,
     0x1.5341444d9ecdap+6,
     0x1.9880f009735c1p+4},

    {0x1.6168e126c7b4dp+101,
     0x1.19523e4b4d4bap+6,
     0x1.95dc5ccdca95cp+6,
     0x1.e8b480b19487ap+4},

    {0x1.15557b419c5c2p+118,
     0x1.477c53741fb67p+6,
     0x1.d8764072e3b94p+6,
     0x1.1c734eb9bbd40p+5},

    {0x1.b2eed32d4b5b2p+134,
     0x1.75a59e436d5f0p+6,
     0x1.0d87801518cd4p+7,
     0x1.448bad5873304p+5},

    {0x1.0401ad53812c5p+168,
     0x1.d1db65d9bc456p+6,
     0x1.500b788ef61bdp+7,
     0x1.94a3659511cfdp+5},

    {0x1.13ea4bfc0ed2bp+251,
     0x1.5c1c166abca98p+7,
     0x1.f637566ca2bffp+7,
     0x1.2e5d400a5401fp+6},

    {0x1.0000000000000p+0,
     0x0.0000000000000p+0,
     0x0.0000000000000p+0,
     0x0.0000000000000p+0},

    {0x1.0000000000000p+1,
     0x1.62e42fefa39efp-1,
     0x1.0000000000000p+0,
     0x1.34413509f79ffp-2},

    {0x1.8000000000000p+1,
     0x1.193ea7aad030bp+0,
     0x1.95c01a39fbd68p+0,
     0x1.e8927964fd5fdp-2},

    {0x1.22ad962000000p+27,
     0x1.2d78e26ae18e8p+4,
     0x1.b2eeb55d7c2d7p+4,
     0x1.05dafd7670640p+3},

    {0x1.9000000000000p+4,
     0x1.9c041f7ed8d33p+1,
     0x1.2934f0979a371p+2,
     0x1.65df657b04301p+0},

    {0x1.fae147ae147aep-1,
     -0x1.495453e6fd4bcp-7,
     -0x1.db1f34d2c386dp-7,
     -0x1.1e0d4874f92f2p-8},

    {-0x1.0000000000000p+0,
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

MATH_DECOMPOSITION_DOUBLE TestDecompositionDoubles[] = {
    {0x1.921fb54442d18p-1, 0x0.0000000000000p+0, 0x1.921fb54442d18p-1},
    {0x1.921fb54442d18p+1, 0x1.8000000000000p+1, 0x1.21fb54442d180p-3},
    {0x1.f6a7a2955385ep+1, 0x1.8000000000000p+1, 0x1.da9e8a554e178p-1},
    {0x1.921fb54442d18p+2, 0x1.8000000000000p+2, 0x1.21fb54442d180p-2},
    {0x1.f6a7a2955385ep+1, 0x1.8000000000000p+1, 0x1.da9e8a554e178p-1},
    {0x1.5fdbbe9bba775p+2, 0x1.4000000000000p+2, 0x1.fdbbe9bba7750p-2},
    {0x1.0000000000000p-3, 0x0.0000000000000p+0, 0x1.0000000000000p-3},
    {0x1.945b6c3760bf6p-7, 0x0.0000000000000p+0, 0x1.945b6c3760bf6p-7},
    {0x1.c560c7c0f4517p-9, 0x0.0000000000000p+0, 0x1.c560c7c0f4517p-9},
    {0x1.ba35f15394d29p-11, 0x0.0000000000000p+0, 0x1.ba35f15394d29p-11},
    {0x1.4000000000000p+3, 0x1.4000000000000p+3, 0x0.0000000000000p+0},
    {0x1.24f8000000000p+18, 0x1.24f8000000000p+18, 0x0.0000000000000p+0},
    {0x1.4f8b588e368f1p-17, 0x0.0000000000000p+0, 0x1.4f8b588e368f1p-17},
    {0x1.b7cdfd9d7bdbbp-34, 0x0.0000000000000p+0, 0x1.b7cdfd9d7bdbbp-34},
    {0x1.203af9ee75616p-50, 0x0.0000000000000p+0, 0x1.203af9ee75616p-50},
    {0x1.79ca10c924223p-67, 0x0.0000000000000p+0, 0x1.79ca10c924223p-67},
    {0x1.cdef9d8000000p+34, 0x1.cdef9d8000000p+34, 0x0.0000000000000p+0},
    {0x1.6bcc41e900000p+51, 0x1.6bcc41e900000p+51, 0x0.0000000000000p+0},
    {0x1.1e3ab8395c6e8p+68, 0x1.1e3ab8395c6e8p+68, 0x0.0000000000000p+0},
    {0x1.c1fc7b177378fp+84, 0x1.c1fc7b177378fp+84, 0x0.0000000000000p+0},
    {0x1.6168e126c7b4dp+101, 0x1.6168e126c7b4dp+101, 0x0.0000000000000p+0},
    {0x1.15557b419c5c2p+118, 0x1.15557b419c5c2p+118, 0x0.0000000000000p+0},
    {0x1.b2eed32d4b5b2p+134, 0x1.b2eed32d4b5b2p+134, 0x0.0000000000000p+0},
    {0x1.0401ad53812c5p+168, 0x1.0401ad53812c5p+168, 0x0.0000000000000p+0},
    {0x1.13ea4bfc0ed2bp+251, 0x1.13ea4bfc0ed2bp+251, 0x0.0000000000000p+0},
    {0x1.249ad2594c37dp+332, 0x1.249ad2594c37dp+332, 0x0.0000000000000p+0},
    {0x1.38d352e5096afp+498, 0x1.38d352e5096afp+498, 0x0.0000000000000p+0},
    {0x1.0000000000000p+0, 0x1.0000000000000p+0, 0x0.0000000000000p+0},
    {0x1.0000000000000p+1, 0x1.0000000000000p+1, 0x0.0000000000000p+0},
    {0x1.8000000000000p+1, 0x1.8000000000000p+1, 0x0.0000000000000p+0},
    {0x1.22ad962000000p+27, 0x1.22ad962000000p+27, 0x0.0000000000000p+0},
    {0x1.9000000000000p+4, 0x1.9000000000000p+4, 0x0.0000000000000p+0},
    {0x1.fae147ae147aep-1, 0x0.0000000000000p+0, 0x1.fae147ae147aep-1},
    {-0x1.0000000000000p+0, -0x1.0000000000000p+0, -0x0.0000000000000p+0},
    {-0x1.999999999999ap-4, -0x0.0000000000000p+0, -0x1.999999999999ap-4},
    {-0x1.0bb448ec2f608p+70, -0x1.0bb448ec2f608p+70, -0x0.0000000000000p+0},
    {0x1.fe9af5b6bcbd4p+89, 0x1.fe9af5b6bcbd4p+89, 0x0.0000000000000p+0},
    {0x1.12210f47de981p+60, 0x1.12210f47de981p+60, 0x0.0000000000000p+0},
    {0x1.7c6e3bfd70fdep+116, 0x1.7c6e3bfd70fdep+116, 0x0.0000000000000p+0},
    {0x1.b1ae4d6e2ef50p+69, 0x1.b1ae4d6e2ef50p+69, 0x0.0000000000000p+0},
    {0x1.9d971e4fe8402p+89, 0x1.9d971e4fe8402p+89, 0x0.0000000000000p+0},
    {0x1.3426172c74d82p+116, 0x1.3426172c74d82p+116, 0x0.0000000000000p+0},
    {INFINITY, INFINITY, 0x0.0000000000000p+0},
    {-INFINITY, -INFINITY, -0x0.0000000000000p+0},
    {NAN, NAN, NAN},
};

MATH_CEILING_FLOOR_DOUBLE_VALUE TestCeilingFloorDoubles[] = {
    {-0x1.1000000000000p+5,
     -0x1.1000000000000p+5,
     -0x1.1000000000000p+5},

    {-0x1.059999999999ap+5,
     -0x1.0000000000000p+5,
     -0x1.0800000000000p+5},

    {-0x1.f666666666666p+4,
     -0x1.f000000000000p+4,
     -0x1.0000000000000p+5},

    {-0x1.e19999999999ap+4,
     -0x1.e000000000000p+4,
     -0x1.f000000000000p+4},

    {-0x1.ccccccccccccdp+4,
     -0x1.c000000000000p+4,
     -0x1.d000000000000p+4},

    {-0x1.b800000000000p+4,
     -0x1.b000000000000p+4,
     -0x1.c000000000000p+4},

    {-0x1.a333333333333p+4,
     -0x1.a000000000000p+4,
     -0x1.b000000000000p+4},

    {-0x1.8e66666666666p+4,
     -0x1.8000000000000p+4,
     -0x1.9000000000000p+4},

    {-0x1.799999999999ap+4,
     -0x1.7000000000000p+4,
     -0x1.8000000000000p+4},

    {-0x1.64ccccccccccdp+4,
     -0x1.6000000000000p+4,
     -0x1.7000000000000p+4},

    {-0x1.5000000000000p+4,
     -0x1.5000000000000p+4,
     -0x1.5000000000000p+4},

    {-0x1.3b33333333333p+4,
     -0x1.3000000000000p+4,
     -0x1.4000000000000p+4},

    {-0x1.2666666666666p+4,
     -0x1.2000000000000p+4,
     -0x1.3000000000000p+4},

    {-0x1.1199999999999p+4,
     -0x1.1000000000000p+4,
     -0x1.2000000000000p+4},

    {-0x1.f999999999999p+3,
     -0x1.e000000000000p+3,
     -0x1.0000000000000p+4},

    {-0x1.d000000000000p+3,
     -0x1.c000000000000p+3,
     -0x1.e000000000000p+3},

    {-0x1.a666666666666p+3,
     -0x1.a000000000000p+3,
     -0x1.c000000000000p+3},

    {-0x1.7ccccccccccccp+3,
     -0x1.6000000000000p+3,
     -0x1.8000000000000p+3},

    {-0x1.5333333333333p+3,
     -0x1.4000000000000p+3,
     -0x1.6000000000000p+3},

    {-0x1.2999999999999p+3,
     -0x1.2000000000000p+3,
     -0x1.4000000000000p+3},

    {-0x1.fffffffffffffp+2,
     -0x1.c000000000000p+2,
     -0x1.0000000000000p+3},

    {-0x1.accccccccccccp+2,
     -0x1.8000000000000p+2,
     -0x1.c000000000000p+2},

    {-0x1.5999999999998p+2,
     -0x1.4000000000000p+2,
     -0x1.8000000000000p+2},

    {-0x1.0666666666665p+2,
     -0x1.0000000000000p+2,
     -0x1.4000000000000p+2},

    {-0x1.6666666666664p+1,
     -0x1.0000000000000p+1,
     -0x1.8000000000000p+1},

    {-0x1.7fffffffffffbp+0,
     -0x1.0000000000000p+0,
     -0x1.0000000000000p+1},

    {-0x1.9999999999970p-3,
     -0x0.0000000000000p+0,
     -0x1.0000000000000p+0},

    {0x1.199999999999fp+0,
     0x1.0000000000000p+1,
     0x1.0000000000000p+0},

    {0x1.3333333333336p+1,
     0x1.8000000000000p+1,
     0x1.0000000000000p+1},

    {0x1.d99999999999cp+1,
     0x1.0000000000000p+2,
     0x1.8000000000000p+1},

    {0x1.4000000000002p+2,
     0x1.8000000000000p+2,
     0x1.4000000000000p+2},

    {0x1.9333333333335p+2,
     0x1.c000000000000p+2,
     0x1.8000000000000p+2},

    {0x1.e666666666668p+2,
     0x1.0000000000000p+3,
     0x1.c000000000000p+2},

    {0x1.1cccccccccccep+3,
     0x1.2000000000000p+3,
     0x1.0000000000000p+3},

    {0x1.4666666666667p+3,
     0x1.6000000000000p+3,
     0x1.4000000000000p+3},

    {0x1.7000000000001p+3,
     0x1.8000000000000p+3,
     0x1.6000000000000p+3},

    {0x1.999999999999ap+3,
     0x1.a000000000000p+3,
     0x1.8000000000000p+3},

    {0x1.c333333333334p+3,
     0x1.e000000000000p+3,
     0x1.c000000000000p+3},

    {0x1.ecccccccccccep+3,
     0x1.0000000000000p+4,
     0x1.e000000000000p+3},

    {0x1.0b33333333334p+4,
     0x1.1000000000000p+4,
     0x1.0000000000000p+4},

    {0x1.2000000000000p+4,
     0x1.2000000000000p+4,
     0x1.2000000000000p+4},

    {0x1.34ccccccccccdp+4,
     0x1.4000000000000p+4,
     0x1.3000000000000p+4},

    {0x1.499999999999ap+4,
     0x1.5000000000000p+4,
     0x1.4000000000000p+4},

    {0x1.5e66666666667p+4,
     0x1.6000000000000p+4,
     0x1.5000000000000p+4},

    {0x1.7333333333334p+4,
     0x1.8000000000000p+4,
     0x1.7000000000000p+4},

    {0x1.8800000000001p+4,
     0x1.9000000000000p+4,
     0x1.8000000000000p+4},

    {0x1.9cccccccccccdp+4,
     0x1.a000000000000p+4,
     0x1.9000000000000p+4},

    {0x1.b19999999999ap+4,
     0x1.c000000000000p+4,
     0x1.b000000000000p+4},

    {0x1.c666666666667p+4,
     0x1.d000000000000p+4,
     0x1.c000000000000p+4},

    {0x1.db33333333334p+4,
     0x1.e000000000000p+4,
     0x1.d000000000000p+4},

    {0x1.f000000000001p+4,
     0x1.0000000000000p+5,
     0x1.f000000000000p+4},

    {0x1.0266666666667p+5,
     0x1.0800000000000p+5,
     0x1.0000000000000p+5},

    {0x1.0cccccccccccdp+5,
     0x1.1000000000000p+5,
     0x1.0800000000000p+5},
};

MATH_MODULO_DOUBLE_VALUE TestModuloDoubles[] = {
    {0x0.0000000000000p+0,
     -0x0.0000000000000p+0,
     -INFINITY},

    {0x0.0000000000000p+0,
     -0x1.0000000000000p+0,
     0x0.0000000000000p+0},

    {-0x0.0000000000000p+0,
     -0x1.0000000000000p+0,
     -0x0.0000000000000p+0},

    {0x1.199999999999ap+0,
     0x1.8422737df88fap-114,
     0x1.33f2269b346c2p-114},

    {-0x1.8800000000000p+6,
     0x1.8c00000000000p+6,
     -0x1.8800000000000p+6},

    {0x1.9000000000000p+6,
     0x1.4000000000000p+3,
     0x0.0000000000000p+0},

    {0x1.4000000000000p+3,
     0x1.999999999999ap-4,
     0x1.9999999999972p-4},

    {0x1.9d3f427d3f994p+216,
     -0x1.151b071b1392ap+832,
     0x1.9d3f427d3f994p+216},

    {0x1.eba390bc6d0c6p+832,
     0x1.0000000000000p+2,
     0x0.0000000000000p+0},

    {0x1.0624dd2f1a9fcp-10,
     0x1.0624dd2f1a9fcp-10,
     0x0.0000000000000p+0},

    {0x1.8000000000000p+2,
     0x1.0000000000000p+1,
     0x0.0000000000000p+0},

    {-0x1.921fb54442d18p+1,
     -0x1.0000000000000p+1,
     -0x1.243f6a8885a30p+0},

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
TestMath (
    VOID
    )

/*++

Routine Description:

    This routine implements the entry point for the C library math test.

Arguments:

    None.

Return Value:

    Returns the count of test failures.

--*/

{

    ULONG Failures;

    Failures = 0;
    Failures += TestBasicTrigonometry();
    Failures += TestSquareRoot();
    Failures += TestArcTrigonometry();
    Failures += TestExponentiation();
    Failures += TestPower();
    Failures += TestLogarithm();
    Failures += TestDecomposition();
    Failures += TestCeilingAndFloor();
    Failures += TestModulo();
    return Failures;
}

//
// --------------------------------------------------------- Internal Functions
//

ULONG
TestBasicTrigonometry (
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

    volatile double Cosine;
    ULONG Failures;
    volatile double HyperbolicCosine;
    volatile double HyperbolicSine;
    volatile double HyperbolicTangent;
    volatile double Sine;
    volatile double Tangent;
    PMATH_TRIG_DOUBLE_VALUE Test;
    ULONG TestIndex;
    ULONG ValueCount;

    Failures = 0;
    ValueCount = sizeof(TestBasicTrigonometryDoubles) /
                 sizeof(TestBasicTrigonometryDoubles[0]);

    for (TestIndex = 0; TestIndex < ValueCount; TestIndex += 1) {
        Test = &(TestBasicTrigonometryDoubles[TestIndex]);
        Sine = sin(Test->Argument);
        Cosine = cos(Test->Argument);
        if (TestCompareResults(Sine, Test->Sine) == FALSE) {
            printf("sin(%.13a) was %.13a, should have been %.13a. Diff %.13a\n",
                   Test->Argument,
                   Sine,
                   Test->Sine,
                   Sine - Test->Sine);

            Failures += 1;
        }

        if (TestCompareResults(Cosine, Test->Cosine) == FALSE) {
            printf("cos(%.13a) was %.13a, should have been %.13a.\n",
                   Test->Argument,
                   Cosine,
                   Test->Cosine);

            Failures += 1;
        }

        Tangent = tan(Test->Argument);
        if (TestCompareResults(Tangent, Test->Tangent) == FALSE) {
            printf("tan(%.13a) was %.13a, should have been %.13a.\n",
                   Test->Argument,
                   Tangent,
                   Test->Tangent);

            Failures += 1;
        }

        HyperbolicSine = sinh(Test->Argument);
        HyperbolicCosine = cosh(Test->Argument);
        HyperbolicTangent = tanh(Test->Argument);
        if (TestCompareResults(HyperbolicSine, Test->HyperbolicSine) == FALSE) {
            printf("sinh(%.13a) was %.13a, should have been %.13a. "
                   "Diff %.13a\n",
                   Test->Argument,
                   HyperbolicSine,
                   Test->HyperbolicSine,
                   HyperbolicSine - Test->HyperbolicSine);

            Failures += 1;
        }

        if (TestCompareResults(HyperbolicCosine,
                               Test->HyperbolicCosine) == FALSE) {

            printf("cosh(%.13a) was %.13a, should have been %.13a.\n",
                   Test->Argument,
                   HyperbolicCosine,
                   Test->HyperbolicCosine);

            Failures += 1;
        }

        if (TestCompareResults(HyperbolicTangent,
                               Test->HyperbolicTangent) == FALSE) {

            printf("tanh(%.13a) was %.13a, should have been %.13a.\n",
                   Test->Argument,
                   HyperbolicTangent,
                   Test->HyperbolicTangent);

            Failures += 1;
        }

    }

    return Failures;
}

ULONG
TestSquareRoot (
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
    volatile double SquareRoot;
    PMATH_TEST_SQUARE_ROOT_DOUBLE_VALUE Test;
    ULONG TestIndex;
    ULONG ValueCount;

    Failures = 0;
    ValueCount = sizeof(TestSquareRootDoubles) /
                 sizeof(TestSquareRootDoubles[0]);

    for (TestIndex = 0; TestIndex < ValueCount; TestIndex += 1) {
        Test = &(TestSquareRootDoubles[TestIndex]);
        SquareRoot = sqrt(Test->Argument);
        if (TestCompareResults(SquareRoot, Test->SquareRoot) == FALSE) {
            printf("sqrt(%.13a) was %.13a, should have been %.13a.\n",
                   Test->Argument,
                   SquareRoot,
                   Test->SquareRoot);

            Failures += 1;
        }
    }

    return Failures;
}

ULONG
TestArcTrigonometry (
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

    volatile double ArcCosine;
    volatile double ArcSine;
    volatile double ArcTangent;
    PMATH_ARC_TANGENT_DOUBLE ArcTangentTest;
    ULONG Failures;
    double Quotient;
    PMATH_ARC_DOUBLE_VALUE Test;
    ULONG TestIndex;
    ULONG ValueCount;

    Failures = 0;
    ValueCount = sizeof(TestArcDoubles) /
                 sizeof(TestArcDoubles[0]);

    for (TestIndex = 0; TestIndex < ValueCount; TestIndex += 1) {
        Test = &(TestArcDoubles[TestIndex]);
        ArcSine = asin(Test->Argument);
        if (TestCompareResults(ArcSine, Test->ArcSine) == FALSE) {
            printf("asin(%.13a) was %.13a, should have been %.13a.\n",
                   Test->Argument,
                   ArcSine,
                   Test->ArcSine);

            Failures += 1;
        }

        ArcCosine = acos(Test->Argument);
        if (TestCompareResults(ArcCosine, Test->ArcCosine) == FALSE) {
            printf("acos(%.13a) was %.13a, should have been %.13a.\n",
                   Test->Argument,
                   ArcCosine,
                   Test->ArcCosine);

            Failures += 1;
        }

        ArcTangent = atan(Test->Argument);
        if (TestCompareResults(ArcTangent, Test->ArcTangent) == FALSE) {
            printf("atan(%.13a) was %.13a, should have been %.13a.\n",
                   Test->Argument,
                   ArcTangent,
                   Test->ArcTangent);

            Failures += 1;
        }
    }

    //
    // Test arc tangent 2.
    //

    ValueCount = sizeof(TestArcTangentDoubles) /
                 sizeof(TestArcTangentDoubles[0]);

    for (TestIndex = 0; TestIndex < ValueCount; TestIndex += 1) {
        ArcTangentTest = &(TestArcTangentDoubles[TestIndex]);
        Quotient = ArcTangentTest->Numerator /
                   ArcTangentTest->Denominator;

        ArcTangent = atan(Quotient);
        if (TestCompareResults(ArcTangent,
                               ArcTangentTest->ArcTangent) == FALSE) {

            printf("atan(%.13a) was %.13a, should have been %.13a.\n",
                   Quotient,
                   ArcTangent,
                   ArcTangentTest->ArcTangent);

            Failures += 1;
        }

        ArcTangent = atan2(ArcTangentTest->Numerator,
                           ArcTangentTest->Denominator);

        if (TestCompareResults(ArcTangent,
                               ArcTangentTest->ArcTangent2) == FALSE) {

            printf("atan2(%.13a, %.13a) was %.13a, should have been %.13a.\n",
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
TestExponentiation (
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
    volatile double Exponentiation;
    volatile double ExponentiationMinusOne;
    ULONG Failures;
    PMATH_EXP_DOUBLE Test;
    ULONG TestIndex;
    ULONG ValueCount;

    Failures = 0;
    ValueCount = sizeof(TestExpDoubles) /
                 sizeof(TestExpDoubles[0]);

    for (TestIndex = 0; TestIndex < ValueCount; TestIndex += 1) {
        Test = &(TestExpDoubles[TestIndex]);
        Exponentiation = exp(Test->Argument);
        if (TestCompareResults(Exponentiation, Test->Exponentiation) == FALSE) {
            printf("exp(%.13a) was %.13a, should have been %.13a.\n",
                   Test->Argument,
                   Exponentiation,
                   Test->Exponentiation);

            Failures += 1;
        }

        ExponentiationMinusOne = expm1(Test->Argument);
        Equal = TestCompareResults(ExponentiationMinusOne,
                                   Test->ExponentiationMinusOne);

        if (Equal == FALSE) {
            printf("expm1(%.13a) was %.13a, should have been %.13a.\n",
                   Test->Argument,
                   ExponentiationMinusOne,
                   Test->ExponentiationMinusOne);

            Failures += 1;
        }
    }

    return Failures;
}

ULONG
TestPower (
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
    volatile double Hypotenuse;
    volatile double RaisedValue;
    PMATH_POWER_DOUBLE Test;
    ULONG TestIndex;
    ULONG ValueCount;

    Failures = 0;
    ValueCount = sizeof(TestPowerDoubles) /
                 sizeof(TestPowerDoubles[0]);

    for (TestIndex = 0; TestIndex < ValueCount; TestIndex += 1) {
        Test = &(TestPowerDoubles[TestIndex]);
        RaisedValue = pow(Test->Value, Test->Exponent);
        Hypotenuse = hypot(Test->Value, Test->Exponent);
        if (TestCompareResults(RaisedValue, Test->Result) == FALSE) {
            printf("pow(%.13a, %.13a) was %.13a, should have been %.13a.\n",
                   Test->Value,
                   Test->Exponent,
                   RaisedValue,
                   Test->Result);

            Failures += 1;
        }

        if (TestCompareResults(Hypotenuse, Test->Hypotenuse) == FALSE) {
            printf("hypot(%.13a, %.13a) was %.13a, should have been %.13a.\n",
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
TestLogarithm (
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
    volatile double Logarithm;
    PMATH_LOGARITHM_DOUBLE Test;
    ULONG TestIndex;
    ULONG ValueCount;

    Failures = 0;
    ValueCount = sizeof(TestLogarithmDoubles) /
                 sizeof(TestLogarithmDoubles[0]);

    for (TestIndex = 0; TestIndex < ValueCount; TestIndex += 1) {
        Test = &(TestLogarithmDoubles[TestIndex]);
        Logarithm = log(Test->Argument);
        if (TestCompareResults(Logarithm, Test->Logarithm) == FALSE) {
            printf("log(%.13a) was %.13a, should have been %.13a.\n",
                   Test->Argument,
                   Logarithm,
                   Test->Logarithm);

            Failures += 1;
        }

        Logarithm = log2(Test->Argument);
        if (TestCompareResults(Logarithm, Test->Log2) == FALSE) {
            printf("log2(%.13a) was %.13a, should have been %.13a.\n",
                   Test->Argument,
                   Logarithm,
                   Test->Log2);

            Failures += 1;
        }

        Logarithm = log10(Test->Argument);
        if (TestCompareResults(Logarithm, Test->Log10) == FALSE) {
            printf("log10(%.13a) was %.13a, should have been %.13a.\n",
                   Test->Argument,
                   Logarithm,
                   Test->Log10);

            Failures += 1;
        }
    }

    return Failures;
}

ULONG
TestDecomposition (
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
    double FractionalPart;
    double IntegerPart;
    PMATH_DECOMPOSITION_DOUBLE Test;
    ULONG TestIndex;
    ULONG ValueCount;

    Failures = 0;
    ValueCount = sizeof(TestDecompositionDoubles) /
                 sizeof(TestDecompositionDoubles[0]);

    for (TestIndex = 0; TestIndex < ValueCount; TestIndex += 1) {
        Test = &(TestDecompositionDoubles[TestIndex]);
        FractionalPart = modf(Test->Argument, &IntegerPart);
        if ((TestCompareResults(IntegerPart, Test->IntegerPart) == FALSE) ||
            (TestCompareResults(FractionalPart, Test->FractionalPart) ==
             FALSE)) {

            printf("modf(%.13a) was {%.13a, %.13a} should have been "
                   "{%.13a, %.13a}.\n",
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
TestCeilingAndFloor (
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

    volatile double Ceiling;
    ULONG Failures;
    volatile double Floor;
    PMATH_CEILING_FLOOR_DOUBLE_VALUE Test;
    ULONG TestIndex;
    ULONG ValueCount;

    Failures = 0;
    ValueCount = sizeof(TestCeilingFloorDoubles) /
                 sizeof(TestCeilingFloorDoubles[0]);

    for (TestIndex = 0; TestIndex < ValueCount; TestIndex += 1) {
        Test = &(TestCeilingFloorDoubles[TestIndex]);
        Ceiling = ceil(Test->Argument);
        Floor = floor(Test->Argument);
        if (TestCompareResults(Ceiling, Test->Ceiling) == FALSE) {
            printf("ceil(%.13a) was %.13a, should have been %.13a.\n",
                   Test->Argument,
                   Ceiling,
                   Test->Ceiling);

            Failures += 1;
        }

        if (TestCompareResults(Floor, Test->Floor) == FALSE) {
            printf("floor(%.13a) was %.13a, should have been %.13a.\n",
                   Test->Argument,
                   Floor,
                   Test->Floor);

            Failures += 1;
        }
    }

    return Failures;
}

ULONG
TestModulo (
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
    volatile double Remainder;
    PMATH_MODULO_DOUBLE_VALUE Test;
    ULONG TestIndex;
    ULONG ValueCount;

    Failures = 0;
    ValueCount = sizeof(TestModuloDoubles) /
                 sizeof(TestModuloDoubles[0]);

    for (TestIndex = 0; TestIndex < ValueCount; TestIndex += 1) {
        Test = &(TestModuloDoubles[TestIndex]);
        Remainder = fmod(Test->Numerator, Test->Denominator);
        if (TestCompareResults(Remainder, Test->Remainder) == FALSE) {
            printf("fmod(%.13a, %.13a) was %.13a, should have been %.13a.\n",
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
TestCompareResults (
    double Value1,
    double Value2
    )

/*++

Routine Description:

    This routine compares two double values, and returns whether or not they
    are almost equal.

Arguments:

    Value1 - Supplies the first value.

    Value2 - Supplies the second value.

Return Value:

    TRUE if the values are nearly equal.

    FALSE if they are not equal.

--*/

{

    LONG High1;
    LONG High2;
    LONG LowDifference;
    DOUBLE_PARTS Parts1;
    DOUBLE_PARTS Parts2;
    ULONG SignMask;

    SignMask = DOUBLE_SIGN_BIT >> DOUBLE_HIGH_WORD_SHIFT;
    Parts1.Double = Value1;
    Parts2.Double = Value2;
    High1 = Parts1.Ulong.High;
    High2 = Parts2.Ulong.High;
    if (((High1 ^ High2) & SignMask) != 0) {
        return FALSE;
    }

    if ((High1 & ~SignMask) >= NAN_HIGH_WORD) {
        if ((High2 & ~SignMask) >= NAN_HIGH_WORD) {
            return TRUE;
        }

        return FALSE;
    }

    if (High1 != High2) {
        return FALSE;
    }

    LowDifference = Parts1.Ulong.Low - Parts2.Ulong.Low;
    if (LowDifference < 0) {
        LowDifference = -LowDifference;
    }

    if (LowDifference < MATH_RESULT_SLOP) {
        return TRUE;
    }

    return FALSE;
}

