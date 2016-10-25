/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    div.c

Abstract:

    This module implements support for division in EFI.

Author:

    Evan Green 7-Apr-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "ueficore.h"

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

BOOLEAN
EfiDivideUnsigned64 (
    UINT64 Dividend,
    UINT64 Divisor,
    UINT64 *QuotientOut,
    UINT64 *RemainderOut
    )

/*++

Routine Description:

    This routine performs a 64-bit divide of two unsigned numbers.

Arguments:

    Dividend - Supplies the number that is going to be divided (the numerator).

    Divisor - Supplies the number to divide into (the denominator).

    QuotientOut - Supplies a pointer that receives the result of the divide.
        This parameter may be NULL.

    RemainderOut - Supplies a pointer that receives the remainder of the
        divide. This parameter may be NULL.

Return Value:

    Returns TRUE if the operation was successful, or FALSE if there was an
    error (like divide by 0).

--*/

{

    UINT32 BitCount;
    UINT32 BitIndex;
    UINT64 LastDividend;
    UINT32 MostSignificantBit;
    UINT64 Quotient;
    UINT64 Remainder;
    UINT64 Subtraction;
    UINT32 SubtractionTopBitNot;

    BitCount = 64;
    Quotient = 0;
    Remainder = 0;

    //
    // Check easy cases.
    //

    if (Divisor == 0) {
        return FALSE;
    }

    if (Divisor > Dividend) {
        Remainder = Dividend;
        goto DivideUnsigned64End;
    }

    if (Divisor == Dividend) {
        Quotient = 1;
        goto DivideUnsigned64End;
    }

    LastDividend = 0;
    while (Remainder < Divisor) {
        MostSignificantBit = (Dividend & 0x8000000000000000ULL) >> 63;
        Remainder = (Remainder << 1) | MostSignificantBit;
        LastDividend = Dividend;
        Dividend = Dividend << 1;
        BitCount -= 1;
    }

    //
    // Undo the last iteration of the loop (instead of adding an if into the
    // loop.
    //

    Dividend = LastDividend;
    Remainder = Remainder >> 1;
    BitCount += 1;
    for (BitIndex = 0; BitIndex < BitCount; BitIndex += 1) {
        MostSignificantBit = (Dividend & 0x8000000000000000ULL) >> 63;
        Remainder = (Remainder << 1) | MostSignificantBit;
        Subtraction = Remainder - Divisor;
        SubtractionTopBitNot = !(Subtraction & 0x8000000000000000ULL);
        Dividend = Dividend << 1;
        Quotient = (Quotient << 1) | SubtractionTopBitNot;
        if (SubtractionTopBitNot != 0) {
            Remainder = Subtraction;
        }
    }

DivideUnsigned64End:
    if (QuotientOut != NULL) {
        *QuotientOut = Quotient;
    }

    if (RemainderOut != NULL) {
        *RemainderOut = Remainder;
    }

    return TRUE;
}

BOOLEAN
EfiDivide64 (
    INT64 Dividend,
    INT64 Divisor,
    INT64 *QuotientOut,
    INT64 *RemainderOut
    )

/*++

Routine Description:

    This routine performs a 64-bit divide of two signed numbers.

Arguments:

    Dividend - Supplies the number that is going to be divided (the numerator).

    Divisor - Supplies the number to divide into (the denominator).

    QuotientOut - Supplies a pointer that receives the result of the divide.
        This parameter may be NULL.

    RemainderOut - Supplies a pointer that receives the remainder of the
        divide. This parameter may be NULL.

Return Value:

    Returns TRUE if the operation was successful, or FALSE if there was an
    error (like divide by 0).

--*/

{

    UINT64 AbsoluteDividend;
    UINT64 AbsoluteDivisor;
    BOOLEAN Result;
    UINT64 UnsignedQuotient;
    UINT64 UnsignedRemainder;

    //
    // Get the unsigned versions and do an unsigned division.
    //

    AbsoluteDividend = Dividend;
    AbsoluteDivisor = Divisor;
    if (Dividend < 0) {
        AbsoluteDividend = -AbsoluteDividend;
    }

    if (Divisor < 0) {
        AbsoluteDivisor = -AbsoluteDivisor;
    }

    Result = EfiDivideUnsigned64(AbsoluteDividend,
                                 AbsoluteDivisor,
                                 &UnsignedQuotient,
                                 &UnsignedRemainder);

    if (Result == FALSE) {
        return FALSE;
    }

    //
    // The quotient is negative if the signs of the operands are different.
    //

    if (QuotientOut != NULL) {
        *QuotientOut = UnsignedQuotient;
        if (((Divisor > 0) && (Dividend < 0)) ||
            ((Divisor < 0) && (Dividend >= 0)) ) {

            *QuotientOut = -UnsignedQuotient;
        }
    }

    //
    // The sign of the remainder is the same as the sign of the dividend.
    //

    if (RemainderOut != NULL) {
        *RemainderOut = UnsignedRemainder;
        if (Dividend < 0) {
            *RemainderOut = -UnsignedRemainder;
        }
    }

    return TRUE;
}

//
// --------------------------------------------------------- Internal Functions
//

