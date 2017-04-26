/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    intrinsc.c

Abstract:

    This module implements x86 compiler intrinsics.

Author:

    Evan Green 23-Sep-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "../../rtlp.h"

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
// ------------------------------------------------------------------ Functions
//

//
// --------------------------------------------------------- Internal Functions
//

__USED
RTL_API
LONGLONG
__divdi3 (
    LONGLONG Numerator,
    LONGLONG Denominator
    )

/*++

Routine Description:

    This routine performs a 64-bit divide of two signed numbers.

Arguments:

    Numerator - Supplies the numerator of the division.

    Denominator - Supplies the denominator of the division.

Return Value:

    Returns the result of the division, or 0 if there was an error (such as
    divide by 0).

--*/

{

    return RtlDivide64(Numerator, Denominator);
}

__USED
RTL_API
ULONGLONG
__udivdi3 (
    ULONGLONG Numerator,
    ULONGLONG Denominator
    )

/*++

Routine Description:

    This routine performs a 64-bit divide of two unsigned numbers.

Arguments:

    Numerator - Supplies the numerator of the division.

    Denominator - Supplies the denominator of the division.

Return Value:

    Returns the result of the division, or 0 if there was an error (such as
    divide by 0).

--*/

{

    return RtlDivideUnsigned64(Numerator, Denominator, NULL);
}

__USED
RTL_API
LONGLONG
__moddi3 (
    LONGLONG Numerator,
    LONGLONG Denominator
    )

/*++

Routine Description:

    This routine performs a 64-bit divide of two signed numbers and returns
    the remainder.

Arguments:

    Numerator - Supplies the numerator of the division.

    Denominator - Supplies the denominator of the division.

Return Value:

    Returns the remainder from the division, or 0 if there was an error
    (like a divide by 0).

--*/

{

    LONGLONG Remainder;

    RtlDivideModulo64(Numerator, Denominator, &Remainder);
    return Remainder;
}

__USED
RTL_API
ULONGLONG
__umoddi3 (
    ULONGLONG Numerator,
    ULONGLONG Denominator
    )

/*++

Routine Description:

    This routine performs a 64-bit divide of two unsigned numbers and returns
    the remainder.

Arguments:

    Numerator - Supplies the numerator of the division.

    Denominator - Supplies the denominator of the division.

Return Value:

    Returns the remainder from the division, or 0 if there was an error
    (like a divide by 0).

--*/

{

    ULONGLONG Remainder;

    RtlDivideUnsigned64(Numerator, Denominator, &Remainder);
    return Remainder;
}

__USED
RTL_API
INT
__ctzdi2 (
    ULONGLONG Value
    )

/*++

Routine Description:

    This routine returns the number of trailing zero bits in the given value,
    starting at the least significant bit position. The value must not be zero.

Arguments:

    Value - Supplies the value to check.

Return Value:

    Returns the number of trailing zero bits.

--*/

{

    return RtlCountTrailingZeros64(Value);
}

__USED
RTL_API
INT
__ffsdi2 (
    LONGLONG Value
    )

/*++

Routine Description:

    This routine finds the index of the least significant bit that is set to 1.

Arguments:

    Value - Supplies the value to get the first set bit of.

Return Value:

    Returns the first bit index set to 1.

    0 if the value is zero.

--*/

{

    if (Value == 0) {
        return 0;
    }

    return RtlCountTrailingZeros64(Value) + 1;
}

__USED
RTL_API
INT
__popcountdi2 (
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

    return RtlCountSetBits64(Value);
}

__USED
RTL_API
INT
__popcountsi2 (
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

    return RtlCountSetBits32(Value);
}

//
// Long double stubs.
//

__USED
RTL_API
long double
__multf3 (
    long double Value1,
    long double Value2
    )

/*++

Routine Description:

    This routine returns the product of two long doubles.

Arguments:

    Value1 - Supplies the first value to multiply.

    Value2 - Supplies the second value to multiply.

Return Value:

    Returns the product of the two values.

--*/

{

    ASSERT(FALSE);

    return 0;
}

__USED
RTL_API
long double
__divtf3 (
    long double Dividend,
    long double Divisor
    )

