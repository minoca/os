/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    ceil.c

Abstract:

    This module implements support for the ceiling math functions.

    Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.

    Developed at SunPro, a Sun Microsystems, Inc. business.
    Permission to use, copy, modify, and distribute this
    software is freely granted, provided that this notice
    is preserved.

Author:

    Evan Green 29-Aug-2013

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
ceil (
    double Value
    )

/*++

Routine Description:

    This routine computes the smallest integral value not less then the given
    value.

Arguments:

    Value - Supplies the value to compute the ceiling of.

Return Value:

    Returns the ceiling on success.

    NaN if the given value is NaN.

    Returns the value itself for +/- 0 and +/- Infinity.

--*/

{

    LONG Exponent;
    ULONG ExponentMask;
    ULONG ExponentShift;
    LONG HighWord;
    LONG LowWord;
    ULONG NewLowWord;
    ULONG NonExponent;
    DOUBLE_PARTS Parts;

    Parts.Double = Value;
    HighWord = Parts.Ulong.High;
    LowWord = Parts.Ulong.Low;
    ExponentMask = DOUBLE_EXPONENT_MASK >> DOUBLE_HIGH_WORD_SHIFT;
    ExponentShift = DOUBLE_EXPONENT_SHIFT - DOUBLE_HIGH_WORD_SHIFT;
    Exponent = ((HighWord & ExponentMask) >> ExponentShift) -
               DOUBLE_EXPONENT_BIAS;

    if (Exponent < 20) {

        //
        // Raise an inexact if the value isn't zero.
        //

        if (Exponent < 0) {

            //
            // Return 0 * sign(Value) if |Value| < 1.
            //

            if (ClHugeValue + Value > 0.0) {
                if (HighWord < 0) {
                    HighWord = DOUBLE_SIGN_BIT >> DOUBLE_HIGH_WORD_SHIFT;
                    LowWord = 0;

                } else if ((HighWord | LowWord) != 0) {
                    HighWord = DOUBLE_ONE_HIGH_WORD;
                    LowWord = 0;
                }
            }

        } else {
            NonExponent = DOUBLE_HIGH_VALUE_MASK >> Exponent;

            //
            // Return the value itself if it's integral.
            //

            if (((HighWord & NonExponent) | LowWord) == 0) {
                return Value;
            }

            //
            // Raise the inexact flag.
            //

            if (ClHugeValue + Value > 0.0) {
                if (HighWord > 0) {
                    HighWord += (1 << ExponentShift) >> Exponent;
                }

                HighWord &= (~NonExponent);
                LowWord = 0;
            }
        }

    } else if (Exponent > 51) {

        //
        // Handle infinity or NaN.
        //

        if (Exponent == (DOUBLE_NAN_EXPONENT - DOUBLE_EXPONENT_BIAS)) {
            return Value + Value;

        //
        // Return the value itself if it is integral.
        //

        } else {
            return Value;
        }

    } else {
        NonExponent = ((ULONG)(MAX_ULONG)) >> (Exponent - ExponentShift);

        //
        // Check to see if the value is integral.
        //

        if ((LowWord & NonExponent) == 0) {
            return Value;
        }

        //
        // Raise the inexact flag.
        //

        if (ClHugeValue + Value > 0.0) {
            if (HighWord > 0) {
                if (Exponent == 20) {
                    HighWord += 1;

                } else {
                    NewLowWord = LowWord + (1 << (52 - Exponent));
                    if (NewLowWord < LowWord) {

                        //
                        // Perform the carry.
                        //

                        HighWord += 1;
                    }

                    LowWord = NewLowWord;
                }
            }

            LowWord &= (~NonExponent);
        }
    }

    Parts.Ulong.High = HighWord;
    Parts.Ulong.Low = LowWord;
    return Parts.Double;
}

//
// --------------------------------------------------------- Internal Functions
//

