/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    mathp.h

Abstract:

    This header contains internal definitions for the math library.

Author:

    Evan Green 23-Jul-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <math.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define some numeric constants.
//

#define PI_OVER_4_HIGH_WORD 0x3FE921FB
#define NAN_HIGH_WORD 0x7FF00000
#define MATH_5_PI_OVER_4_HIGH_WORD 0x400F6A7A
#define DOUBLE_NEGATIVE_ZERO_HIGH_WORD 0xBFF00000
#define DOUBLE_ONE_HALF_HIGH_WORD 0x3FE00000
#define DOUBLE_ONE_HIGH_WORD 0x3FF00000
#define DOUBLE_TWO_HIGH_WORD 0x40000000
#define DOUBLE_THREE_HIGH_WORD 0x40080000
#define DOUBLE_FOUR_HIGH_WORD 0x40100000

#define FLOAT_PI_OVER_4_WORD 0x3F490FD8
#define FLOAT_3_PI_OVER_4_WORD 0x4016CBE4
#define FLOAT_NEGATIVE_ZERO_WORD 0xBf800000
#define FLOAT_ONE_HALF_WORD 0x3F000000
#define FLOAT_ONE_WORD 0x3F800000
#define FLOAT_TWO_WORD 0x40000000

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _FLOATING_PRECISION {
    FloatingPrecisionSingle,
    FloatingPrecisionDouble,
    FloatingPrecisionExtended,
    FloatingPrecisionQuad,
    FloatingPrecisionCount
} FLOATING_PRECISION, *PFLOATING_PRECISION;

//
// -------------------------------------------------------------------- Globals
//

//
// Define some useful constants.
//

extern const double ClDoubleHugeValue;
extern const double ClDoubleTinyValue;
extern const double ClDoubleZero;
extern const double ClDoubleOne;
extern const double ClDoubleOneHalf;
extern const double ClPi;
extern const double ClPiOver4;
extern const double ClPiOver4Tail;
extern const double ClInverseLn2;
extern const double ClTwo54;
extern const double ClDoubleLn2High[2];
extern const double ClDoubleLn2Low[2];
extern const double ClTwo52[2];

extern const float ClFloatHugeValue;
extern const float ClFloatTinyValue;
extern const float ClFloatZero;
extern const float ClFloatOne;
extern const float ClFloatOneHalf;
extern const float ClFloatPi;
extern const float ClFloatPiOver4;
extern const float ClFloatPiOver4Tail;
extern const float ClFloatInverseLn2;
extern const float ClFloatTwo25;
extern const float ClFloatLn2High[2];
extern const float ClFloatLn2Low[2];
extern const float ClFloatTwo23[2];

//
// -------------------------------------------------------- Function Prototypes
//

double
ClpLogOnePlus (
    double Value
    );

/*++

Routine Description:

    This routine returns log(1 + value) - value for 1 + value in
    ~[sqrt(2)/2, sqrt(2)].

Arguments:

    Value - Supplies the input value to compute log(1 + value) for.

Return Value:

    Returns log(1 + value).

--*/

float
ClpLogOnePlusFloat (
    float Value
    );

/*++

Routine Description:

    This routine returns log(1 + value) - value for 1 + value in
    ~[sqrt(2)/2, sqrt(2)].

Arguments:

    Value - Supplies the input value to compute log(1 + value) for.

Return Value:

    Returns log(1 + value).

--*/