/*++

Routine Description:

    This routine returns the division of two long doubles.

Arguments:

    Dividend - Supplies the dividend.

    Divisor - Supplies the divisor.

Return Value:

    Returns the quotient.

--*/

{

    ASSERT(FALSE);

    return 0;
}

__USED
RTL_API
long double
__subtf3 (
    long double Value1,
    long double Value2
    )

/*++

Routine Description:

    This routine returns the difference between two long doubles.

Arguments:

    Value1 - Supplies the left value.

    Value2 - Supplies the right value.

Return Value:

    Returns the left value minus the right value.

--*/

{

    ASSERT(FALSE);

    return 0;
}

__USED
RTL_API
long double
__addtf3 (
    long double Value1,
    long double Value2
    )

/*++

Routine Description:

    This routine returns the sum of two long double.

Arguments:

    Value1 - Supplies the first value to add.

    Value2 - Supplies the second value to add.

Return Value:

    Returns the sum of the two values.

--*/

{

    ASSERT(FALSE);

    return 0;
}

__USED
RTL_API
int
__netf2 (
    long double Value1,
    long double Value2
    )

/*++

Routine Description:

    This routine returns 0 if the two values are not equal.

Arguments:

    Value1 - Supplies the first value to compare.

    Value2 - Supplies the second value to compare.

Return Value:

    Returns 0 if the two values are equal, non-zero otherwise.

--*/

{

    ASSERT(FALSE);

    return 0;
}

__USED
RTL_API
int
__eqtf2 (
    long double Value1,
    long double Value2
    )

/*++

Routine Description:

    This routine returns 0 if the two values are equal.

Arguments:

    Value1 - Supplies the first value to compare.

    Value2 - Supplies the second value to compare.

Return Value:

    Returns 0 if the two values are equal, non-zero otherwise.

--*/

{

    ASSERT(FALSE);

    return 0;
}

__USED
RTL_API
int
__lttf2 (
    long double Value1,
    long double Value2
    )

/*++

Routine Description:

    This routine returns 0 if Value1 < Value2.

Arguments:

    Value1 - Supplies the first value to compare.

    Value2 - Supplies the second value to compare.

Return Value:

    Returns 0 if Value1 < Value2, or non-zero otherwise.

--*/

{

    ASSERT(FALSE);

    return 0;
}

__USED
RTL_API
long double
__copysigntf3 (
    long double Value,
    long double ValueWithSign
    )

/*++

Routine Description:

    This routine sets the sign of the first value to that of the second value.

Arguments:

    Value - Supplies the value to modify.

    ValueWithSign - Supplies the value with the desired sign.

Return Value:

    Returns the first value with the sign of the second value.

--*/

{

    ASSERT(FALSE);

    return Value;
}

__USED
RTL_API
long double
__fabstf2 (
    long double Value
    )

/*++

Routine Description:

    This routine returns the absolute value of the given value.

Arguments:

    Value - Supplies the value.

Return Value:

    Returns the absolute value of the given value.

--*/

{

    ASSERT(FALSE);

    return Value;
}

__USED
RTL_API
unsigned int
__fixunstfsi (
    long double Value
    )

/*++

Routine Description:

    This routine converts a long double to an integer, rounding towards zero.
    Negative values all become zero.

Arguments:

    Value - Supplies the value to convert.

Return Value:

    Returns the integer representation of the value.

--*/

{

    ASSERT(FALSE);

    return 0;
}

__USED
RTL_API
long double
__floatsitf (
    int Value
    )

/*++

Routine Description:

    This routine converts a value into its long double equivalent.

Arguments:

    Value - Supplies the value to convert.

Return Value:

    Returns the long double representation of the value.

--*/

{

    ASSERT(FALSE);

    return 0.0;
}

__USED
RTL_API
long double
__floatunsitf (
    unsigned int Value
    )

/*++

Routine Description:

    This routine converts a value into its long double equivalent.

Arguments:

    Value - Supplies the value to convert.

Return Value:

    Returns the long double representation of the value.

--*/

{

    ASSERT(FALSE);

    return 0.0;
}

