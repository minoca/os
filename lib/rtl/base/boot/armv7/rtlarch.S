/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    rtlarch.S

Abstract:

    This module implements ARM processor architecture features not
    implementable in C for the boot environment.

Author:

    Chris Stevens 18-Mar-2014

Environment:

    Boot

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
// VOID
// RtlMemoryBarrier (
//     VOID
//     )
//

/*++

Routine Description:

    This routine provides a full memory barrier, ensuring that all memory
    accesses occurring before this function complete before any memory accesses
    after this function start.

Arguments:

    None.

Return Value:

    None.

--*/

FUNCTION RtlMemoryBarrier
    DMB
    bx      %lr

END_FUNCTION RtlMemoryBarrier

//
// RTL_API
// VOID
// RtlDebugBreak (
//     VOID
//     )
//

/*++

Routine Description:

    This routine causes a break into the debugger.

Arguments:

    None.

Return Value:

    None.

--*/

PROTECTED_FUNCTION RtlDebugBreak
    DEBUGGER_BREAK                              @ Debugger break.
    bx  %lr                                     @ Return.

END_FUNCTION RtlDebugBreak

//
// RTL_API
// VOID
// RtlDebugService (
//     UINTN ServiceRequest,
//     PVOID Parameter
//     )
//

/*++

Routine Description:

    This routine enters the debugger for a service request.

Arguments:

    ServiceRequest - Supplies the reason for entering the debugger.

    Parameter - Supplies the parameter to pass to the debug service routine.

Return Value:

    None.

--*/

PROTECTED_FUNCTION RtlDebugService
    DEBUG_SERVICE
    bx      %lr                                 @ Return.

END_FUNCTION RtlDebugService

//
// ULONG
// RtlAtomicExchange32 (
//     volatile ULONG *Address,
//     ULONG ExchangeValue
//     )
//

/*++

Routine Description:

    This routine atomically exchanges the value at the given memory address
    with the given value.

Arguments:

    Address - Supplies the address of the value to exchange with.

    ExchangeValue - Supplies the value to write to the address.

Return Value:

    Returns the original value at the given address.

--*/

FUNCTION RtlAtomicExchange32
    ldr     %r2, [%r0]                          @ Get *Address.
    str     %r1, [%r0]                          @ Store new value.
    mov     %r0, %r2                            @ Move loaded value to R0.
    bx      %lr                                 @ Return R0.

END_FUNCTION RtlAtomicExchange32

//
// ULONG
// RtlAtomicCompareExchange32 (
//     volatile ULONG *Address,
//     ULONG ExchangeValue,
//     ULONG CompareValue
//     )
//

/*++

Routine Description:

    This routine atomically compares memory at the given address with a value
    and exchanges it with another value if they are equal.

Arguments:

    Address - Supplies the address of the value to compare and potentially
        exchange.

    ExchangeValue - Supplies the value to write to Address if the comparison
        returns equality.

    CompareValue - Supplies the value to compare against.

Return Value:

    Returns the original value at the given address.

--*/

FUNCTION RtlAtomicCompareExchange32
    ldr     %r3, [%r0]                          @ Get *Address.
    cmp     %r3, %r2                            @ Compare to CompareValue
    bne     RtlAtomicCompareExchange32End       @ If not equal, exit.
    str     %r1, [%r0]                          @ Store.

RtlAtomicCompareExchange32End:
    mov     %r0, %r3                            @ Move loaded value to R0.
    bx      %lr                                 @ Return R0.

END_FUNCTION RtlAtomicCompareExchange32

//
// ULONG
// RtlAtomicCompareExchange64 (
//     volatile ULONGLONG *Address,
//     ULONGLONG ExchangeValue,
//     ULONGLONG CompareValue
//     )
//

/*++

Routine Description:

    This routine atomically compares memory at the given address with a value
    and exchanges it with another value if they are equal.

Arguments:

    Address - Supplies the address of the value to compare and potentially
        exchange.

    ExchangeValue - Supplies the value to write to Address if the comparison
        returns equality.

    CompareValue - Supplies the value to compare against.

Return Value:

    Returns the original value at the given address.

--*/

