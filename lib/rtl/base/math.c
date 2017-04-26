/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    math.c

Abstract:

    This module implements math support routines needed by the kernel.

Author:

    Evan Green 24-Jul-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "rtlp.h"

//
// ---------------------------------------------------------------- Definitions
//

#define LONG_BITS (sizeof(LONG) * BITS_PER_BYTE)
#define ULONG_BITS (sizeof(ULONG) * BITS_PER_BYTE)
#define LONGLONG_BITS (sizeof(LONGLONG) * BITS_PER_BYTE)
#define ULONGLONG_BITS (sizeof(ULONGLONG) * BITS_PER_BYTE)

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// Use a small table for determining how many bits are set in a nibble.
//

CHAR RtlSetBitCounts[16] = {0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4};

//
// ------------------------------------------------------------------ Functions
//

RTL_API
BOOL
RtlAreUuidsEqual (
    PUUID Uuid1,
    PUUID Uuid2
    )

/*++

Routine Description:

    This routine compares two UUIDs.

Arguments:

    Uuid1 - Supplies the first UUID.

    Uuid2 - Supplies the second UUID.

Return Value:

    TRUE if the UUIDs are equal.

    FALSE if the UUIDs are not equal.

--*/

{

    if ((Uuid1->Data[0] == Uuid2->Data[0]) &&
        (Uuid1->Data[1] == Uuid2->Data[1]) &&
        (Uuid1->Data[2] == Uuid2->Data[2]) &&
        (Uuid1->Data[3] == Uuid2->Data[3])) {

        return TRUE;
    }

    return FALSE;
}

__USED
RTL_API
ULONGLONG
RtlDivideUnsigned64 (
    ULONGLONG Dividend,
    ULONGLONG Divisor,
    PULONGLONG Remainder
    )

/*++

Routine Description:

    This routine performs a 64-bit divide of two unsigned numbers.

Arguments:

    Dividend - Supplies the number that is going to be divided (the numerator).

    Divisor - Supplies the number to divide into (the denominator).

    Remainder - Supplies a pointer that receives the remainder of the
        divide. This parameter may be NULL.

Return Value:

    Returns the quotient.

--*/

