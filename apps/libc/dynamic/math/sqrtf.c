/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    sqrtf.c

Abstract:

    This module implements support for the square root function.

    Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.

    Developed at SunPro, a Sun Microsystems, Inc. business.
    Permission to use, copy, modify, and distribute this
    software is freely granted, provided that this notice
    is preserved.

Author:

    Chris Stevens 3-Jan-2017

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
float
sqrtf (
    float Value
    )

/*++

Routine Description:

    This routine implements the square root function.

Arguments:

    Value - Supplies the value to get the square root of.

Return Value:

    Returns the square root of the value.

    +-0 for inputs of +-0.

    Infinity for inputs of infinity.

    NaN for inputs of NaN or negative values.

--*/

{

    LONG BitIndex;
    ULONG CoveredBits;
    ULONG CurrentBit;
    LONG Exponent;
    ULONG FirstExponentBit;
    float RoundingValue;
    LONG Sum;
    FLOAT_PARTS ValueParts;
    LONG Word;
    LONG WorkingValue;

    ValueParts.Float = Value;
    Word = ValueParts.Ulong;
    FirstExponentBit = 1 << FLOAT_EXPONENT_SHIFT;

    //
    // Handle infinity and NaN.
    //

    if ((Word & FLOAT_NAN) == FLOAT_NAN) {
        return Value * Value + Value;
    }

    //
    // Take care of zero. and negative values.
    //

    if (Word <= 0) {

        //
        // The square root of +-0 is +-0.
        //

        if ((Word & ~FLOAT_SIGN_BIT) == 0) {
            return Value;

        //
        // The square root of a negative value is NaN.
        //

        } else if (Word < 0) {
            return (Value - Value) / (Value - Value);
        }
    }

    //
    // Step 1: Normalize the value.
    //

    Exponent = Word >> FLOAT_EXPONENT_SHIFT;

    //
    // Watch out for subnormal values.
    //

    if (Exponent == 0) {
        for (BitIndex = 0; (Word & FirstExponentBit) == 0; BitIndex += 1) {
            Word <<= 1;
        }

        Exponent -= BitIndex - 1;
    }

    Exponent -= FLOAT_EXPONENT_BIAS;
    Word = (Word & FLOAT_VALUE_MASK) | FirstExponentBit;

    //
    // If the exponent is odd, double the value to make it even.
    //

    if ((Exponent & 0x1) != 0) {
        Word += Word;
    }

    //
    // Divide the exponent by 2.
    //

    Exponent >>= 1;

    //
    // Step 2: Generate the square root value bit by bit.
    //

    Word += Word;
    CoveredBits = 0;
    Sum = 0;

    //
    // Loop along the word (the two exponent bits plus the value part)
    // from high to low.
    //

    CurrentBit = FirstExponentBit << 1;
    while (CurrentBit != 0) {
        WorkingValue = Sum + CurrentBit;
        if (WorkingValue <= Word) {
            Sum = WorkingValue + CurrentBit;
            Word -= WorkingValue;
            CoveredBits += CurrentBit;
        }

        Word += Word;
        CurrentBit >>= 1;
    }

    //
    // Step 3: Use floating add to find out the rounding direction.
    //

    if (Word != 0) {

        //
        // Trigger the inexact flag.
        //

        RoundingValue = ClFloatOne - ClFloatTinyValue;
        if (RoundingValue >= ClFloatOne) {
            RoundingValue = ClFloatOne + ClFloatTinyValue;
            if (RoundingValue > ClFloatOne) {
                if (CoveredBits == MAX_ULONG - 1) {
                    CoveredBits += 1;
                }

                CoveredBits += 2;

            } else {
                CoveredBits += (CoveredBits & 0x1);
            }
        }
    }

    Word = (CoveredBits >> 1) +
           ((FLOAT_EXPONENT_BIAS - 1) << FLOAT_EXPONENT_SHIFT);

    Word += (Exponent << FLOAT_EXPONENT_SHIFT);
    ValueParts.Ulong = Word;
    return ValueParts.Float;
}

//
// --------------------------------------------------------- Internal Functions
//

