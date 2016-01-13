/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

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

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// Define some useful constants.
//

extern const double ClHugeValue;
extern const double ClTinyValue;
extern const double ClDoubleZero;
extern const double ClDoubleOne;
extern const double ClDoubleOneHalf;
extern const double ClPi;
extern const double ClPiOver4;
extern const double ClPiOver4Tail;
extern const double ClInverseLn2;
extern const double ClTwo54;
extern const double ClLn2High[2];
extern const double ClLn2Low[2];

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