{

    ULONG Carry;
    ULONGLONG_PARTS Denominator;
    LONGLONG Difference;
    ULONGLONG_PARTS Numerator;
    ULONGLONG_PARTS QuotientParts;
    ULONGLONG_PARTS RemainderParts;
    ULONGLONG ShiftRight;

    Numerator.Ulonglong = Dividend;
    Denominator.Ulonglong = Divisor;

    ASSERT(Divisor != 0);

    //
    // Handle the numerator being zero.
    //

    if (Numerator.Ulong.High == 0) {

        //
        // If the denominator is 0 too, this just a 32 bit divide.
        //

        if (Denominator.Ulong.High == 0) {
            if (Remainder != NULL) {
                *Remainder = Numerator.Ulong.Low % Denominator.Ulong.Low;
            }

            return Numerator.Ulong.Low / Denominator.Ulong.Low;
        }

        //
        // This is a 32-bit value divided by a large 64-bit value. It will
        // be zero, remainder the value.
        //

        if (Remainder != NULL) {
            *Remainder = Numerator.Ulong.Low;
        }

        return 0;
    }

    //
    // The numerator is fully 64 bits. Check to see if the denominator has a
    // low part.
    //

    if (Denominator.Ulong.Low == 0) {

        //
        // Handle divide by zero.
        //

        if (Denominator.Ulong.High == 0) {
            if (Remainder != NULL) {
                *Remainder = Numerator.Ulong.High % Denominator.Ulong.Low;
            }

            return Numerator.Ulong.High / Denominator.Ulong.Low;
        }

        //
        // The denominator has a high part, but no low part. If the numerator
        // has no low part then this is n00000000 / d00000000.
        //

        if (Numerator.Ulong.Low == 0) {
            if (Remainder != NULL) {
                RemainderParts.Ulong.High = Numerator.Ulong.High %
                                            Denominator.Ulong.High;

                RemainderParts.Ulong.Low = 0;
                *Remainder = RemainderParts.Ulonglong;
            }

            return Numerator.Ulong.High / Denominator.Ulong.High;
        }

        //
        // The numerator is a full 64 bit value, but the denominator has 8
        // zeros at the end. Check to see if the denominator is a power of 2.
        //

        if ((Denominator.Ulong.High & (Denominator.Ulong.High - 1)) == 0) {
            if (Remainder != NULL) {
                RemainderParts.Ulong.Low = Numerator.Ulong.Low;
                RemainderParts.Ulong.High = Numerator.Ulong.High &
                                            (Denominator.Ulong.High - 1);

                *Remainder = RemainderParts.Ulonglong;
            }

            QuotientParts.Ulonglong = Numerator.Ulong.High >>
                                      __builtin_ctz(Denominator.Ulong.High);

            return QuotientParts.Ulonglong;
        }

        //
        // The denominator is not a power of 2 (but does have 8 trailing
        // zeros). Do the full divide.
        //

        ShiftRight = __builtin_clz(Denominator.Ulong.High) -
                     __builtin_clz(Numerator.Ulong.High);

        if (ShiftRight > (ULONG_BITS - 2)) {
            if (Remainder != NULL) {
                *Remainder = Numerator.Ulonglong;
            }

            return 0;
        }

        ShiftRight += 1;
        QuotientParts.Ulong.Low = 0;
        QuotientParts.Ulong.High =
                              Numerator.Ulong.Low << (ULONG_BITS - ShiftRight);

        RemainderParts.Ulong.High = Numerator.Ulong.High >> ShiftRight;
        RemainderParts.Ulong.Low =
            (Numerator.Ulong.High <<
             (ULONG_BITS - ShiftRight)) |
            (Numerator.Ulong.Low >> ShiftRight);

    //
    // The denominator has a non-zero low part.
    //

    } else {

        //
        // Handle the high part of the denominator being zero, 64 / 32.
        //

        if (Denominator.Ulong.High == 0) {

            //
            // Check for a power of 2.
            //

            if ((Denominator.Ulong.Low & (Denominator.Ulong.Low - 1)) == 0) {
                if (Remainder != NULL) {
                    *Remainder = Numerator.Ulong.Low &
                                 (Denominator.Ulong.Low - 1);
                }

                if (Denominator.Ulong.Low == 1) {
                    return Numerator.Ulonglong;
                }

                ShiftRight = __builtin_ctz(Denominator.Ulong.Low);
                QuotientParts.Ulong.High = Numerator.Ulong.High >> ShiftRight;
                QuotientParts.Ulong.Low =
                    (Numerator.Ulong.High <<
                     (ULONG_BITS - ShiftRight)) |
                    (Numerator.Ulong.Low >> ShiftRight);

                return QuotientParts.Ulonglong;
            }

            //
            // This is a full 64 / 32. The remainder is Numerator >> ShiftRight,
            // and the quotient is the numerator << (64 - ShiftRight).
            //

            ShiftRight = ULONG_BITS + 1 + __builtin_clz(Denominator.Ulong.Low) -
                         __builtin_clz(Numerator.Ulong.High);

            if (ShiftRight == ULONG_BITS) {
                QuotientParts.Ulong.Low = 0;
                QuotientParts.Ulong.High = Numerator.Ulong.Low;
                RemainderParts.Ulong.Low = Numerator.Ulong.High;
                RemainderParts.Ulong.High = 0;

            //
            // The shift right is at least 2, but less than a full rotation.
            //

            } else if (ShiftRight < ULONG_BITS) {
                QuotientParts.Ulong.Low = 0;
                QuotientParts.Ulong.High =
                              Numerator.Ulong.Low << (ULONG_BITS - ShiftRight);

                RemainderParts.Ulong.Low =
                    (Numerator.Ulong.High << (ULONG_BITS - ShiftRight)) |
                    (Numerator.Ulong.Low >> ShiftRight);

                RemainderParts.Ulong.High = Numerator.Ulong.High >> ShiftRight;

            //
            // The shift right is somewhere between 32 and 64.
            //

            } else {
                QuotientParts.Ulong.Low =
                          Numerator.Ulong.Low << (ULONGLONG_BITS - ShiftRight);

                QuotientParts.Ulong.High =
                    (Numerator.Ulong.High << (ULONGLONG_BITS - ShiftRight)) |
                    (Numerator.Ulong.Low >> (ShiftRight - ULONG_BITS));

                RemainderParts.Ulong.Low =
                             Numerator.Ulong.High >> (ShiftRight - ULONG_BITS);

                RemainderParts.Ulong.High = 0;
            }

        //
        // The denominator is the full 64 bits long, and the numerator has
        // stuff in the high bits.
        //

        } else {
            ShiftRight = __builtin_clz(Denominator.Ulong.High) -
                         __builtin_clz(Numerator.Ulong.High);

            if (ShiftRight > ULONG_BITS - 1) {
                if (Remainder != NULL) {
                    *Remainder = Numerator.Ulonglong;
                }

                return 0;
            }

            ShiftRight += 1;

            //
            // The shift is somewhere between 1 and 32, inclusive. The quotient
            // is Numerator << (64 - ShiftCount).
            //

            QuotientParts.Ulong.Low = 0;
            if (ShiftRight == ULONG_BITS) {
                QuotientParts.Ulong.High = Numerator.Ulong.Low;
                RemainderParts.Ulong.Low = Numerator.Ulong.High;
                RemainderParts.Ulong.High = 0;

            } else {
                QuotientParts.Ulong.High =
                              Numerator.Ulong.Low << (ULONG_BITS - ShiftRight);

                RemainderParts.Ulong.Low =
                    (Numerator.Ulong.High << (ULONG_BITS - ShiftRight)) |
                    (Numerator.Ulong.Low >> ShiftRight);

                RemainderParts.Ulong.High =
                    Numerator.Ulong.High >> ShiftRight;
            }
        }
    }

    //
    // Enough weaseling, just do the divide. The quotient is currently
    // initialized with Numerator << (64 - ShiftRight), and the remainder is
    // Numerator >> ShiftRight. ShiftRight is somewhere between 1 and 63,
    // inclusive.
    //

    Carry = 0;
    while (ShiftRight > 0) {
        RemainderParts.Ulong.High =
            (RemainderParts.Ulong.High << 1) |
            (RemainderParts.Ulong.Low >> (ULONG_BITS - 1));

        RemainderParts.Ulong.Low =
            (RemainderParts.Ulong.Low << 1) |
            (QuotientParts.Ulong.High >> (ULONG_BITS - 1));

        QuotientParts.Ulong.High =
            (QuotientParts.Ulong.High << 1) |
            (QuotientParts.Ulong.Low >> (ULONG_BITS - 1));

        QuotientParts.Ulong.Low = (QuotientParts.Ulong.Low << 1) | Carry;

        //
        // If the remainder is greater than or equal to the denominator, set
        // the carry and subtract the denominator to get it back in proper
        // remainder range.
        //

        Difference =
                (LONGLONG)(Denominator.Ulonglong -
                           RemainderParts.Ulonglong - 1) >>
                (ULONGLONG_BITS - 1);

        Carry = Difference & 0x1;
        RemainderParts.Ulonglong -= Denominator.Ulonglong & Difference;
        ShiftRight -= 1;
    }

    QuotientParts.Ulonglong = (QuotientParts.Ulonglong << 1) | Carry;
    if (Remainder != NULL) {
        *Remainder = RemainderParts.Ulonglong;
    }

    return QuotientParts.Ulonglong;
}

