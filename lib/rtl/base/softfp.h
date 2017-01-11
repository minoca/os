/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    softfp.h

Abstract:

    This header contains internal definitions for the soft floating point
    library.

Author:

    Evan Green 12-Nov-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// This macro packs the given sign, exponent, and significand, returning the
// ULONG form of a float.
//

#define FLOAT_PACK(_Sign, _Exponent, _Significand)                  \
    (((ULONG)(_Sign)) << FLOAT_SIGN_BIT_SHIFT) +                    \
    (((ULONG)(_Exponent)) << FLOAT_EXPONENT_SHIFT) + (ULONG)(_Significand)

//
// This macro gets the sign out of a float parts structure.
//

#define FLOAT_GET_SIGN(_Parts) ((_Parts).Ulong >> FLOAT_SIGN_BIT_SHIFT)

//
// This macro gets the exponent out of a float parts structure.
//

#define FLOAT_GET_EXPONENT(_Parts) \
    (((_Parts).Ulong & FLOAT_EXPONENT_MASK) >> FLOAT_EXPONENT_SHIFT)

//
// This macro gets the significand out of a float parts structure.
//

#define FLOAT_GET_SIGNIFICAND(_Parts) ((_Parts).Ulong & FLOAT_VALUE_MASK)

//
// This macro returns non-zero if the given value (in float parts) is NaN.
//

#define FLOAT_IS_NAN(_Parts) ((ULONG)((_Parts).Ulong << 1) > 0xFF000000UL)

//
// This macro returns non-zero if the given value (in float parts) is a
// signaling NaN.
//

#define FLOAT_IS_SIGNALING_NAN(_Parts)                                      \
    ((((_Parts).Ulong >> (FLOAT_EXPONENT_SHIFT - 1) & 0x1FF) == 0x1FE) &&   \
     ((_Parts).Ulong == 0x003FFFFF))

//
// This macro returns the sign bit of the given double parts.
//

#define DOUBLE_GET_SIGN(_Parts) \
    (((_Parts).Ulong.High & (DOUBLE_SIGN_BIT >> DOUBLE_HIGH_WORD_SHIFT)) != 0)

//
// This macro packs up the given sign, exponent, and significand, returning the
// ULONGLONG form of a double.
//

#define DOUBLE_PACK(_Sign, _Exponent, _Significand)         \
    (((ULONGLONG)(_Sign) << DOUBLE_SIGN_BIT_SHIFT) +        \
     ((ULONGLONG)(_Exponent) << DOUBLE_EXPONENT_SHIFT) +    \
     (ULONGLONG)(_Significand))

//
//
// This macro extracts the exponent from the given double parts structure.
//

#define DOUBLE_GET_EXPONENT(_Parts)                         \
    (((_Parts).Ulong.High &                                 \
      (DOUBLE_EXPONENT_MASK >> DOUBLE_HIGH_WORD_SHIFT)) >>  \
     (DOUBLE_EXPONENT_SHIFT - DOUBLE_HIGH_WORD_SHIFT))

//
// This macro extracts the significand portion of the given double parts.
//

#define DOUBLE_GET_SIGNIFICAND(_Parts) ((_Parts).Ulonglong & DOUBLE_VALUE_MASK)

//
// This macro returns non-zero if the given value (in double parts) is NaN.
//

#define DOUBLE_IS_NAN(_Parts) \
    ((ULONGLONG)((_Parts).Ulonglong << 1) > 0xFFE0000000000000ULL)

//
// This macro returns non-zero if the given value (in double parts) is a
// signaling NaN.
//

#define DOUBLE_IS_SIGNALING_NAN(_Parts)                 \
    (((((_Parts).Ulonglong >> 51) & 0xFFF) == 0xFFE) && \
     (((_Parts).Ulonglong & 0x0007FFFFFFFFFFFFULL) != 0))

//
// ---------------------------------------------------------------- Definitions
//

//
// Define soft float exception flags.
//

#define SOFT_FLOAT_INEXACT          0x00000001
#define SOFT_FLOAT_UNDERFLOW        0x00000002
#define SOFT_FLOAT_OVERFLOW         0x00000004
#define SOFT_FLOAT_DIVIDE_BY_ZERO   0x00000008
#define SOFT_FLOAT_INVALID          0x00000010

//
// Define a default NaN value.
//

#define FLOAT_DEFAULT_NAN  0xFFC00000UL
#define DOUBLE_DEFAULT_NAN 0xFFF8000000000000ULL

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _SOFT_FLOAT_ROUNDING_MODE {
    SoftFloatRoundNearestEven = 0,
    SoftFloatRoundDown        = 1,
    SoftFloatRoundUp          = 2,
    SoftFloatRoundToZero      = 3
} SOFT_FLOAT_ROUNDING_MODE, *PSOFT_FLOAT_ROUNDING_MODE;

typedef enum _SOFT_FLOAT_DETECT_TININESS {
    SoftFloatTininessAfterRounding  = 0,
    SoftFloatTininessBeforeRounding = 1,
} SOFT_FLOAT_DETECT_TININESS, *PSOFT_FLOAT_DETECT_TININESS;

typedef struct _COMMON_NAN {
    CHAR Sign;
    ULONGLONG High;
    ULONGLONG Low;
} COMMON_NAN, *PCOMMON_NAN;

