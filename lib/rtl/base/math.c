/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

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

RTL_API
BOOL
RtlDivideUnsigned64 (
    ULONGLONG Dividend,
    ULONGLONG Divisor,
    PULONGLONG QuotientOut,
    PULONGLONG RemainderOut
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

    ULONG BitCount;
    ULONG BitIndex;
    ULONGLONG LastDividend;
    ULONG MostSignificantBit;
    ULONGLONG Quotient;
    ULONGLONG Remainder;
    INT Shift;
    ULONGLONG Subtraction;
    ULONG SubtractionTopBitNot;

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

    if (POWER_OF_2(Divisor) != FALSE) {
        Remainder = REMAINDER(Dividend, Divisor);
        Shift = RtlCountTrailingZeros64(Divisor);
        Quotient = Dividend >> Shift;
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

RTL_API
BOOL
RtlDivide64 (
    LONGLONG Dividend,
    LONGLONG Divisor,
    PLONGLONG QuotientOut,
    PLONGLONG RemainderOut
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

    ULONGLONG AbsoluteDividend;
    ULONGLONG AbsoluteDivisor;
    BOOL Result;
    ULONGLONG UnsignedQuotient;
    ULONGLONG UnsignedRemainder;

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

    Result = RtlDivideUnsigned64(AbsoluteDividend,
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