__USED
RTL_API
LONGLONG
RtlDivide64 (
    LONGLONG Dividend,
    LONGLONG Divisor
    )

/*++

Routine Description:

    This routine performs a 64-bit divide of two signed numbers.

Arguments:

    Dividend - Supplies the number that is going to be divided (the numerator).

    Divisor - Supplies the number to divide into (the denominator).

Return Value:

    Returns the quotient.

--*/

{

    LONGLONG DenominatorSign;
    LONGLONG NumeratorSign;
    LONGLONG Quotient;

    //
    // Negate the numerator and denominator if they're less than zero.
    //

    NumeratorSign = Dividend >> (LONGLONG_BITS - 1);
    DenominatorSign = Divisor >> (LONGLONG_BITS - 1);
    Dividend = (Dividend ^ NumeratorSign) - NumeratorSign;
    Divisor = (Divisor ^ DenominatorSign) - DenominatorSign;

    //
    // Get the sign of the quotient.
    //

    NumeratorSign ^= DenominatorSign;
    Quotient = (RtlDivideUnsigned64(Dividend, Divisor, NULL) ^ NumeratorSign) -
               NumeratorSign;

    return Quotient;
}

__USED
RTL_API
LONGLONG
RtlDivideModulo64 (
    LONGLONG Dividend,
    LONGLONG Divisor,
    PLONGLONG Remainder
    )

/*++

Routine Description:

    This routine performs a 64-bit divide and modulo of two signed numbers.

Arguments:

    Dividend - Supplies the number that is going to be divided (the numerator).

    Divisor - Supplies the number to divide into (the denominator).

    Remainder - Supplies a pointer where the remainder will be returned.

Return Value:

    Returns the quotient.

--*/

{

    LONGLONG Quotient;

    Quotient = RtlDivide64(Dividend, Divisor);
    *Remainder = Dividend - (Quotient * Divisor);
    return Quotient;
}

__USED
RTL_API
ULONG
RtlDivideUnsigned32 (
    ULONG Dividend,
    ULONG Divisor,
    PULONG Remainder
    )

