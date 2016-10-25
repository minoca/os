/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    modf.c

Abstract:

    This module implements support for the mod function, which splits a
    floating point value into an integer portion and a fraction portion.

    Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.

    Developed at SunPro, a Sun Microsystems, Inc. business.
    Permission to use, copy, modify, and distribute this
    software is freely granted, provided that this notice
    is preserved.

Author:

    Evan Green 24-Jul-2013

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
// ------------------------------------------------------------------ Functions
//

LIBC_API
double
modf (
    double Value,
    double *IntegerPortion
    )

/*++

Routine Description:

    This routine breaks the given value up into integral and fractional parts,
    each of which has the same sign as the argument. It stores the integral
    part as a floating point value.

Arguments:

    Value - Supplies the value to decompose into an integer and a fraction.

    IntegerPortion - Supplies a pointer where the integer portion of the
        value will be returned. If the given value is NaN or +/- Infinity, then
        NaN or +/- Infinity will be returned.

Return Value:

    Returns the fractional portion of the given value on success.

    NaN if the input is NaN.

    0 if +/- Infinity is given.

--*/

{

    LONG Exponent;
    ULONG ExponentMask;
    ULONG ExponentShift;
    ULONG FractionMask;
    LONG HighWord;
    LONG LowWord;
    DOUBLE_PARTS Parts;
    ULONG SignBit;

    Parts.Double = Value;
    HighWord = Parts.Ulong.High;
    LowWord = Parts.Ulong.Low;
    SignBit = DOUBLE_SIGN_BIT >> DOUBLE_HIGH_WORD_SHIFT;
    ExponentShift = DOUBLE_EXPONENT_SHIFT - DOUBLE_HIGH_WORD_SHIFT;
    ExponentMask = DOUBLE_EXPONENT_MASK >> DOUBLE_HIGH_WORD_SHIFT;
    Exponent = (((ULONG)HighWord & ExponentMask) >> ExponentShift) -
               DOUBLE_EXPONENT_BIAS;

    //
    // If the exponent is small, then there's some fractional part in the
    // high word.
    //

    if (Exponent < 20) {

        //
        // If the exponent is negative the absolute value is less than 1,
        // so there is no integer portion. Copy the sign and return the value
        // as the fractional portion.
        //

        if (Exponent < 0) {
            Parts.Ulong.High = HighWord & SignBit;
            Parts.Ulong.Low = 0;
            *IntegerPortion = Parts.Double;
            return Value;

        } else {
            FractionMask = DOUBLE_HIGH_VALUE_MASK >> Exponent;

            //
            // If the fraction mask portion and the whole low word is zero, the
            // value is integral. Return +/- 0 to match the sign.
            //

            if (((HighWord & FractionMask) | LowWord) == 0) {
                *IntegerPortion = Value;
                Parts.Ulong.High = HighWord & SignBit;
                Parts.Ulong.Low = 0;
                return Parts.Double;
            }

            Parts.Ulong.High = HighWord & (~FractionMask);
            Parts.Ulong.Low = 0;
            *IntegerPortion = Parts.Double;
            return Value - *IntegerPortion;
        }

    //
    // If the exponent is large, all the bits are integral bits.
    //

    } else if (Exponent > 51) {

        //
        // Handle Infinity and NaN.
        //

        if (HighWord >= NAN_HIGH_WORD) {
            *IntegerPortion = Value;
            return 0.0 / Value;
        }

        *IntegerPortion = Value * ClDoubleOne;

        //
        // Return +/- 0.
        //

        Parts.Ulong.High = HighWord & SignBit;
        Parts.Ulong.Low = 0;
        return Parts.Double;
    }

    //
    // The exponent is between 20 and 51, so the fractional part is in the
    // low word.
    //

    FractionMask = MAX_ULONG >> (Exponent - ExponentShift);

    //
    // If there's nothing in the fraction mask then the value is integral.
    // In that case return +/- 0.
    //

    if ((LowWord & FractionMask) == 0) {
        *IntegerPortion = Value;
        Parts.Ulong.High = HighWord & SignBit;
        Parts.Ulong.Low = 0;
        return Parts.Double;
    }

    Parts.Ulong.High = HighWord;
    Parts.Ulong.Low = LowWord & (~FractionMask);
    *IntegerPortion = Parts.Double;
    return Value - *IntegerPortion;
}

//
// --------------------------------------------------------- Internal Functions
//

