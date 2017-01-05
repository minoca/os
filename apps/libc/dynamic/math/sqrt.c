/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    sqrt.c

Abstract:

    This module implements support for the square root function.

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
sqrt (
    double Value
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
    LONG HighWord;
    ULONG LowWord;
    LONG RootBit;
    double RoundingValue;
    LONG SignBit;
    ULONG SignBitShift;
    LONG Sum;
    ULONG SumLow;
    DOUBLE_PARTS ValueParts;
    ULONG WorkingLow;
    LONG WorkingValue;

    //
    // This method computes the square root bit by bit using integer arithmetic.
    // There are three steps.
    // 1. Normalization
    //    Scale the value to y in [1, 4) with even powers of 2:
    //    Find an integer k such at 1 <= (y = x * 2^(2k)) < 4, then
    //        sqrt(x) = 2^k * sqrt(y)
    //
    // 2. Bit by bit computation
    //    Let q = sqrt(y) truncated to i bits after binary point (q = 1),
    //                         i+1         2
    //    s  = 2*q , and y  = 2    * (y - q ).                              (1)
    //     i      i       i                i
    //
    //    To compute q(i+1) from q(i), one checks whether
    //           -(i+i)  2
    //    (q  + 2       )  <= y                                             (2)
    //      i
    //
    //    If equation (2) is false, then q(i+1) = q(i), otherwise
    //    q(i+1) = q(i) + 2.
    //    With some algebraic manipulation, it is not difficult to see that
    //    equation (2) is equivalent to
    //          -(i+1)
    //    s  + 2       <= y                                                 (3)
    //     i               i
    //
    //    The advantage of equation (3) is that s(i) and y(i) can be computed
    //    by the following reference formula:
    //    If equation (3) is false:
    //        s    = s , y    = y ;                                         (4)
    //         i+1    i   i+1    i
    //
    //    Otherwise
    //                 -i                    -(i+1)
    //    s    = s  + 2  , y    = y  - s  - 2                               (5)
    //     i+1    i         i+1    i    i
    //
    //    Apparently it's easy to use induction to prove (4) and (5).
    //    Note that since the left hand side of equation (3) contains only
    //    i+2 bits, it is not necessary to do a full 53 bit comparison in (3).
    //
    // 3. Final rounding
    //    After generating the 53 bit result, compute one more bit. Together
    //    with the remainder, the result will either be exact, bigger than
    //    1/2 ULP, or less than 1/2 ULP (it will never be equal to 1/2 ULP).
    //    The rounding can be detected by checking if huge + tiny is equal to
    //    huge, and whether huge - tiny is equal to huge.
    //

    ValueParts.Double = Value;
    HighWord = ValueParts.Ulong.High;
    LowWord = ValueParts.Ulong.Low;
    SignBit = DOUBLE_SIGN_BIT >> DOUBLE_HIGH_WORD_SHIFT;
    SignBitShift = DOUBLE_SIGN_BIT_SHIFT - DOUBLE_HIGH_WORD_SHIFT;
    FirstExponentBit = 1 << (DOUBLE_EXPONENT_SHIFT - DOUBLE_HIGH_WORD_SHIFT);

    //
    // Handle infinity and NaN.
    //

    if ((HighWord & NAN_HIGH_WORD) == NAN_HIGH_WORD) {
        return Value * Value + Value;
    }

    //
    // Take care of zero. and negative values.
    //

    if (HighWord <= 0) {

        //
        // The square root of +-0 is +-0.
        //

        if (((HighWord & (~SignBit)) | LowWord) == 0) {
            return Value;

        //
        // The square root of a negative value is NaN.
        //

        } else if (HighWord < 0) {
            return (Value - Value) / (Value - Value);
        }
    }

    //
    // Step 1: Normalize the value.
    //

    Exponent = (HighWord >> 20);

    //
    // Watch out for subnormal values.
    //

    if (Exponent == 0) {
        while (HighWord == 0) {
            Exponent -= 21;
            HighWord |= (LowWord >> 11);
            LowWord <<= 21;
        }

        for (BitIndex = 0; (HighWord & FirstExponentBit) == 0; BitIndex += 1) {
            HighWord <<= 1;
        }

        Exponent -= BitIndex - 1;
        HighWord |= (LowWord >> ((sizeof(ULONG) * BITS_PER_BYTE) - BitIndex));
        LowWord <<= BitIndex;
    }

    Exponent -= DOUBLE_EXPONENT_BIAS;
    HighWord = (HighWord & DOUBLE_HIGH_VALUE_MASK) | FirstExponentBit;

    //
    // If the exponent is odd, double the value to make it even.
    //

    if ((Exponent & 0x1) != 0) {
        HighWord += HighWord +
                    ((LowWord & SignBit) >> SignBitShift);

        LowWord += LowWord;
    }

    //
    // Divide the exponent by 2.
    //

    Exponent >>= 1;

    //
    // Step 2: Generate the square root value bit by bit.
    //

    HighWord += HighWord + ((LowWord & SignBit) >> SignBitShift);
    LowWord += LowWord;
    RootBit = 0;
    CoveredBits = 0;
    Sum = 0;
    SumLow = 0;

    //
    // Loop along the high word (the two exponent bits plus the value part)
    // from high to low.
    //

    CurrentBit = FirstExponentBit << 1;
    while (CurrentBit != 0) {
        WorkingValue = Sum + CurrentBit;
        if (WorkingValue <= HighWord) {
            Sum = WorkingValue + CurrentBit;
            HighWord -= WorkingValue;
            RootBit += CurrentBit;
        }

        HighWord += HighWord + ((LowWord & SignBit) >> SignBitShift);
        LowWord += LowWord;
        CurrentBit >>= 1;
    }

    CurrentBit = SignBit;
    while (CurrentBit != 0) {
        WorkingLow = SumLow + CurrentBit;
        WorkingValue = Sum;
        if ((WorkingValue < HighWord) ||
            ((WorkingValue == HighWord) && (WorkingLow <= LowWord))) {

            SumLow = WorkingLow + CurrentBit;
            if (((WorkingLow & SignBit) == SignBit) &&
                ((SumLow & SignBit) == 0)) {

                Sum += 1;
            }

            HighWord -= WorkingValue;
            if (LowWord < WorkingLow) {
                HighWord -= 1;
            }

            LowWord -= WorkingLow;
            CoveredBits += CurrentBit;
        }

        HighWord += HighWord + ((LowWord & SignBit) >> SignBitShift);
        LowWord += LowWord;
        CurrentBit >>= 1;
    }

    //
    // Step 3: Use floating add to find out the rounding direction.
    //

    if ((HighWord | LowWord) != 0) {

        //
        // Trigger the inexact flag.
        //

        RoundingValue = ClDoubleOne - ClDoubleTinyValue;
        if (RoundingValue >= ClDoubleOne) {
            RoundingValue = ClDoubleOne + ClDoubleTinyValue;
            if (CoveredBits == MAX_ULONG) {
                CoveredBits = 0;
                RootBit += 1;

            } else if (RoundingValue > ClDoubleOne) {
                if (CoveredBits == MAX_ULONG - 1) {
                    RootBit += 1;
                }

                CoveredBits += 2;

            } else {
                CoveredBits += (CoveredBits & 0x1);
            }
        }
    }

    HighWord = (RootBit >> 1) +
               ((DOUBLE_EXPONENT_BIAS - 1) <<
                (DOUBLE_EXPONENT_SHIFT - DOUBLE_HIGH_WORD_SHIFT));

    LowWord = CoveredBits >> 1;
    if ((RootBit & 1) == 1) {
        LowWord |= SignBit;
    }

    HighWord += (Exponent << 20);
    ValueParts.Ulong.High = HighWord;
    ValueParts.Ulong.Low = LowWord;
    return ValueParts.Double;
}

//
// --------------------------------------------------------- Internal Functions
//