/*++

Routine Description:

    This routine performs a 32-bit divide of two unsigned numbers.

Arguments:

    Dividend - Supplies the number that is going to be divided (the numerator).

    Divisor - Supplies the number to divide into (the denominator).

    Remainder - Supplies an optional pointer where the remainder will be
        returned.

Return Value:

    Returns the quotient.

--*/

{

    ULONG Carry;
    LONG Difference;
    ULONG Quotient;
    ULONG RemainderValue;
    ULONG ShiftRight;

    //
    // Handle divide by zero.
    //

    if (Divisor == 0) {

        ASSERT(Divisor != 0);

        if (Remainder != NULL) {
            *Remainder = 0;
        }

        return 0;
    }

    if (Dividend == 0) {
        if (Remainder != NULL) {
            *Remainder = 0;
        }

        return 0;
    }

    ShiftRight = __builtin_clz(Divisor) - __builtin_clz(Dividend);
    if (ShiftRight > (ULONG_BITS - 1)) {
        if (Remainder != NULL) {
            *Remainder = Dividend;
        }

        return 0;
    }

    if (ShiftRight == (ULONG_BITS - 1)) {
        if (Remainder != NULL) {
            *Remainder = 0;
        }

        return Dividend;
    }

    ShiftRight += 1;
    Quotient = Dividend << (ULONG_BITS - ShiftRight);
    RemainderValue = Dividend >> ShiftRight;
    Carry = 0;
    while (ShiftRight > 0) {
        RemainderValue = (RemainderValue << 1) | (Quotient >> (ULONG_BITS - 1));
        Quotient = (Quotient << 1) | Carry;

        //
        // If the remainder is greater than the divisor, set the carry and
        // subtract a divisor from the remainder to get it back in range.
        //

        Difference = (LONG)(Divisor - RemainderValue - 1) >> (ULONG_BITS - 1);
        Carry = Difference & 0x1;
        RemainderValue -= Divisor & Difference;
        ShiftRight -= 1;
    }

    Quotient = (Quotient << 1) | Carry;
    if (Remainder != NULL) {
        *Remainder = RemainderValue;
    }

    return Quotient;
}

__USED
RTL_API
LONG
RtlDivide32 (
    LONG Dividend,
    LONG Divisor
    )

/*++

Routine Description:

    This routine performs a 32-bit divide of two signed numbers.

Arguments:

    Dividend - Supplies the number that is going to be divided (the numerator).

    Divisor - Supplies the number to divide into (the denominator).

Return Value:

    Returns the quotient.

--*/

{

    LONG DenominatorSign;
    LONG NumeratorSign;

    //
    // Negate the numerator and denominator if they are negative.
    //

    NumeratorSign = Dividend >> (LONG_BITS - 1);
    DenominatorSign = Divisor >> (LONG_BITS - 1);
    Dividend = (Dividend ^ NumeratorSign) - NumeratorSign;
    Divisor = (Divisor ^ DenominatorSign) - DenominatorSign;

    //
    // Compute the sign of the quotient.
    //

    NumeratorSign ^= DenominatorSign;

    //
    // Perform an unsigned divide. The hope is that the architecture has an
    // unsigned divide instruction. If not, then the soft divide unsigned
    // gets called.
    //

    return (((ULONG)Dividend / (ULONG)Divisor) ^ NumeratorSign) - NumeratorSign;
}

__USED
RTL_API
LONG
RtlDivideModulo32 (
    LONG Dividend,
    LONG Divisor,
    PLONG Remainder
    )

/*++

Routine Description:

    This routine performs a 32-bit divide and modulo of two signed numbers.

Arguments:

    Dividend - Supplies the number that is going to be divided (the numerator).

    Divisor - Supplies the number to divide into (the denominator).

    Remainder - Supplies a pointer where the remainder will be returned.

Return Value:

    Returns the quotient.

--*/

{

    LONG Quotient;

    Quotient = RtlDivide32(Dividend, Divisor);
    *Remainder = Dividend - (Quotient * Divisor);
    return Quotient;
}

RTL_API
ULONGLONG
RtlByteSwapUlonglong (
    ULONGLONG Input
    )

/*++

Routine Description:

    This routine performs a byte-swap of a 64-bit integer, effectively changing
    its endianness.

Arguments:

    Input - Supplies the integer to byte swap.

Return Value:

    Returns the byte-swapped integer.

--*/

