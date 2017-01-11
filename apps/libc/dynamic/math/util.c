/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    util.c

Abstract:

    This module implements generic utility functions for the math library.

Author:

    Evan Green 23-Jul-2013

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "../libcp.h"
#include "mathp.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// Define some useful constants.
//

const double ClDoubleHugeValue = 1.0e+300;
const double ClDoubleTinyValue = 1.0e-300;
const double ClDoubleZero = 0.0;
const double ClDoubleOne = 1.0;
const double ClDoubleOneHalf = 5.00000000000000000000e-01;
const double ClPi = 3.14159265358979311600e+00;
const double ClPiOver4 = 7.85398163397448278999e-01;
const double ClPiOver4Tail = 3.06161699786838301793e-17;
const double ClInverseLn2 = 1.44269504088896338700e+00;
const double ClTwo54 = 1.80143985094819840000e+16;

const double ClDoubleLn2High[2] = {
    6.93147180369123816490e-01,
    -6.93147180369123816490e-01
};

const double ClDoubleLn2Low[2] = {
    1.90821492927058770002e-10,
    -1.90821492927058770002e-10
};

const double ClTwo52[2] = {
    4.50359962737049600000e+15,
    -4.50359962737049600000e+15
};

const float ClFloatHugeValue = 1.0e+30;
const float ClFloatTinyValue = 1.0e-30;
const float ClFloatZero = 0.0;
const float ClFloatOne = 1.0;
const float ClFloatOneHalf = 0.5;
const float ClFloatPi = 3.1415925026e+00;
const float ClFloatPiOver4 = 7.8539812565e-01;;
const float ClFloatPiOver4Tail = 3.7748947079e-08;
const float ClFloatInverseLn2 = 1.4426950216e+00;
const float ClFloatTwo25 = 3.355443200e+07;

const float ClFloatLn2High[2] = {
    6.9313812256e-01,
    -6.9313812256e-01
};

const float ClFloatLn2Low[2] = {
    9.0580006145e-06,
    -9.0580006145e-06
};

const float ClFloatTwo23[2] = {
    8.3886080000e+06,
    -8.3886080000e+06
};

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
double
copysign (
    double Value,
    double Sign
    )

/*++

Routine Description:

    This routine replaces the sign bit on the given value with the sign bit
    from the other given value.

Arguments:

    Value - Supplies the value to modify.

    Sign - Supplies the double with the desired sign bit.

Return Value:

    Returns the value with the modified sign bit.

--*/

{

    DOUBLE_PARTS SignParts;
    DOUBLE_PARTS ValueParts;

    ValueParts.Double = Value;
    SignParts.Double = Sign;
    ValueParts.Ulonglong &= ~DOUBLE_SIGN_BIT;
    ValueParts.Ulonglong |= (SignParts.Ulonglong & DOUBLE_SIGN_BIT);
    return ValueParts.Double;
}

LIBC_API
float
copysignf (
    float Value,
    float Sign
    )

/*++

Routine Description:

    This routine replaces the sign bit on the given value with the sign bit
    from the other given value.

Arguments:

    Value - Supplies the value to modify.

    Sign - Supplies the float with the desired sign bit.

Return Value:

    Returns the value with the modified sign bit.

--*/

{

    FLOAT_PARTS SignParts;
    FLOAT_PARTS ValueParts;

    ValueParts.Float = Value;
    SignParts.Float = Sign;
    ValueParts.Ulong &= ~FLOAT_SIGN_BIT;
    ValueParts.Ulong |= (SignParts.Ulong & FLOAT_SIGN_BIT);
    return ValueParts.Float;
}

LIBC_API
double
round (
    double Value
    )

/*++

Routine Description:

    This routine rounds the given value to the nearest integer. Rounding
    halfway leans away from zero regardless of the current rounding direction.

Arguments:

    Value - Supplies the value to round.

Return Value:

    Returns the rounded value.

--*/

{

    double Floor;
    DOUBLE_PARTS Parts;

    Parts.Double = Value;
    if (Parts.Ulong.High == NAN_HIGH_WORD) {
        return Value;
    }

    if (Value >= 0.0) {
        Floor = floor(Value);
        if (Floor - Value <= -0.5) {
            Floor += 1.0;
        }

        return Floor;
    }

    Floor = floor(-Value);
    if (Floor + Value <= -0.5) {
        Floor += 1.0;
    }

    return -Floor;
}

LIBC_API
float
roundf (
    float Value
    )

/*++

Routine Description:

    This routine rounds the given value to the nearest integer. Rounding
    halfway leans away from zero regardless of the current rounding direction.

Arguments:

    Value - Supplies the value to round.

Return Value:

    Returns the rounded value.

--*/

{

    float Floor;
    FLOAT_PARTS Parts;

    Parts.Float = Value;
    if (Parts.Ulong == FLOAT_NAN) {
        return Value;
    }

    if (Value >= (float)0.0) {
        Floor = floorf(Value);
        if (Floor - Value <= (float)-0.5) {
            Floor += (float)1.0;
        }

        return Floor;
    }

    Floor = floorf(-Value);
    if (Floor + Value <= (float)-0.5) {
        Floor += (float)1.0;
    }

    return -Floor;
}

//
// --------------------------------------------------------- Internal Functions
//

