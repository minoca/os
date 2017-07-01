/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    intrinsa.S

Abstract:

    This module implements ARM processor architecture compiler intrinsics not
    implementable in C that are common amongst different versions of ARM.

Author:

    Chris Stevens 18-Mar-2014

Environment:

    Kernel mode

--*/

//
// ------------------------------------------------------------------ Includes
//

#include <minoca/kernel/arm.inc>

//
// --------------------------------------------------------------- Definitions
//

//
// ---------------------------------------------------------------------- Code
//

ASSEMBLY_FILE_HEADER

//
// SIGNED_INT64_DIVIDE_RESULT
// __aeabi_ldivmod (
//    long long Numerator,
//    long long Denominator
//    )
//

/*++

Routine Description:

    This routine performs a 64-bit divide and modulo of two signed integers.

Arguments:

    Numerator - Supplies the numerator of the division.

    Denominator - Supplies the denominator of the division.

Return Value:

    Returns the result of the division and modulo in the registers R0-R3.

--*/

PROTECTED_FUNCTION __aeabi_ldivmod
    push    {%lr}
    sub     %sp, %sp, #16
    add     %r12, %sp, #8
    str     %r12, [%sp]
    blx     RtlDivideModulo64
    ldr     %r2, [%sp, #8]
    ldr     %r3, [%sp, #12]
    add     %sp, %sp, #16
    pop     {%pc}
END_FUNCTION __aeabi_ldivmod

//
// UNSIGNED_INT64_DIVIDE_RESULT
// __aeabi_uldivmod (
//    long long Numerator,
//    long long Denominator
//    )
//

/*++

Routine Description:

    This routine performs a 64-bit divide and modulo of two unsigned integers.

Arguments:

    Numerator - Supplies the numerator of the division.

    Denominator - Supplies the denominator of the division.

Return Value:

    Returns the result of the division and modulo in the registers R0-R3.

--*/

PROTECTED_FUNCTION __aeabi_uldivmod
    push    {%lr}
    sub     %sp, %sp, #16
    add     %r12, %sp, #8
    str     %r12, [%sp]
    blx     RtlDivideUnsigned64
    ldr     %r2, [%sp, #8]
    ldr     %r3, [%sp, #12]
    add     %sp, %sp, #16
    pop     {%pc}
END_FUNCTION __aeabi_uldivmod

//
// SIGNED_INT_DIVIDE_RESULT
// __aeabi_idivmod (
//     unsigned int Numerator,
//     unsigned int Denominator
//     )
//

/*++

Routine Description:

    This routine performs a 32-bit divide and modulo of two signed integers.

Arguments:

    Numerator - Supplies the numerator of the division.

    Denominator - Supplies the denominator of the division.

Return Value:

    Returns the quotient and remainder in R0 and R1.

--*/

PROTECTED_FUNCTION __aeabi_idivmod
    push    {%lr}
    sub     %sp, %sp, #4
    mov     %r2, %sp
    blx     RtlDivideModulo32
    ldr     %r1, [%sp]
    add     %sp, %sp, #4
    pop     {%pc}
END_FUNCTION __aeabi_idivmod

//
// UNSIGNED_INT_DIVIDE_RESULT
// __aeabi_uidivmod (
//     unsigned int Numerator,
//     unsigned int Denominator
//     )
//

/*++

Routine Description:

    This routine performs a 32-bit divide and modulo of two unsigned integers.

Arguments:

    Numerator - Supplies the numerator of the division.

    Denominator - Supplies the denominator of the division.

Return Value:

    Returns the quotient and the remainder in R0 and R1.

--*/

PROTECTED_FUNCTION __aeabi_uidivmod
    push    {%lr}
    sub     %sp, %sp, #4
    mov     %r2, %sp
    blx     RtlDivideUnsigned32
    ldr     %r1, [%sp]
    add     %sp, %sp, #4
    pop     {%pc}
END_FUNCTION __aeabi_uidivmod

//
// ULONGLONG
// __aeabi_llsl (
//     ULONGLONG Value
//     ULONG ShiftCount
//     )
//

/*++

Routine Description:

    This routine performs a performs a logical shift left of a given value by
    a given amount.

Arguments:

    Value - Supplies the value to shift.

    ShiftCount - Supplies the number of bits to shift.

Return Value:

    Returns the shifted value.

--*/

PROTECTED_FUNCTION __aeabi_llsl
    subs    %r3, %r2, #32
    rsb     %r12, %r2, #32
    ITTTE(mi)
    movmi   %r1, %r1, lsl %r2
.if THUMB
    lsrmi   %r3, %r0, %r12
    orrmi   %r1, %r1, %r2
.else
    orrmi   %r1, %r1, %r0, lsr %r12
.endif
    movpl   %r1, %r0, lsl %r3
    lsl     %r0, %r0, %r2
    bx      %lr

END_FUNCTION __aeabi_llsl

//
// ULONGLONG
// __aeabi_llsr (
//     ULONGLONG Value
//     ULONG ShiftCount
//     )
//

/*++

Routine Description:

    This routine performs a performs a logical shift right of a given value by
    a given amount.

Arguments:

    Value - Supplies the value to shift.

    ShiftCount - Supplies the number of bits to shift.

Return Value:

    Returns the shifted value.

--*/

PROTECTED_FUNCTION __aeabi_llsr
    subs    %r3, %r2, #32
    rsb     %r12, %r2, #32
    ITTTE(mi)
    movmi   %r0, %r0, lsr %r2
.if THUMB
    lslmi   %r3, %r1, %r12
    orrmi   %r0, %r0, %r3
.else
    orrmi   %r0, %r0, %r1, lsl %r12
.endif
    movpl   %r0, %r1, lsr %r3
    lsr     %r1, %r1, %r2
    bx      %lr

END_FUNCTION __aeabi_llsr

//
// ULONGLONG
// __aeabi_lasr (
//     ULONGLONG Value
//     ULONG ShiftCount
//     )
//

/*++

Routine Description:

    This routine performs a performs an arithmetic shift right of a given value
    by a given amount.

Arguments:

    Value - Supplies the value to shift.

    ShiftCount - Supplies the number of bits to shift.

Return Value:

    Returns the shifted value.

--*/

PROTECTED_FUNCTION __aeabi_lasr
    subs    %r3, %r2, #32
    rsb     %r12, %r2, #32
    ITTTE(mi)
    movmi   %r0, %r0, lsr %r2
.if THUMB
    lslmi   %r3, %r1, %r12
    orrmi   %r0, %r0, %r3
.else
    orrmi   %r0, %r0, %r1, lsl %r12
.endif
    movpl   %r0, %r1, asr %r3
    asr     %r1, %r1, %r2
    bx      %lr

END_FUNCTION __aeabi_lasr

//
// --------------------------------------------------------- Internal Functions
//