{

    ULONGLONG Result;

    Result = (Input >> 32) | (Input << 32);
    Result = ((Result & 0xFF00FF00FF00FF00ULL) >> 8) |
             ((Result & 0x00FF00FF00FF00FFULL) << 8);

    Result = ((Result & 0xFFFF0000FFFF0000ULL) >> 16) |
             ((Result & 0x0000FFFF0000FFFFULL) << 16);

    return Result;
}

RTL_API
ULONG
RtlByteSwapUlong (
    ULONG Input
    )

/*++

Routine Description:

    This routine performs a byte-swap of a 32-bit integer, effectively changing
    its endianness.

Arguments:

    Input - Supplies the integer to byte swap.

Return Value:

    Returns the byte-swapped integer.

--*/

{

    ULONG Result;

    Result = (Input >> 16) | (Input << 16);
    Result = ((Result & 0xFF00FF00UL) >> 8) |
             ((Result & 0x00FF00FFUL) << 8);

    return Result;
}

RTL_API
USHORT
RtlByteSwapUshort (
    USHORT Input
    )

/*++

Routine Description:

    This routine performs a byte-swap of a 16-bit integer, effectively changing
    its endianness.

Arguments:

    Input - Supplies the integer to byte swap.

Return Value:

    Returns the byte-swapped integer.

--*/

{

    ULONG Result;

    Result = ((Input & 0x00FF) << 8) | (Input >> 8);
    return Result;
}

RTL_API
INT
RtlCountTrailingZeros64 (
    ULONGLONG Value
    )

/*++

Routine Description:

    This routine determines the number of trailing zero bits in the given
    64-bit value.

Arguments:

    Value - Supplies the value to get the number of trailing zeros for. This
        must not be zero.

Return Value:

    Returns the number of trailing zero bits in the given value.

--*/

{

    LONG Extra;
    ULONGLONG_PARTS Parts;
    LONG Portion;

    Extra = 0;
    Parts.Ulonglong = Value;
    if (Parts.Ulong.Low != 0) {
        Portion = Parts.Ulong.Low;

    } else {
        Portion = Parts.Ulong.High;
        Extra = sizeof(ULONG) * BITS_PER_BYTE;
    }

    return RtlCountTrailingZeros32(Portion) + Extra;
}

RTL_API
INT
RtlCountTrailingZeros32 (
    ULONG Value
    )

/*++

Routine Description:

    This routine determines the number of trailing zero bits in the given
    32-bit value.

Arguments:

    Value - Supplies the value to get the number of trailing zeros for. This
        must not be zero.

Return Value:

    Returns the number of trailing zero bits in the given value.

--*/

{

    return __builtin_ctzl(Value);
}

RTL_API
INT
RtlCountLeadingZeros64 (
    ULONGLONG Value
    )

/*++

Routine Description:

    This routine determines the number of leading zero bits in the given 64-bit
    value.

Arguments:

    Value - Supplies the value to get the number of leading zeros for. This
        must not be zero.

Return Value:

    Returns the number of leading zero bits in the given value.

--*/

{

    return __builtin_clzll(Value);
}

RTL_API
INT
RtlCountLeadingZeros32 (
    ULONG Value
    )

/*++

Routine Description:

    This routine determines the number of leading zero bits in the given 32-bit
    value.

Arguments:

    Value - Supplies the value to get the number of leading zeros for. This
        must not be zero.

Return Value:

    Returns the number of leading zero bits in the given value.

--*/

{

    return __builtin_clzl(Value);
}

RTL_API
INT
RtlCountSetBits64 (
    ULONGLONG Value
    )

/*++

Routine Description:

    This routine determines the number of bits set to one in the given 64-bit
    value.

Arguments:

    Value - Supplies the value to count set bits for.

Return Value:

    Returns the number of bits set to one.

--*/

{

    INT Count;
    ULONGLONG_PARTS Parts;

    Count = 0;
    Parts.Ulonglong = Value;
    if (Parts.Ulong.High != 0) {
        Count = RtlCountSetBits32(Parts.Ulong.High);
    }

    Count += RtlCountSetBits32(Parts.Ulong.Low);
    return Count;
}

RTL_API
INT
RtlCountSetBits32 (
    ULONG Value
    )

/*++

Routine Description:

    This routine determines the number of bits set to one in the given 32-bit
    value.

Arguments:

    Value - Supplies the value to count set bits for.

Return Value:

    Returns the number of bits set to one.

--*/

{

    INT Count;

    Count = 0;
    while (Value != 0) {
        Count += RtlSetBitCounts[Value & 0x0F];
        Value = Value >> 4;
    }

    return Count;
}

//
// --------------------------------------------------------- Internal Functions
//