FUNCTION RtlAtomicCompareExchange64
    stmdb   %sp!, {%r4-%r7}                     @ Save nonvolatiles.
    ldr     %r5, [%sp, #16]                     @ Get the last argument.
    mov     %r4, %r3
    ldrd    %r6, %r7, [%r0]                     @ Get *Address.
    cmp     %r6, %r4                            @ Compare to CompareValue
    bne     RtlAtomicCompareExchange64End       @ If not equal, exit.
    cmp     %r7, %r5                            @ Compare to CompareValue
    bne     RtlAtomicCompareExchange64End       @ If not equal, exit.
    mov     %r3, %r2                            @ Shift arguments over so the
    mov     %r2, %r1                            @ operand register is even.
    strd    %r2, %r3, [%r0]                     @ Store.

RtlAtomicCompareExchange64End:
    mov     %r0, %r6                            @ Move original low part to R0.
    mov     %r1, %r7                            @ Move original high part to R1.
    ldmia   %sp!, {%r4-%r7}                     @ Restore nonvolatiles.
    bx      %lr                                 @ Return R0.

END_FUNCTION RtlAtomicCompareExchange64

//
// ULONG
// RtlAtomicAdd32 (
//     volatile ULONG *Address,
//     ULONG Increment
//     )
//

/*++

Routine Description:

    This routine atomically adds the given amount to a 32-bit variable.

Arguments:

    Address - Supplies the address of the value to atomically add to.

    Increment - Supplies the amount to add.

Return Value:

    Returns the value before the atomic addition was performed.

--*/

FUNCTION RtlAtomicAdd32
    ldr     %r2, [%r0]                          @ Get *Address.
    add     %r3, %r2, %r1                       @ Add increment.
    str     %r3, [%r0]                          @ Store the result.
    mov     %r0, %r2                            @ Return original value.
    bx      %lr                                 @ Return.

END_FUNCTION RtlAtomicAdd32

//
// ULONG
// RtlAtomicAdd64 (
//     volatile ULONGLONG *Address,
//     ULONGLONG Increment
//     )
//

/*++

Routine Description:

    This routine atomically adds the given amount to a 64-bit variable.

Arguments:

    Address - Supplies the address of the value to atomically add to.

    Increment - Supplies the amount to add.

Return Value:

    Returns the value before the atomic addition was performed.

--*/

FUNCTION RtlAtomicAdd64
    stmdb   %sp!, {%r4-%r7}                     @ Save nonvolatiles.
    ldrd    %r4, %r5, [%r0]                     @ Get *Address.
    adds    %r6, %r4, %r1                       @ Add low word, setting flags.
    adc     %r7, %r5, %r2                       @ Add high word, with carry.
    strd    %r6, %r7, [%r0]                     @ Store the result.
    mov     %r0, %r4                            @ Move original low part to R0.
    mov     %r1, %r5                            @ Move original high part to R1.
    ldmia   %sp!, {%r4-%r7}                     @ Restore nonvolatiles.
    bx      %lr                                 @ Return.

END_FUNCTION RtlAtomicAdd64

//
// ULONG
// RtlAtomicOr32 (
//     volatile ULONG *Address,
//     ULONG Mask
//     )
//

/*++

Routine Description:

    This routine atomically ORs the given mask to a 32-bit variable.

Arguments:

    Address - Supplies the address of the value to atomically OR with.

    Mask - Supplies the bitmask to logically OR in to the value.

Return Value:

    Returns the value before the atomic operation was performed.

--*/

FUNCTION RtlAtomicOr32
    ldr     %r2, [%r0]                          @ Get *Address.
    orr     %r3, %r2, %r1                       @ OR in mask.
    str     %r3, [%r0]                          @ Store the result.
    mov     %r0, %r2                            @ Return original value.
    bx      %lr                                 @ Return.

END_FUNCTION RtlAtomicOr32

//
// ULONGLONG
// RtlAtomicOr64 (
//     volatile ULONGLONG *Address,
//     ULONGLONG Mask
//     )
//

/*++

Routine Description:

    This routine atomically ORs the given amount to a 64-bit variable.

Arguments:

    Address - Supplies the address of the value to atomically OR with.

    Mask - Supplies the bitmask to logically OR in to the value.

Return Value:

    Returns the value before the atomic operation was performed.

--*/

FUNCTION RtlAtomicOr64
    stmdb   %sp!, {%r4-%r7}                     @ Save nonvolatiles.
    ldrd    %r4, %r5, [%r0]                     @ Get *Address exclusive.
    orr     %r6, %r4, %r2                       @ OR low word.
    orr     %r7, %r5, %r3                       @ OR high word.
    strd    %r6, %r7, [%r0]                     @ Store the result.
    mov     %r0, %r4                            @ Move original low part to R0.
    mov     %r1, %r5                            @ Move original high part to R1.
    ldmia   %sp!, {%r4-%r7}                     @ Restore nonvolatiles.
    bx      %lr                                 @ Return.

END_FUNCTION RtlAtomicOr64

//
// ULONG
// RtlAtomicAnd32 (
//     volatile ULONG *Address,
//     ULONG Mask
//     )
//

/*++

Routine Description:

    This routine atomically ANDs the given mask to a 32-bit variable.

Arguments:

    Address - Supplies the address of the value to atomically AND with.

    Mask - Supplies the bitmask to logically AND in to the value.

Return Value:

    Returns the value before the atomic operation was performed.

--*/

FUNCTION RtlAtomicAnd32
    ldr     %r2, [%r0]                          @ Get *Address.
    and     %r3, %r2, %r1                       @ AND mask.
    str     %r3, [%r0]                          @ Store the result.
    mov     %r0, %r2                            @ Return original value.
    bx      %lr                                 @ Return.

END_FUNCTION RtlAtomicAnd32

//
// ULONG
// RtlAtomicXor32 (
//     volatile ULONG *Address,
//     ULONG Mask
//     )
//

/*++

Routine Description:

    This routine atomically exclusive ORs the given mask to a 32-bit variable.

Arguments:

    Address - Supplies the address of the value to atomically XOR with.

    Mask - Supplies the bitmask to logically XOR in to the value.

Return Value:

    Returns the value before the atomic operation was performed.

--*/

FUNCTION RtlAtomicXor32
    ldr     %r2, [%r0]                          @ Get *Address.
    eor     %r3, %r2, %r1                       @ XOR mask.
    str     %r3, [%r0]                          @ Store the result.
    mov     %r0, %r2                            @ Return original value.
    bx      %lr                                 @ Return.

END_FUNCTION RtlAtomicXor32

//
// --------------------------------------------------------- Internal Functions
//

