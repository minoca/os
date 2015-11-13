/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    aeabisfp.c

Abstract:

    This module implements ARM EABI compiler intrinsics for a soft floating
    point implementation.

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
double
__aeabi_dadd (
    double FirstValue,
    double SecondValue
    )

/*++

Routine Description:

    This routine adds two 64-bit floating point values.

Arguments:

    FirstValue - Supplies the first value.

    SecondValue - Supplies the second value.

Return Value:

    Returns the sum of the two values as a double.

--*/

{

    return RtlDoubleAdd(FirstValue, SecondValue);
}

RTL_API
double
__aeabi_dsub (
    double FirstValue,
    double SecondValue
    )

/*++

Routine Description:

    This routine subtracts two 64-bit floating point values.

Arguments:

    FirstValue - Supplies the value to subtract from.

    SecondValue - Supplies the value to subtract.

Return Value:

    Returns the first value minus the second value.

--*/

{

    return RtlDoubleSubtract(FirstValue, SecondValue);
}

RTL_API
double
__aeabi_dmul (
    double FirstValue,
    double SecondValue
    )

/*++

Routine Description:

    This routine multiplies two 64-bit floating point values.

Arguments:

    FirstValue - Supplies the first value.

    SecondValue - Supplies the second value.

Return Value:

    Returns the product of the two values as a double.

--*/

{

    return RtlDoubleMultiply(FirstValue, SecondValue);
}

RTL_API
double
__aeabi_ddiv (
    double Dividend,
    double Divisor
    )

/*++

Routine Description:

    This routine divides two 64-bit floating point values.

Arguments:

    Dividend - Supplies the number to divide.

    Divisor - Supplies the number to divide by.

Return Value:

    Returns the quotient of the two values as a double.

--*/

{

    return RtlDoubleDivide(Dividend, Divisor);
}

RTL_API
int
__aeabi_d2iz (
    double Value
    )

/*++

Routine Description:

    This routine converts a double to a signed 32 bit integer.

Arguments:

    Value - Supplies the integer to convert.

Return Value:

    Returns the integer equivalent of the given double.

--*/

{

    return RtlDoubleConvertToInteger32RoundToZero(Value);
}

RTL_API
long long
__aeabi_d2lz (
    double Value
    )

/*++

Routine Description:

    This routine converts a double to a signed 64 bit integer.

Arguments:

    Value - Supplies the integer to convert.

Return Value:

    Returns the integer equivalent of the given double.

--*/

{

    return RtlDoubleConvertToInteger64RoundToZero(Value);
}

RTL_API
unsigned int
__aeabi_d2uiz (
    double Value
    )

/*++

Routine Description:

    This routine converts a double to an unsigned 32 bit integer.

Arguments:

    Value - Supplies the integer to convert.

Return Value:

    Returns the integer equivalent of the given double.

--*/

{

    return RtlDoubleConvertToInteger32RoundToZero(Value);
}

RTL_API
unsigned long long
__aeabi_d2ulz (
    double Value
    )

/*++

Routine Description:

    This routine converts a double to an unsigned 64 bit integer.

Arguments:

    Value - Supplies the integer to convert.

Return Value:

    Returns the integer equivalent of the given double.

--*/

{

    return RtlDoubleConvertToInteger64RoundToZero(Value);
}

RTL_API
float
__aeabi_d2f (
    double Value
    )

/*++

Routine Description:

    This routine converts a double to a 32 bit single precision floating point
    value.

Arguments:

    Value - Supplies the double to convert.

Return Value:

    Returns the 32-bit floating point equivalent of the given double.

--*/

{

    return RtlDoubleConvertToFloat(Value);
}

RTL_API
double
__aeabi_f2d (
    float Value
    )

/*++

Routine Description:

    This routine converts a float (32-bit single precision floating point
    value) to a double (64-bit double precision floating point value).

Arguments:

    Value - Supplies the float to convert.

Return Value:

    Returns the 64-bit floating point equivalent of the given double.

--*/

{

    return RtlFloatConvertToDouble(Value);
}

RTL_API
int
__aeabi_dcmpeq (
    double Left,
    double Right
    )

/*++

Routine Description:

    This routine returns a comparison of the two given floating point values.

Arguments:

    Left - Supplies the left hand side of the comparison.

    Right - Supplies the right hand side of the comparison.

Return Value:

    1 if the two values are equal.

    0 if the two values are not equal.

--*/

{

    if (RtlDoubleIsEqual(Left, Right) != FALSE) {
        return 1;
    }

    return 0;
}

RTL_API
int
__aeabi_dcmplt (
    double Left,
    double Right
    )

/*++

Routine Description:

    This routine returns a comparison of the two given floating point values.

Arguments:

    Left - Supplies the left hand side of the comparison.

    Right - Supplies the right hand side of the comparison.

Return Value:

    1 if Left < Right.

    0 otherwise.

--*/

{

    if (RtlDoubleIsLessThan(Left, Right) != FALSE) {
        return 1;
    }

    return 0;
}

RTL_API
int
__aeabi_dcmple (
    double Left,
    double Right
    )

/*++

Routine Description:

    This routine returns a comparison of the two given floating point values.

Arguments:

    Left - Supplies the left hand side of the comparison.

    Right - Supplies the right hand side of the comparison.

Return Value:

    1 if Left <= Right.

    0 otherwise.

--*/

{

    if (RtlDoubleIsLessThanOrEqual(Left, Right) != FALSE) {
        return 1;
    }

    return 0;
}

RTL_API
int
__aeabi_dcmpgt (
    double Left,
    double Right
    )

/*++

Routine Description:

    This routine returns a comparison of the two given floating point values.

Arguments:

    Left - Supplies the left hand side of the comparison.

    Right - Supplies the right hand side of the comparison.

Return Value:

    1 if Left > Right.

    0 otherwise.

--*/

{

    if ((RtlDoubleIsNan(Left) != FALSE) || (RtlDoubleIsNan(Right) != FALSE)) {
        return 0;
    }

    if (RtlDoubleIsLessThanOrEqual(Left, Right) == 0) {
        return 1;
    }

    return 0;
}

RTL_API
int
__aeabi_dcmpge (
    double Left,
    double Right
    )

/*++

Routine Description:

    This routine returns a comparison of the two given floating point values.

Arguments:

    Left - Supplies the left hand side of the comparison.

    Right - Supplies the right hand side of the comparison.

Return Value:

    1 if Left >= Right.

    0 otherwise.

--*/

{

    if ((RtlDoubleIsNan(Left) != FALSE) || (RtlDoubleIsNan(Right) != FALSE)) {
        return 0;
    }

    if (RtlDoubleIsLessThan(Left, Right) == 0) {
        return 1;
    }

    return 0;
}

