/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    fmodf.c

Abstract:

    This module implements support for the modulo function.

    Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.

    Developed at SunPro, a Sun Microsystems, Inc. business.
    Permission to use, copy, modify, and distribute this
    software is freely granted, provided that this notice
    is preserved.

Author:

    Chris Stevens 5-Jan-2017

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
fmodf (
    float Dividend,
    float Divisor
    )

/*++

Routine Description:

    This routine computes the remainder of dividing the given two values.

Arguments:

    Dividend - Supplies the numerator of the division.

    Divisor - Supplies the denominator of the division.

Return Value:

    Returns the remainder of the division on success.

    NaN if the divisor is zero, either value is NaN, or the dividend is
    infinite.

    Returns the dividend if the dividend is not infinite and the denominator is.

--*/

{

    LONG BitNumber;
    LONG DividendExponent;
    LONG DividendSign;
    LONG DividendWord;
    LONG DivisorExponent;
    LONG DivisorWord;
    LONG ExponentIndex;
    FLOAT_PARTS Parts;
    LONG SubtractionWord;

    Parts.Float = Dividend;
    DividendWord = Parts.Ulong;
    Parts.Float = Divisor;
    DivisorWord = Parts.Ulong;
    DividendSign = DividendWord & FLOAT_SIGN_BIT;

    //
    // Get the absolute value of the dividend.
    //

    DividendWord ^= DividendSign;

    //
    // Get the absolute value of the divisor.
    //

    DivisorWord &= ~FLOAT_SIGN_BIT;

    //
    // Handle cases where the divisor is zero, the dividend is not finite, or
    // the divisor is NaN.
    //

    if ((DivisorWord == 0) ||
        (DividendWord >= FLOAT_NAN) ||
        (DivisorWord > FLOAT_NAN)) {

        return (Dividend * Divisor) / (Dividend * Divisor);
    }

    //
    // If |Dividend| < |Divisor|, return Dividend.
    //

    if (DividendWord < DivisorWord) {
        return Dividend;
    }

    //
    // If |Dividend| = |Divisor| then return Dividend * 0.
    //

    if (DividendWord == DivisorWord) {
        if (DividendSign != 0) {
            return -ClFloatZero;
        }

        return ClFloatZero;
    }

    //
    // Determine ilogb(Dividend).
    //

    if (DividendWord < (1 << FLOAT_EXPONENT_SHIFT)) {
        DividendExponent = -126;
        for (ExponentIndex = (DividendWord << 8);
             ExponentIndex > 0;
             ExponentIndex <<= 1) {

            DividendExponent -= 1;
        }

    } else {
        DividendExponent = (DividendWord >> FLOAT_EXPONENT_SHIFT) -
                           FLOAT_EXPONENT_BIAS;
    }

    //
    // Determine ilogb(Divisor).
    //

    if (DivisorWord < (1 << FLOAT_EXPONENT_SHIFT)) {
        DivisorExponent = -126;
        for (ExponentIndex = (DivisorWord << 8);
             ExponentIndex > 0;
             ExponentIndex <<= 1) {

            DivisorExponent -= 1;
        }

    } else {
        DivisorExponent = (DivisorWord >> FLOAT_EXPONENT_SHIFT) -
                          FLOAT_EXPONENT_BIAS;
    }

    //
    // Set up {DividendHigh,DividendLow}, {DivisorHigh,DivisorLow} and align
    // Divisor to Dividend.
    //

    if (DividendExponent >= -126) {
        DividendWord = (1 << FLOAT_EXPONENT_SHIFT) |
                       (FLOAT_VALUE_MASK & DividendWord);

    } else {

        //
        // This is a subnormal dividend, shift it to be normal.
        //

        BitNumber = -126 - DividendExponent;
        DividendWord = DividendWord << BitNumber;
    }

    if (DivisorExponent >= -126) {
        DivisorWord = (1 << FLOAT_EXPONENT_SHIFT) |
                      (FLOAT_VALUE_MASK & DivisorWord);

    } else {

        //
        // This is a subnormal divisor, shift it to be normal.
        //

        BitNumber = -126 - DivisorExponent;
        DivisorWord = DivisorWord << BitNumber;
    }

    //
    // Now do fixed point modulo.
    //

    BitNumber = DividendExponent - DivisorExponent;
    while (BitNumber != 0) {
        BitNumber -= 1;
        SubtractionWord = DividendWord - DivisorWord;
        if (SubtractionWord < 0) {
            DividendWord = DividendWord + DividendWord;

        } else {
            if (SubtractionWord == 0) {

                //
                // Return sign(Dividend) * 0.
                //

                if (DividendSign != 0) {
                    return -ClFloatZero;
                }

                return ClFloatZero;
            }

            DividendWord = SubtractionWord + SubtractionWord;
        }
    }

    SubtractionWord = DividendWord - DivisorWord;
    if (SubtractionWord >= 0) {
        DividendWord = SubtractionWord;
    }

    //
    // Convert back to floating point and restore the sign.
    //

    if (DividendWord == 0) {

        //
        // Return sign(Dividend) * 0.
        //

        if (DividendSign != 0) {
            return -ClFloatZero;
        }

        return ClFloatZero;
    }

    //
    // Normalize the dividend.
    //

    while (DividendWord < (1 << FLOAT_EXPONENT_SHIFT)) {
        DividendWord = DividendWord + DividendWord;
        DivisorExponent -= 1;
    }

    //
    // Normalize the output.
    //

    if (DivisorExponent >= -126) {
        DividendWord = (DividendWord - (1 << FLOAT_EXPONENT_SHIFT)) |
                       ((DivisorExponent + 127) << FLOAT_EXPONENT_SHIFT);

        Parts.Ulong = DividendWord | DividendSign;
        Dividend = Parts.Float;

    //
    // Handle a subnormal output.
    //

    } else {
        BitNumber = -126 - DivisorExponent;
        DividendWord >>= BitNumber;
        Parts.Ulong = DividendWord | DividendSign;
        Dividend = Parts.Float;

        //
        // Create a signal if necessary.
        //

        Dividend *= ClFloatOne;
    }

    return Dividend;
}

//
// --------------------------------------------------------- Internal Functions
//

