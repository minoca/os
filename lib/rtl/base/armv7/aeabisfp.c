/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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
float
__aeabi_fadd (
    float FirstValue,
    float SecondValue
    )

/*++

Routine Description:

    This routine adds two 32-bit floating point values.

Arguments:

    FirstValue - Supplies the first value.

    SecondValue - Supplies the second value.

Return Value:

    Returns the sum of the two values as a float.

--*/

{

    return RtlFloatAdd(FirstValue, SecondValue);
}

RTL_API
float
__aeabi_fsub (
    float FirstValue,
    float SecondValue
    )

/*++

Routine Description:

    This routine subtracts two 32-bit floating point values.

Arguments:

    FirstValue - Supplies the value to subtract from.

    SecondValue - Supplies the value to subtract.

Return Value:

    Returns the first value minus the second value.

--*/

{

    return RtlFloatSubtract(FirstValue, SecondValue);
}

RTL_API
float
__aeabi_fmul (
    float FirstValue,
    float SecondValue
    )

/*++

Routine Description:

    This routine multiplies two 32-bit floating point values.

Arguments:

    FirstValue - Supplies the first value.

    SecondValue - Supplies the second value.

Return Value:

    Returns the product of the two values as a float.

--*/

{

    return RtlFloatMultiply(FirstValue, SecondValue);
}

RTL_API
float
__aeabi_fdiv (
    float Dividend,
    float Divisor
    )

/*++

Routine Description:

    This routine divides two 32-it floating point values.

Arguments:

    Dividend - Supplies the number to divide.

    Divisor - Supplies the number to divide by.

Return Value:

    Returns the quotient of the two values as a float.

--*/

{

    return RtlFloatDivide(Dividend, Divisor);
}

RTL_API
int
__aeabi_fcmpeq (
    float Left,
    float Right
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

    if (RtlFloatIsEqual(Left, Right) != FALSE) {
        return 1;
    }

    return 0;
}

RTL_API
int
__aeabi_fcmplt (
    float Left,
    float Right
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

    if (RtlFloatIsLessThan(Left, Right) != FALSE) {
        return 1;
    }

    return 0;
}

RTL_API
int
__aeabi_fcmple (
    float Left,
    float Right
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

    if (RtlFloatIsLessThanOrEqual(Left, Right) != FALSE) {
        return 1;
    }

    return 0;
}

RTL_API
int
__aeabi_fcmpgt (
    float Left,
    float Right
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

    if ((RtlFloatIsNan(Left) != FALSE) || (RtlFloatIsNan(Right) != FALSE)) {
        return 0;
    }

    if (RtlFloatIsLessThanOrEqual(Left, Right) == 0) {
        return 1;
    }

    return 0;
}

RTL_API
int
__aeabi_fcmpge (
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

    if ((RtlFloatIsNan(Left) != FALSE) || (RtlFloatIsNan(Right) != FALSE)) {
        return 0;
    }

    if (RtlFloatIsLessThan(Left, Right) == 0) {
        return 1;
    }

    return 0;
}

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

