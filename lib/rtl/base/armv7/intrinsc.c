/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    intrinsc.c

Abstract:

    This module implements ARM compiler intrinsics.

Author:

    Evan Green 13-Aug-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#include "../../rtlp.h"

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

RTL_API
int
__aeabi_idiv0 (
    int ReturnValue
    )

/*++

Routine Description:

    This routine is called when an integer divide by zero occurs.

Arguments:

    ReturnValue - Supplies the value that is returned.

Return Value:

    Returns the given parameter.

--*/

{

    RtlDebugPrint("Divide by zero!\n");

    ASSERT(FALSE);

    return ReturnValue;
}

RTL_API
long long
__aeabi_ldiv0 (
    long long ReturnValue
    )

/*++

Routine Description:

    This routine is called when a long integer divide by zero occurs.

Arguments:

    ReturnValue - Supplies the value that is returned.

Return Value:

    Returns the given parameter.

--*/

{

    RtlDebugPrint("Divide by zero!\n");

    ASSERT(FALSE);

    return ReturnValue;
}

RTL_API
int
__aeabi_idiv (
    int Numerator,
    int Denominator
    )

/*++

Routine Description:

    This routine performs a 32-bit divide of two signed integers.

Arguments:

    Numerator - Supplies the numerator of the division.

    Denominator - Supplies the denominator of the division.

Return Value:

    Returns the result of the division, or 0 if there was an error (such as
    divide by 0).

--*/

{

    BOOL Correct;
    LONGLONG Quotient;

    ASSERT(Denominator != 0);

    if (Denominator == 0) {
        return __aeabi_idiv0(Numerator);
    }

    Correct = RtlDivide64(Numerator, Denominator, &Quotient, NULL);
    if (Correct == FALSE) {
        return __aeabi_idiv0(Numerator);
    }

    return (int)Quotient;
}

RTL_API
unsigned int
__aeabi_uidiv (
    unsigned int Numerator,
    unsigned int Denominator
    )

/*++

Routine Description:

    This routine performs a 32-bit divide of two unsigned integers.

Arguments:

    Numerator - Supplies the numerator of the division.

    Denominator - Supplies the denominator of the division.

Return Value:

    Returns the result of the division, or 0 if there was an error (such as
    divide by 0).

--*/

{

    BOOL Correct;
    ULONGLONG Quotient;

    ASSERT(Denominator != 0);

    if (Denominator == 0) {
        return __aeabi_idiv0(Numerator);
    }

    Correct = RtlDivideUnsigned64(Numerator, Denominator, &Quotient, NULL);
    if (Correct == FALSE) {
        return __aeabi_idiv0(Numerator);
    }

    return (unsigned int)Quotient;
}

//
// Floating point intrinsic routines. These are used in both soft and hard
// float implementations. Note that even in hard-float implementations, double
// values are returned using integer registers.
//

RTL_API
unsigned long long
__aeabi_i2d (
    int Value
    )

/*++

Routine Description:

    This routine converts a signed 32 bit integer into a 64 bit floating point
    value (double).

Arguments:

    Value - Supplies the integer to convert.

Return Value:

    Returns the double equivalent of the given integer, in the integer
    registers.

--*/

{

    DOUBLE_PARTS Result;

    Result.Double = RtlDoubleConvertFromInteger32(Value);
    return Result.Ulonglong;
}

RTL_API
unsigned long long
__aeabi_ui2d (
    unsigned int Value
    )

/*++

Routine Description:

    This routine converts an unsigned 32 bit integer into a 64 bit floating
    point value (double).

Arguments:

    Value - Supplies the integer to convert.

Return Value:

    Returns the double equivalent of the given integer, in the integer
    registers.

--*/

{

    DOUBLE_PARTS Result;

    Result.Double = RtlDoubleConvertFromUnsignedInteger32(Value);
    return Result.Ulonglong;
}

RTL_API
unsigned long long
__aeabi_l2d (
    long long Value
    )

/*++

Routine Description:

    This routine converts a signed 64 bit integer into a 64 bit floating point
    value (double).

Arguments:

    Value - Supplies the integer to convert.

Return Value:

    Returns the double equivalent of the given integer, in the integer
    registers.

--*/

{

    DOUBLE_PARTS Result;

    Result.Double = RtlDoubleConvertFromInteger64(Value);
    return Result.Ulonglong;
}

RTL_API
unsigned long long
__aeabi_ul2d (
    unsigned long long Value
    )

/*++

Routine Description:

    This routine converts an unsigned 64 bit integer into a 64 bit floating
    point value (double).

Arguments:

    Value - Supplies the integer to convert.

Return Value:

    Returns the double equivalent of the given integer, in the integer
    registers.

--*/

{

    DOUBLE_PARTS Result;

    Result.Double = RtlDoubleConvertFromUnsignedInteger64(Value);
    return Result.Ulonglong;
}

