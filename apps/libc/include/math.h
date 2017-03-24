/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    math.h

Abstract:

    This header contains mathematical declaraions and definitions.

Author:

    Evan Green 22-Jul-2013

--*/

#ifndef _MATH_H
#define _MATH_H

//
// ------------------------------------------------------------------- Includes
//

#include <libcbase.h>
#include <stddef.h>

//
// --------------------------------------------------------------------- Macros
//

#ifdef __cplusplus

extern "C" {

#endif

//
// This macro classifies the given real-floating value into one of five
// categories: NaN, infinite, normal, subnormal, and zero.
//

#define fpclassify(_Value)             \
    __builtin_fpclassify(FP_NAN,       \
                         FP_INFINITE,  \
                         FP_NORMAL,    \
                         FP_SUBNORMAL, \
                         FP_ZERO,      \
                         _Value)

//
// This macro determines whether or not the given real-floating value is finite.
//

#define isfinite(_Value) __builtin_isfinite(_Value)

//
// This macro determines whether or not Value1 is greater than Value2.
//

#define isgreater(_Value1, _Value2) __builtin_isgreater(_Value1, _Value2)

//
// This macro determines whether or not Value1 is greater than or equal to
// Value2.
//

#define isgreaterequal(_Value1, _Value2) \
    __builtin_isgreaterequal(_Value1, _Value2)

//
// This macro determines whether or not the given real-floating value is
// positive or negative infinity.
//

#define isinf(_Value) __builtin_isinf(_Value)

//
// This macro determines whether or not Value1 is less than Value2.
//

#define isless(_Value1, _Value2) __builtin_isless(_Value1, _Value2)

//
// This macro determines whether or not Value1 is less than or equal to Value2.
//

#define islessequal(_Value1, _Value2) __builtin_islessequal(_Value1, _Value2)

//
// This macro determines whether or not Value1 is less than or greater than
// Value2.
//

#define islessgreater(_Value1, _Value2) \
    __builtin_islessgreater(_Value1, _Value2)

//
// This macro determines whether or not the given real-floating value is NaN.
//

#define isnan(_Value) __builtin_isnan(_Value)

//
// This macro determines whether or not the given real-floating value is normal.
// That is, it is not NaN, zero, or infinite and it is not too small to be
// represented in normalized format.
//

#define isnormal(_Value) __builtin_isnormal(_Value)

//
// This macro determines whether or not at least one value is NaN and thus they
// cannot be compared with each other.
//

#define isunordered(_Value1, _Value2) __builtin_isunordered(_Value1, _Value2)

//
// This macro determines whether or not the given real-floating value is
// negative.
//

#define signbit(_Value) __builtin_signbit(_Value)

//
// ---------------------------------------------------------------- Definitions
//

//
// Define positive infinity as a double.
//

#define HUGE_VAL __builtin_huge_val()

//
// Define positive infinity as a float.
//

#define HUGE_VALF __builtin_huge_valf()

//
// Define positive infinity as a long double.
//

#define HUGE_VALL __builtin_huge_vall()

//
// Define infinity as a positive or unsigned float.
//

#define INFINITY __builtin_inf()

//
// Define a constant expression of type float representing a quiet
// "not a number".
//

#define NAN __builtin_nan("")

//
// Define some traditional constants.
//

#define M_E        2.71828182845904523536028747135266250
#define M_LOG2E    1.44269504088896340735992468100189214
#define M_LOG10E   0.434294481903251827651128918916605082
#define M_LN2      0.693147180559945309417232121458176568
#define M_LN10     2.30258509299404568401799145468436421
#define M_PI       3.14159265358979323846264338327950288
#define M_PI_2     1.57079632679489661923132169163975144
#define M_PI_4     0.785398163397448309615660845819875721
#define M_1_PI     0.318309886183790671537767526745028724
#define M_2_PI     0.636619772367581343075535053490057448
#define M_2_SQRTPI 1.12837916709551257389615890312154517
#define M_SQRT2    1.41421356237309504880168872420969808
#define M_SQRT1_2  0.707106781186547524400844362104849039

//
// Define the floating point number categories.
//

#define FP_NAN 0
#define FP_INFINITE 1
#define FP_NORMAL 2
#define FP_SUBNORMAL 3
#define FP_ZERO 4

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
double
sin (
    double Value
    );

/*++

Routine Description:

    This routine returns the sine of the given value.

Arguments:

    Value - Supplies the value to compute the sine of, in radians.

Return Value:

    Returns the sine of the value.

--*/

LIBC_API
float
sinf (
    float Value
    );

/*++

Routine Description:

    This routine returns the sine of the given value.

Arguments:

    Value - Supplies the value to compute the sine of, in radians.

Return Value:

    Returns the sine of the value.

--*/

LIBC_API
double
cos (
    double Value
    );

/*++

Routine Description:

    This routine returns the cosine of the given value.

Arguments:

    Value - Supplies the value to compute the cosine of, in radians.

Return Value:

    Returns the cosine of the value.

--*/

LIBC_API
float
cosf (
    float Value
    );

/*++

Routine Description:

    This routine returns the cosine of the given value.

Arguments:

    Value - Supplies the value to compute the cosine of, in radians.

Return Value:

    Returns the cosine of the value.

--*/

LIBC_API
double
tan (
    double Value
    );

/*++

Routine Description:

    This routine returns the tangent of the given value.

Arguments:

    Value - Supplies the value to compute the tangent of, in radians.

Return Value:

    Returns the cosine of the value.

--*/

LIBC_API
float
tanf (
    float Value
    );

/*++

Routine Description:

    This routine returns the tangent of the given value.

Arguments:

    Value - Supplies the value to compute the tangent of, in radians.

Return Value:

    Returns the cosine of the value.

--*/

LIBC_API
double
asin (
    double Value
    );

/*++

Routine Description:

    This routine computes the arc sine of the given value.

Arguments:

    Value - Supplies the sine value to convert back to an angle.

Return Value:

    Returns the arc sine of the value, in radians.

--*/

LIBC_API
float
asinf (
    float Value
    );

/*++

Routine Description:

    This routine computes the arc sine of the given value.

Arguments:

    Value - Supplies the sine value to convert back to an angle.

Return Value:

    Returns the arc sine of the value, in radians.

--*/

LIBC_API
double
acos (
    double Value
    );

/*++

Routine Description:

    This routine computes the arc cosine of the given value.

Arguments:

    Value - Supplies the cosine value to convert back to an angle.

Return Value:

    Returns the arc cosine of the value, in radians.

--*/

LIBC_API
float
acosf (
    float Value
    );

/*++

Routine Description:

    This routine computes the arc cosine of the given value.

Arguments:

    Value - Supplies the cosine value to convert back to an angle.

Return Value:

    Returns the arc cosine of the value, in radians.

--*/

LIBC_API
double
atan (
    double Value
    );

/*++

Routine Description:

    This routine computes the arc tangent of the given value.

Arguments:

    Value - Supplies the tangent value to convert back to an angle.

Return Value:

    Returns the arc tangent of the value, in radians.

--*/

LIBC_API
float
atanf (
    float Value
    );

/*++

Routine Description:

    This routine computes the arc tangent of the given value.

Arguments:

    Value - Supplies the tangent value to convert back to an angle.

Return Value:

    Returns the arc tangent of the value, in radians.

--*/

LIBC_API
double
atan2 (
    double Numerator,
    double Denominator
    );

/*++

Routine Description:

    This routine computes the arc tangent of the given values, using the signs
    of both the numerator and the denominator to determine the correct
    quadrant for the output angle.

Arguments:

    Numerator - Supplies the numerator to the tangent value.

    Denominator - Supplies the denominator to the tangent value.

Return Value:

    Returns the arc tangent of the value, in radians.

    Pi if the numerator is +/- 0 and the denominator is negative.

    +/- 0 if the numerator is +/- 0 and the denominator is positive.

    Negative pi over 2 if the numerator is negative and the denominator is
    +/- 0.

    Pi over 2 if the numerator is positive and the denominator is +/- 0.

    NaN if either input is NaN.

    Returns the numerator over the denominator if the result underflows.

    +/- Pi if the numerator is +/- 0 and the denominator is -0.

    +/- 0 if the numerator is +/- 0 and the denominator is +0.

    +/- Pi for positive finite values of the numerator and -Infinity in the
    denominator.

    +/- 0 for positive finite values of the numerator and +Infinity in the
    denominator.

    +/- Pi/2 for finite values of the denominator if the numerator is
    +/- Infinity.

    +/- 3Pi/4 if the numerator is +/- Infinity and the denominator is -Infinity.

    +/- Pi/4 if the numerator is +/- Infinity and the denominator is +Infinity.

--*/

LIBC_API
float
atan2f (
    float Numerator,
    float Denominator
    );

/*++

Routine Description:

    This routine computes the arc tangent of the given values, using the signs
    of both the numerator and the denominator to determine the correct
    quadrant for the output angle.

Arguments:

    Numerator - Supplies the numerator to the tangent value.

    Denominator - Supplies the denominator to the tangent value.

Return Value:

    Returns the arc tangent of the value, in radians.

    Pi if the numerator is +/- 0 and the denominator is negative.

    +/- 0 if the numerator is +/- 0 and the denominator is positive.

    Negative pi over 2 if the numerator is negative and the denominator is
    +/- 0.

    Pi over 2 if the numerator is positive and the denominator is +/- 0.

    NaN if either input is NaN.

    Returns the numerator over the denominator if the result underflows.

    +/- Pi if the numerator is +/- 0 and the denominator is -0.

    +/- 0 if the numerator is +/- 0 and the denominator is +0.

    +/- Pi for positive finite values of the numerator and -Infinity in the
    denominator.

    +/- 0 for positive finite values of the numerator and +Infinity in the
    denominator.

    +/- Pi/2 for finite values of the denominator if the numerator is
    +/- Infinity.

    +/- 3Pi/4 if the numerator is +/- Infinity and the denominator is -Infinity.

    +/- Pi/4 if the numerator is +/- Infinity and the denominator is +Infinity.

--*/

LIBC_API
double
sinh (
    double Value
    );

/*++

Routine Description:

    This routine computes the hyperbolic sine of the given value.

Arguments:

    Value - Supplies the value to take the hyperbolic sine of.

Return Value:

    Returns the hyperbolic sine on success.

    +/- HUGE_VAL (with the same sign as the value) if the result cannot be
    represented.

    NaN if the input is NaN.

    Returns the value itself if the given value is +/- 0 or +/- Infinity.

--*/

LIBC_API
float
sinhf (
    float Value
    );

/*++

Routine Description:

    This routine computes the hyperbolic sine of the given value.

Arguments:

    Value - Supplies the value to take the hyperbolic sine of.

Return Value:

    Returns the hyperbolic sine on success.

    +/- HUGE_VAL (with the same sign as the value) if the result cannot be
    represented.

    NaN if the input is NaN.

    Returns the value itself if the given value is +/- 0 or +/- Infinity.

--*/

LIBC_API
double
cosh (
    double Value
    );

/*++

Routine Description:

    This routine computes the hyperbolic cosine of the given value.

Arguments:

    Value - Supplies the value to take the hyperbolic cosine of.

Return Value:

    Returns the hyperbolic cosine on success.

    +/- HUGE_VAL (with the same sign as the value) if the result cannot be
    represented.

    NaN if the input is NaN.

    1.0 if the value is +/- 0.

    +Infinity if the value is +/- Infinity.

--*/

LIBC_API
float
coshf (
    float Value
    );

/*++

Routine Description:

    This routine computes the hyperbolic cosine of the given value.

Arguments:

    Value - Supplies the value to take the hyperbolic cosine of.

Return Value:

    Returns the hyperbolic cosine on success.

    +/- HUGE_VAL (with the same sign as the value) if the result cannot be
    represented.

    NaN if the input is NaN.

    1.0 if the value is +/- 0.

    +Infinity if the value is +/- Infinity.

--*/

LIBC_API
double
tanh (
    double Value
    );

/*++

Routine Description:

    This routine computes the hyperbolic tangent of the given value.

Arguments:

    Value - Supplies the value to take the hyperbolic tangent of.

Return Value:

    Returns the hyperbolic tangent on success.

    Returns the value itself if the value is +/- 0.

    Returns +/- 1 if the value is +/- Infinity.

    Returns the value itself with a range error if the value is subnormal.

--*/

LIBC_API
float
tanhf (
    float Value
    );

/*++

Routine Description:

    This routine computes the hyperbolic tangent of the given value.

Arguments:

    Value - Supplies the value to take the hyperbolic tangent of.

Return Value:

    Returns the hyperbolic tangent on success.

    Returns the value itself if the value is +/- 0.

    Returns +/- 1 if the value is +/- Infinity.

    Returns the value itself with a range error if the value is subnormal.

--*/

LIBC_API
double
ldexp (
    double Value,
    int Exponent
    );

/*++

Routine Description:

    This routine computes the given value times two raised to the given
    exponent efficiently. That is, Value * (2 ^ Exponent). On systems where
    FLT_RADIX is 2, this is equivalent to the scalbn function.

Arguments:

    Value - Supplies the value to multiply.

    Exponent - Supplies the exponent to raise two to.

Return Value:

    Returns the scaled value.

--*/

LIBC_API
float
ldexpf (
    float Value,
    int Exponent
    );

/*++

Routine Description:

    This routine computes the given value times two raised to the given
    exponent efficiently. That is, Value * (2 ^ Exponent). On systems where
    FLT_RADIX is 2, this is equivalent to the scalbn function.

Arguments:

    Value - Supplies the value to multiply.

    Exponent - Supplies the exponent to raise two to.

Return Value:

    Returns the scaled value.

--*/

LIBC_API
double
scalbn (
    double Value,
    int Exponent
    );

/*++

Routine Description:

    This routine computes the given value times FLT_RADIX raised to the given
    exponent efficiently. That is, Value * 2 ^ Exponent.

Arguments:

    Value - Supplies the value to multiply.

    Exponent - Supplies the exponent to raise the radix to.

Return Value:

    Returns the scaled value.

--*/

LIBC_API
float
scalbnf (
    float Value,
    int Exponent
    );

/*++

Routine Description:

    This routine computes the given value times FLT_RADIX raised to the given
    exponent efficiently. That is, Value * 2 ^ Exponent.

Arguments:

    Value - Supplies the value to multiply.

    Exponent - Supplies the exponent to raise the radix to.

Return Value:

    Returns the scaled value.

--*/

LIBC_API
double
ceil (
    double Value
    );

/*++

Routine Description:

    This routine computes the smallest integral value not less then the given
    value.

Arguments:

    Value - Supplies the value to compute the ceiling of.

Return Value:

    Returns the ceiling on success.

    NaN if the given value is NaN.

    Returns the value itself for +/- 0 and +/- Infinity.

--*/

LIBC_API
float
ceilf (
    float Value
    );

/*++

Routine Description:

    This routine computes the smallest integral value not less then the given
    value.

Arguments:

    Value - Supplies the value to compute the ceiling of.

Return Value:

    Returns the ceiling on success.

    NaN if the given value is NaN.

    Returns the value itself for +/- 0 and +/- Infinity.

--*/

LIBC_API
double
floor (
    double Value
    );

/*++

Routine Description:

    This routine computes the largest integral value not greater than the
    given value.

Arguments:

    Value - Supplies the value to use.

Return Value:

    Returns the largest integral value not greater than the input value.

--*/

LIBC_API
float
floorf (
    float Value
    );

/*++

Routine Description:

    This routine computes the largest integral value not greater than the
    given value.

Arguments:

    Value - Supplies the value to use.

Return Value:

    Returns the largest integral value not greater than the input value.

--*/

LIBC_API
double
fabs (
    double Value
    );

/*++

Routine Description:

    This routine returns the absolute value of the given value.

Arguments:

    Value - Supplies the value to get the absolute value of.

Return Value:

    Returns the absolute value.

--*/

LIBC_API
float
fabsf (
    float Value
    );

/*++

Routine Description:

    This routine returns the absolute value of the given value.

Arguments:

    Value - Supplies the value to get the absolute value of.

Return Value:

    Returns the absolute value.

--*/

LIBC_API
double
fmin (
    double FirstValue,
    double SecondValue
    );

/*++

Routine Description:

    This routine returns the minimum numeric value between the two given
    arguments. NaN arguments are treated as missing data. If one argument is
    NaN and the other is not, the numeric argument is returned.

Arguments:

    FirstValue - Supplies the first value to consider.

    SecondValue - Supplies the second value to consider.

Return Value:

    Returns the minimum of the two.

--*/

LIBC_API
float
fminf (
    float FirstValue,
    float SecondValue
    );

/*++

Routine Description:

    This routine returns the minimum numeric value between the two given
    arguments. NaN arguments are treated as missing data. If one argument is
    NaN and the other is not, the numeric argument is returned.

Arguments:

    FirstValue - Supplies the first value to consider.

    SecondValue - Supplies the second value to consider.

Return Value:

    Returns the minimum of the two.

--*/

LIBC_API
double
fmax (
    double FirstValue,
    double SecondValue
    );

/*++

Routine Description:

    This routine returns the maximum numeric value between the two given
    arguments. NaN arguments are treated as missing data. If one argument is
    NaN and the other is not, the numeric argument is returned.

Arguments:

    FirstValue - Supplies the first value to consider.

    SecondValue - Supplies the second value to consider.

Return Value:

    Returns the maximum of the two.

--*/

LIBC_API
float
fmaxf (
    float FirstValue,
    float SecondValue
    );

/*++

Routine Description:

    This routine returns the maximum numeric value between the two given
    arguments. NaN arguments are treated as missing data. If one argument is
    NaN and the other is not, the numeric argument is returned.

Arguments:

    FirstValue - Supplies the first value to consider.

    SecondValue - Supplies the second value to consider.

Return Value:

    Returns the maximum of the two.

--*/

LIBC_API
double
fmod (
    double Dividend,
    double Divisor
    );

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

LIBC_API
float
fmodf (
    float Dividend,
    float Divisor
    );

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

LIBC_API
double
round (
    double Value
    );

/*++

Routine Description:

    This routine rounds the given value to the nearest integer. Rounding
    halfway leans away from zero regardless of the current rounding direction.

Arguments:

    Value - Supplies the value to round.

Return Value:

    Returns the rounded value.

--*/

LIBC_API
float
roundf (
    float Value
    );

/*++

Routine Description:

    This routine rounds the given value to the nearest integer. Rounding
    halfway leans away from zero regardless of the current rounding direction.

Arguments:

    Value - Supplies the value to round.

Return Value:

    Returns the rounded value.

--*/

LIBC_API
long
lround (
    double Value
    );

/*++

Routine Description:

    This routine rounds the given value to the nearest integer value, rounding
    halfway cases away from zero, regardless of the current rounding direction.

Arguments:

    Value - Supplies the value to round.

Return Value:

    Returns the rounded integer on success.

    Returns an unspecified value if the given value is out of range, or NaN.

--*/

LIBC_API
long
lroundf (
    float Value
    );

/*++

Routine Description:

    This routine rounds the given value to the nearest integer value, rounding
    halfway cases away from zero, regardless of the current rounding direction.

Arguments:

    Value - Supplies the value to round.

Return Value:

    Returns the rounded integer on success.

    Returns an unspecified value if the given value is out of range, or NaN.

--*/

LIBC_API
long long
llround (
    double Value
    );

/*++

Routine Description:

    This routine rounds the given value to the nearest integer value, rounding
    halfway cases away from zero, regardless of the current rounding direction.

Arguments:

    Value - Supplies the value to round.

Return Value:

    Returns the rounded integer on success.

    Returns an unspecified value if the given value is out of range, or NaN.

--*/

LIBC_API
long long
llroundf (
    float Value
    );

/*++

Routine Description:

    This routine rounds the given value to the nearest integer value, rounding
    halfway cases away from zero, regardless of the current rounding direction.

Arguments:

    Value - Supplies the value to round.

Return Value:

    Returns the rounded integer on success.

    Returns an unspecified value if the given value is out of range, or NaN.

--*/

LIBC_API
long
lrint (
    double Value
    );

/*++

Routine Description:

    This routine round the given value to the nearest integer, using the
    current rounding direction.

Arguments:

    Value - Supplies the value to round into an integral.

Return Value:

    Returns the nearest integer value.

    Returns an undefined value if the integer is NaN or out of range.

--*/

LIBC_API
long
lrintf (
    float Value
    );

/*++

Routine Description:

    This routine round the given value to the nearest integer, using the
    current rounding direction.

Arguments:

    Value - Supplies the value to round into an integral.

Return Value:

    Returns the nearest integer value.

    Returns an undefined value if the integer is NaN or out of range.

--*/

LIBC_API
long long
llrint (
    double Value
    );

/*++

Routine Description:

    This routine round the given value to the nearest integer, using the
    current rounding direction.

Arguments:

    Value - Supplies the value to round into an integral.

Return Value:

    Returns the nearest integer value.

    Returns an undefined value if the integer is NaN or out of range.

--*/

LIBC_API
long long
llrintf (
    float Value
    );

/*++

Routine Description:

    This routine round the given value to the nearest integer, using the
    current rounding direction.

Arguments:

    Value - Supplies the value to round into an integral.

Return Value:

    Returns the nearest integer value.

    Returns an undefined value if the integer is NaN or out of range.

--*/

LIBC_API
double
nearbyint (
    double Value
    );

/*++

Routine Description:

    This routine round the given value to the nearest integer, using the
    current rounding direction. This routine does not raise an inexact
    exception.

Arguments:

    Value - Supplies the value to round into an integral.

Return Value:

    Returns the nearest integral value in the direction of the current rounding
    mode.

    NaN if the given value is NaN.

    Returns the value itself for +/- 0 and +/- Infinity.

--*/

LIBC_API
float
nearbyintf (
    float Value
    );

/*++

Routine Description:

    This routine round the given value to the nearest integer, using the
    current rounding direction. This routine does not raise an inexact
    exception.

Arguments:

    Value - Supplies the value to round into an integral.

Return Value:

    Returns the nearest integral value in the direction of the current rounding
    mode.

    NaN if the given value is NaN.

    Returns the value itself for +/- 0 and +/- Infinity.

--*/

LIBC_API
double
rint (
    double Value
    );

/*++

Routine Description:

    This routine converts the given value into the nearest integral in the
    direction of the current rounding mode.

Arguments:

    Value - Supplies the value to round into an integral.

Return Value:

    Returns the nearest integral value in the direction of the current rounding
    mode.

--*/

LIBC_API
float
rintf (
    float Value
    );

/*++

Routine Description:

    This routine converts the given value into the nearest integral in the
    direction of the current rounding mode.

Arguments:

    Value - Supplies the value to round into an integral.

Return Value:

    Returns the nearest integral value in the direction of the current rounding
    mode.

    NaN if the given value is NaN.

    Returns the value itself for +/- 0 and +/- Infinity.

--*/

LIBC_API
double
hypot (
    double Length,
    double Width
    );

/*++

Routine Description:

    This routine computes the square root of a2 + b2 without undue overflow
    or underflow.

Arguments:

    Length - Supplies the length of the triangle.

    Width - Supplies the width of the triangle.

Return Value:

    Returns the hypotenuse of the triangle.

--*/

LIBC_API
float
hypotf (
    float Length,
    float Width
    );

/*++

Routine Description:

    This routine computes the square root of a2 + b2 without undue overflow
    or underflow.

Arguments:

    Length - Supplies the length of the triangle.

    Width - Supplies the width of the triangle.

Return Value:

    Returns the hypotenuse of the triangle.

--*/

LIBC_API
double
sqrt (
    double Value
    );

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

LIBC_API
float
sqrtf (
    float Value
    );

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

LIBC_API
double
frexp (
    double Value,
    int *Exponent
    );

/*++

Routine Description:

    This routine breaks a floating point number down into a normalized fraction
    and an integer power of 2.

Arguments:

    Value - Supplies the value to normalize.

    Exponent - Supplies a pointer where the exponent will be returned.

Return Value:

    Returns the normalized fraction (the significand).

--*/

LIBC_API
float
frexpf (
    float Value,
    int *Exponent
    );

/*++

Routine Description:

    This routine breaks a floating point number down into a normalized fraction
    and an integer power of 2.

Arguments:

    Value - Supplies the value to normalize.

    Exponent - Supplies a pointer where the exponent will be returned.

Return Value:

    Returns the normalized fraction (the significand).

--*/

LIBC_API
double
exp (
    double Value
    );

/*++

Routine Description:

    This routine computes the base e exponential of the given value.

Arguments:

    Value - Supplies the value to raise e to.

Return Value:

    Returns e to the given value.

--*/

LIBC_API
float
expf (
    float Value
    );

/*++

Routine Description:

    This routine computes the base e exponential of the given value.

Arguments:

    Value - Supplies the value to raise e to.

Return Value:

    Returns e to the given value.

--*/

LIBC_API
double
exp2 (
    double Value
    );

/*++

Routine Description:

    This routine computes the base 2 exponential of the given value.

Arguments:

    Value - Supplies the value to raise 2 to.

Return Value:

    Returns 2 to the given value.

--*/

LIBC_API
float
exp2f (
    float Value
    );

/*++

Routine Description:

    This routine computes the base 2 exponential of the given value.

Arguments:

    Value - Supplies the value to raise 2 to.

Return Value:

    Returns 2 to the given value.

--*/

LIBC_API
double
expm1 (
    double Value
    );

/*++

Routine Description:

    This routine computes the base e exponential of the given value, minus one.

Arguments:

    Value - Supplies the value to raise e to.

Return Value:

    Returns e to the given value, minus one.

--*/

LIBC_API
float
expm1f (
    float Value
    );

/*++

Routine Description:

    This routine computes the base e exponential of the given value, minus one.

Arguments:

    Value - Supplies the value to raise e to.

Return Value:

    Returns e to the given value, minus one.

--*/

LIBC_API
double
pow (
    double Value,
    double Power
    );

/*++

Routine Description:

    This routine raises the given value to the given power.

Arguments:

    Value - Supplies the value to raise.

    Power - Supplies the power to raise the value to.

Return Value:

    Returns the given value raised to the given power.

--*/

LIBC_API
float
powf (
    float Value,
    float Power
    );

/*++

Routine Description:

    This routine raises the given value to the given power.

Arguments:

    Value - Supplies the value to raise.

    Power - Supplies the power to raise the value to.

Return Value:

    Returns the given value raised to the given power.

--*/

LIBC_API
double
log (
    double Value
    );

/*++

Routine Description:

    This routine returns the natural logarithm (base e) of the given value.

Arguments:

    Value - Supplies the value to get the logarithm of.

Return Value:

    Returns the logarithm of the given value.

--*/

LIBC_API
float
logf (
    float Value
    );

/*++

Routine Description:

    This routine returns the natural logarithm (base e) of the given value.

Arguments:

    Value - Supplies the value to get the logarithm of.

Return Value:

    Returns the logarithm of the given value.

--*/

LIBC_API
double
log2 (
    double Value
    );

/*++

Routine Description:

    This routine implements the base two logarithm function.

Arguments:

    Value - Supplies the value to take the base 2 logarithm of.

Return Value:

    Returns the base 2 logarithm of the given value.

--*/

LIBC_API
float
log2f (
    float Value
    );

/*++

Routine Description:

    This routine implements the base two logarithm function.

Arguments:

    Value - Supplies the value to take the base 2 logarithm of.

Return Value:

    Returns the base 2 logarithm of the given value.

--*/

LIBC_API
double
log10 (
    double Value
    );

/*++

Routine Description:

    This routine returns the base 10 logarithm of the given value.

Arguments:

    Value - Supplies the value to get the logarithm of.

Return Value:

    Returns the logarithm of the given value.

--*/

LIBC_API
float
log10f (
    float Value
    );

/*++

Routine Description:

    This routine returns the base 10 logarithm of the given value.

Arguments:

    Value - Supplies the value to get the logarithm of.

Return Value:

    Returns the logarithm of the given value.

--*/

LIBC_API
double
modf (
    double Value,
    double *IntegerPortion
    );

/*++

Routine Description:

    This routine breaks the given value up into integral and fractional parts,
    each of which has the same sign as the argument. It stores the integral
    part as a floating point value.

Arguments:

    Value - Supplies the value to decompose into an integer and a fraction.

    IntegerPortion - Supplies a pointer where the integer portion of the
        value will be returned. If the given value is NaN or +/- Infinity, then
        NaN or +/- Infinity will be returned.

Return Value:

    Returns the fractional portion of the given value on success.

    NaN if the input is NaN.

    0 if +/- Infinity is given.

--*/

LIBC_API
float
modff (
    float Value,
    float *IntegerPortion
    );

/*++

Routine Description:

    This routine breaks the given value up into integral and fractional parts,
    each of which has the same sign as the argument. It stores the integral
    part as a floating point value.

Arguments:

    Value - Supplies the value to decompose into an integer and a fraction.

    IntegerPortion - Supplies a pointer where the integer portion of the
        value will be returned. If the given value is NaN or +/- Infinity, then
        NaN or +/- Infinity will be returned.

Return Value:

    Returns the fractional portion of the given value on success.

    NaN if the input is NaN.

    0 if +/- Infinity is given.

--*/

LIBC_API
double
copysign (
    double Value,
    double Sign
    );

/*++

Routine Description:

    This routine replaces the sign bit on the given value with the sign bit
    from the other given value.

Arguments:

    Value - Supplies the value to modify.

    Sign - Supplies the double with the desired sign bit.

Return Value:

    Returns the value with the modified sign bit.

--*/

LIBC_API
float
copysignf (
    float Value,
    float Sign
    );

/*++

Routine Description:

    This routine replaces the sign bit on the given value with the sign bit
    from the other given value.

Arguments:

    Value - Supplies the value to modify.

    Sign - Supplies the float with the desired sign bit.

Return Value:

    Returns the value with the modified sign bit.

--*/

LIBC_API
double
trunc (
    double Value
    );

/*++

Routine Description:

    This routine truncates the value to an integer, nearest to but not greater
    in magnitude than the argument.

Arguments:

    Value - Supplies the value to truncated.

Return Value:

    Returns the nearest integer.

--*/

LIBC_API
float
truncf (
    float Value
    );

/*++

Routine Description:

    This routine truncates the value to an integer, nearest to but not greater
    in magnitude than the argument.

Arguments:

    Value - Supplies the value to truncated.

Return Value:

    Returns the nearest integer.

--*/

#ifdef __cplusplus

}

#endif
#endif