//
// -------------------------------------------------------------------- Globals
//

//
// Define global exception flags.
//

extern ULONG RtlSoftFloatExceptionFlags;

//
// Define the soft float rounding mode.
//

extern SOFT_FLOAT_ROUNDING_MODE RtlRoundingMode;

//
// Define the method for detecting very small values.
//

extern SOFT_FLOAT_DETECT_TININESS RtlTininessDetection;

//
// -------------------------------------------------------- Function Prototypes
//

VOID
RtlpSoftFloatRaise (
    ULONG Flags
    );

/*++

Routine Description:

    This routine raises the given conditions in the soft float implementation.

Arguments:

    Flags - Supplies the flags to raise.

Return Value:

    None.

--*/

float
RtlpRoundAndPackFloat (
    CHAR SignBit,
    SHORT Exponent,
    ULONG Significand
    );

/*++

Routine Description:

    This routine takes a sign, exponent, and significand and creates the
    proper rounded floating point value from that input. Overflow and
    underflow can be raised here.

Arguments:

    SignBit - Supplies a non-zero value if the value should be negated before
        being converted.

    Exponent - Supplies the exponent for the value.

    Significand - Supplies the significand with its binary point between bits
        30 and 29, which is 7 bits to the left of its usual location. The
        shifted exponent must be normalized or smaller. If the significand is
        not normalized, the exponent must be 0. In that case, the result
        returned is a subnormal number, and it must not require rounding. In
        the normal case wehre the significand is normalized, the exponent must
        be one less than the true floating point exponent.

Return Value:

    Returns the float representation of the given components.

--*/

float
RtlpNormalizeRoundAndPackFloat (
    CHAR SignBit,
    SHORT Exponent,
    ULONG Significand
    );

/*++

Routine Description:

    This routine takes a sign, exponent, and significand and creates the
    proper rounded single floating point value from that input. Overflow and
    underflow can be raised here. This routine is very similar to the "round
    and pack float" routine except that the significand does not have to be
    normalized. Bit 31 of the significand must be zero, and the exponent must
    be one less than the true floating point exponent.

Arguments:

    SignBit - Supplies a non-zero value if the value should be negated before
        being converted.

    Exponent - Supplies the exponent for the value.

    Significand - Supplies the significand with its binary point between bits
        30 and 29, which is 7 bits to the left of its usual location.

Return Value:

    Returns the float representation of the given components.

--*/

double
RtlpRoundAndPackDouble (
    CHAR SignBit,
    SHORT Exponent,
    ULONGLONG Significand
    );

/*++

Routine Description:

    This routine takes a sign, exponent, and significand and creates the
    proper rounded double floating point value from that input. Overflow and
    underflow can be raised here.

Arguments:

    SignBit - Supplies a non-zero value if the value should be negated before
        being converted.

    Exponent - Supplies the exponent for the value.

    Significand - Supplies the significand with its binary point between bits
        62 and 61, which is 10 bits to the left of its usual location. The
        shifted exponent must be normalized or smaller. If the significand is
        not normalized, the exponent must be 0. In that case, the result
        returned is a subnormal number, and it must not require rounding. In
        the normal case wehre the significand is normalized, the exponent must
        be one less than the true floating point exponent.

Return Value:

    Returns the double representation of the given components.

--*/

double
RtlpNormalizeRoundAndPackDouble (
    CHAR SignBit,
    SHORT Exponent,
    ULONGLONG Significand
    );

/*++

Routine Description:

    This routine takes a sign, exponent, and significand and creates the
    proper rounded double floating point value from that input. Overflow and
    underflow can be raised here. This routine is very similar to the "round
    and pack double" routine except that the significand does not have to be
    normalized. Bit 63 of the significand must be zero, and the exponent must
    be one less than the true floating point exponent.

Arguments:

    SignBit - Supplies a non-zero value if the value should be negated before
        being converted.

    Exponent - Supplies the exponent for the value.

    Significand - Supplies the significand with its binary point between bits
        62 and 61, which is 10 bits to the left of its usual location.

Return Value:

    Returns the double representation of the given components.

--*/

VOID
RtlpShift32RightJamming (
    ULONG Value,
    SHORT Count,
    PULONG Result
    );

/*++

Routine Description:

    This routine shifts the given value right by the requested number of bits.
    If any bits are shifted off the right, the least significant bit is set.
    The imagery is that the bits get "jammed" on the end as they try to fall
    off.

Arguments:

    Value - Supplies the value to shift.

    Count - Supplies the number of bits to shift by.

    Result - Supplies a pointer where the result will be stored.

Return Value:

    None.

--*/

VOID
RtlpShift64RightJamming (
    ULONGLONG Value,
    SHORT Count,
    PULONGLONG Result
    );

/*++

Routine Description:

    This routine shifts the given value right by the requested number of bits.
    If any bits are shifted off the right, the least significant bit is set.
    The imagery is that the bits get "jammed" on the end as they try to fall
    off.

Arguments:

    Value - Supplies the value to shift.

    Count - Supplies the number of bits to shift by.

    Result - Supplies a pointer where the result will be stored.

Return Value:

    None.

--*/

