/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

__USED
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

__USED
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

__USED
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

    if (Denominator == 0) {
        return __aeabi_idiv0(Numerator);
    }

    return RtlDivide32(Numerator, Denominator);
}

__USED
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

    if (Denominator == 0) {
        return __aeabi_idiv0(Numerator);
    }

    return RtlDivideUnsigned32(Numerator, Denominator, NULL);
}

//
// Floating point intrinsic routines. These are used in both soft and hard
// float implementations. Note that even in hard-float implementations, double
// values are returned using integer registers.
//

__USED
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

__USED
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

__USED
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

__USED
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

__USED
RTL_API
int
__aeabi_d2iz (
    unsigned long long Value
    )

/*++

Routine Description:

    This routine converts a double to a signed 32 bit integer.

Arguments:

    Value - Supplies the double value to convert, passed via integer registers
        always.

Return Value:

    Returns the integer equivalent of the given double.

--*/

{

    DOUBLE_PARTS Parts;

    Parts.Ulonglong = Value;
    return RtlDoubleConvertToInteger32RoundToZero(Parts.Double);
}

__USED
RTL_API
long long
__aeabi_d2lz (
    unsigned long long Value
    )

/*++

Routine Description:

    This routine converts a double to a signed 64 bit integer.

Arguments:

    Value - Supplies the double value to convert, passed via integer registers
        always.

Return Value:

    Returns the integer equivalent of the given double.

--*/

{

    DOUBLE_PARTS Parts;

    Parts.Ulonglong = Value;
    return RtlDoubleConvertToInteger64RoundToZero(Parts.Double);
}

__USED
RTL_API
unsigned int
__aeabi_d2uiz (
    unsigned long long Value
    )

/*++

Routine Description:

    This routine converts a double to an unsigned 32 bit integer.

Arguments:

    Value - Supplies the double value to convert, passed via integer registers
        always.

Return Value:

    Returns the integer equivalent of the given double.

--*/

{

    DOUBLE_PARTS Parts;

    Parts.Ulonglong = Value;
    return RtlDoubleConvertToInteger32RoundToZero(Parts.Double);
}

__USED
RTL_API
unsigned long long
__aeabi_d2ulz (
    unsigned long long Value
    )

/*++

Routine Description:

    This routine converts a double to an unsigned 64 bit integer.

Arguments:

    Value - Supplies the double value to convert, passed via integer registers
        always.

Return Value:

    Returns the integer equivalent of the given double.

--*/

{

    DOUBLE_PARTS Parts;

    Parts.Ulonglong = Value;
    return RtlDoubleConvertToInteger64RoundToZero(Parts.Double);
}

__USED
RTL_API
unsigned long
__aeabi_i2f (
    int Value
    )

/*++

Routine Description:

    This routine converts a signed 32 bit integer into a 32 bit floating point
    value (float).

Arguments:

    Value - Supplies the integer to convert.

Return Value:

    Returns the float equivalent of the given integer, in the integer
    registers.

--*/

{

    FLOAT_PARTS Result;

    Result.Float = RtlFloatConvertFromInteger32(Value);
    return Result.Ulong;
}

__USED
RTL_API
unsigned long
__aeabi_ui2f (
    unsigned int Value
    )

/*++

Routine Description:

    This routine converts an unsigned 32 bit integer into a 32 bit floating
    point value (float).

Arguments:

    Value - Supplies the integer to convert.

Return Value:

    Returns the float equivalent of the given integer, in the integer
    registers.

--*/

{

    FLOAT_PARTS Result;

    Result.Float = RtlFloatConvertFromUnsignedInteger32(Value);
    return Result.Ulong;
}

__USED
RTL_API
unsigned long
__aeabi_l2f (
    long long Value
    )

/*++

Routine Description:

    This routine converts a signed 64 bit integer into a 32 bit floating point
    value (float).

Arguments:

    Value - Supplies the integer to convert.

Return Value:

    Returns the float equivalent of the given integer, in the integer
    registers.

--*/

{

    FLOAT_PARTS Result;

    Result.Float = RtlFloatConvertFromInteger64(Value);
    return Result.Ulong;
}

__USED
RTL_API
unsigned long
__aeabi_ul2f (
    unsigned long long Value
    )

/*++

Routine Description:

    This routine converts an unsigned 64 bit integer into a 32 bit floating
    point value (float).

Arguments:

    Value - Supplies the integer to convert.

Return Value:

    Returns the float equivalent of the given integer, in the integer
    registers.

--*/

{

    FLOAT_PARTS Result;

    Result.Float = RtlFloatConvertFromUnsignedInteger64(Value);
    return Result.Ulong;
}

__USED
RTL_API
int
__aeabi_f2iz (
    unsigned long Value
    )

/*++

Routine Description:

    This routine converts a float to a signed 32 bit integer.

Arguments:

    Value - Supplies the float value to convert, passed via integer registers
        always.

Return Value:

    Returns the integer equivalent of the given float.

--*/

{

    FLOAT_PARTS Parts;

    Parts.Ulong = Value;
    return RtlFloatConvertToInteger32RoundToZero(Parts.Float);
}

__USED
RTL_API
long long
__aeabi_f2lz (
    unsigned long Value
    )

/*++

Routine Description:

    This routine converts a float to a signed 64 bit integer.

Arguments:

    Value - Supplies the float value to convert, passed via integer registers
        always.

Return Value:

    Returns the integer equivalent of the given float.

--*/

{

    FLOAT_PARTS Parts;

    Parts.Ulong = Value;
    return RtlFloatConvertToInteger64RoundToZero(Parts.Float);
}

__USED
RTL_API
unsigned int
__aeabi_f2uiz (
    unsigned long Value
    )

/*++

Routine Description:

    This routine converts a float to an unsigned 32 bit integer.

Arguments:

    Value - Supplies the float value to convert, passed via integer registers
        always.

Return Value:

    Returns the integer equivalent of the given float.

--*/

{

    FLOAT_PARTS Parts;

    Parts.Ulong = Value;
    return RtlFloatConvertToInteger32RoundToZero(Parts.Float);
}

__USED
RTL_API
unsigned long long
__aeabi_f2ulz (
    unsigned long Value
    )

/*++

Routine Description:

    This routine converts a float to an unsigned 64 bit integer.

Arguments:

    Value - Supplies the float value to convert, passed via integer registers
        always.

Return Value:

    Returns the integer equivalent of the given float.

--*/

{

    FLOAT_PARTS Parts;

    Parts.Ulong = Value;
    return RtlFloatConvertToInteger64RoundToZero(Parts.Float);
}

