/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    fmod.c

Abstract:

    This module implements support for the modulo function.

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
fmod (
    double Dividend,
    double Divisor
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
    LONG DividendHigh;
    ULONG DividendLow;
    LONG DividendSign;
    LONG DivisorExponent;
    LONG DivisorHigh;
    ULONG DivisorLow;
    LONG ExponentIndex;
    ULONG ExponentShift;
    LONG OneExponent;
    DOUBLE_PARTS Parts;
    LONG SubtractionHigh;
    ULONG SubtractionLow;

    Parts.Double = Dividend;
    DividendHigh = Parts.Ulong.High;
    DividendLow = Parts.Ulong.Low;
    Parts.Double = Divisor;
    DivisorHigh = Parts.Ulong.High;
    DivisorLow = Parts.Ulong.Low;
    DividendSign = DividendHigh & (DOUBLE_SIGN_BIT >> DOUBLE_HIGH_WORD_SHIFT);
    ExponentShift = DOUBLE_EXPONENT_SHIFT - DOUBLE_HIGH_WORD_SHIFT;
    OneExponent = 1 << ExponentShift;

    //
    // Get the absolute value of the dividend.
    //

    DividendHigh ^= DividendSign;

    //
    // Get the absolute value of the divisor.
    //

    DivisorHigh &= ((~DOUBLE_SIGN_BIT) >> DOUBLE_HIGH_WORD_SHIFT);

    //
    // Handle cases where the divisor is zero, the dividend is not finite, or
    // the divisor is NaN.
    //

    if (((DivisorHigh | DivisorLow) == 0) || (DividendHigh >= NAN_HIGH_WORD) ||
        ((DivisorHigh | ((DivisorLow | -DivisorLow) >> 31)) > NAN_HIGH_WORD)) {

        return (Dividend * Divisor) / (Dividend * Divisor);
    }

    if (DividendHigh <= DivisorHigh) {

        //
        // If |Dividend| < |Divisor|, return Dividend.
        //

        if ((DividendHigh < DivisorHigh) || (DividendLow < DivisorLow)) {
            return Dividend;
        }

        //
        // If |Dividend| = |Divisor| then return Dividend * 0.
        //

        if (DividendLow == DivisorLow) {
            if (DividendSign != 0) {
                return -ClDoubleZero;
            }

            return ClDoubleZero;
        }
    }

    //
    // Determine ilogb(Dividend).
    //

    if (DividendHigh < OneExponent) {
        if (DividendHigh == 0) {
            DividendExponent = -1043;
            for (ExponentIndex = DividendLow;
                 ExponentIndex > 0;
                 ExponentIndex <<= 1) {

                DividendExponent -= 1;
            }

        } else {
            DividendExponent = -1022;
            for (ExponentIndex = (DividendHigh << 11);
                 ExponentIndex > 0;
                 ExponentIndex <<= 1) {

                DividendExponent -= 1;
            }
        }

    } else {
        DividendExponent = (DividendHigh >> ExponentShift) - 1023;
    }

    //
    // Determine ilogb(Divisor).
    //

    if (DivisorHigh < OneExponent) {
        if (DivisorHigh == 0) {
            DivisorExponent = -1043;
            for (ExponentIndex = DivisorLow;
                 ExponentIndex > 0;
                 ExponentIndex <<= 1) {

                DivisorExponent -= 1;
            }

        } else {
            DivisorExponent = -1022;
            for (ExponentIndex = (DivisorHigh << 11);
                 ExponentIndex > 0;
                 ExponentIndex <<= 1) {

                DivisorExponent -= 1;
            }
        }

    } else {
        DivisorExponent = (DivisorHigh >> ExponentShift) - 1023;
    }

    //
    // Set up {DividendHigh,DividendLow}, {DivisorHigh,DivisorLow} and align
    // Divisor to Dividend.
    //

    if (DividendExponent >= -1022) {
        DividendHigh = OneExponent | (DOUBLE_HIGH_VALUE_MASK & DividendHigh);

    } else {

        //
        // This is a subnormal dividend, shift it to be normal.
        //

        BitNumber = -1022 - DividendExponent;
        if (BitNumber <= 31) {
            DividendHigh = (DividendHigh << BitNumber) |
                           (DividendLow >> (32 - BitNumber));

            DividendLow <<= BitNumber;

        } else {
            DividendHigh = DividendLow << (BitNumber - 32);
            DividendLow = 0;
        }
    }

    if (DivisorExponent >= -1022) {
        DivisorHigh = OneExponent | (DOUBLE_HIGH_VALUE_MASK & DivisorHigh);

    } else {

        //
        // This is a subnormal divisor, shift it to be normal.
        //

        BitNumber = -1022 - DivisorExponent;
        if (BitNumber <= 31) {
            DivisorHigh = (DivisorHigh << BitNumber) |
                          (DivisorLow >> (32 - BitNumber));

            DivisorLow <<= BitNumber;

        } else {
            DivisorHigh = DivisorLow << (BitNumber - 32);
            DivisorLow = 0;
        }
    }

    //
    // Now do fixed point modulo.
    //

    BitNumber = DividendExponent - DivisorExponent;
    while (BitNumber != 0) {
        BitNumber -= 1;
        SubtractionHigh = DividendHigh - DivisorHigh;
        SubtractionLow = DividendLow - DivisorLow;
        if (DividendLow < DivisorLow) {
            SubtractionHigh -= 1;
        }

        if (SubtractionHigh < 0) {
            DividendHigh = DividendHigh + DividendHigh + (DividendLow >> 31);
            DividendLow = DividendLow+DividendLow;

        } else {
            if ((SubtractionHigh | SubtractionLow) == 0) {

                //
                // Return sign(Dividend) * 0.
                //

                if (DividendSign != 0) {
                    return -ClDoubleZero;
                }

                return ClDoubleZero;
            }

            DividendHigh = SubtractionHigh + SubtractionHigh +
                           (SubtractionLow >> 31);

            DividendLow = SubtractionLow + SubtractionLow;
        }
    }

    SubtractionHigh = DividendHigh - DivisorHigh;
    SubtractionLow = DividendLow - DivisorLow;
    if (DividendLow < DivisorLow) {
        SubtractionHigh -= 1;
    }

    if (SubtractionHigh >= 0) {
        DividendHigh = SubtractionHigh;
        DividendLow = SubtractionLow;
    }

    //
    // Convert back to floating point and restore the sign.
    //

    if ((DividendHigh | DividendLow) == 0) {

        //
        // Return sign(Dividend) * 0.
        //

        if (DividendSign != 0) {
            return -ClDoubleZero;
        }

        return ClDoubleZero;
    }

    //
    // Normalize the dividend.
    //

    while (DividendHigh < OneExponent) {
        DividendHigh = DividendHigh + DividendHigh + (DividendLow >> 31);
        DividendLow = DividendLow + DividendLow;
        DivisorExponent -= 1;
    }

    //
    // Normalize the output.
    //

    if (DivisorExponent >= -1022) {
        DividendHigh = ((DividendHigh - OneExponent) |
                       ((DivisorExponent + 1023) << ExponentShift));

        Parts.Ulong.High = DividendHigh | DividendSign;
        Parts.Ulong.Low = DividendLow;
        Dividend = Parts.Double;

    //
    // Handle a subnormal output.
    //

    } else {
        BitNumber = -1022 - DivisorExponent;
        if (BitNumber <= ExponentShift) {
            DividendLow = (DividendLow >> BitNumber) |
                          ((ULONG)DividendHigh << (32 - BitNumber));

            DividendHigh >>= BitNumber;

        } else if (BitNumber <= 31) {
            DividendLow = (DividendHigh << (32 - BitNumber)) |
                          (DividendLow >> BitNumber);

            DividendHigh = DividendSign;

        } else {
            DividendLow = DividendHigh >> (BitNumber - 32);
            DividendHigh = DividendSign;
        }

        Parts.Ulong.High = DividendHigh | DividendSign;
        Parts.Ulong.Low = DividendLow;
        Dividend = Parts.Double;

        //
        // Create a signal if necessary.
        //

        Dividend *= ClDoubleOne;
    }

    return Dividend;
}

//
// --------------------------------------------------------- Internal Functions
//

