/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    rtlarch.S

Abstract:

    This module implements ARMv7 processor architecture features not
    implementable in C.

Author:

    Evan Green 25-Sep-2012

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
// RTL_API
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

PROTECTED_FUNCTION RtlAtomicExchange32
    DSB                                         @ Data synchronization barrier.
    ldrex   %r2, [%r0]                          @ Get *Address exclusive.
    strex   %r3, %r1, [%r0]                     @ Store new value exclusive.
    cmp     %r3, #0                             @ See if the write worked.
    bne     RtlAtomicExchange32                 @ Try again if it didn't.
    mov     %r0, %r2                            @ Move original value to R0.
    DSB                                         @ Another barrier.
    bx      %lr                                 @ Return R0.

END_FUNCTION RtlAtomicExchange32

//
// RTL_API
// ULONGLONG
// RtlAtomicExchange64 (
//     volatile ULONGLONG *Address,
//     ULONGLONG ExchangeValue
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

PROTECTED_FUNCTION RtlAtomicExchange64
    stmdb   %sp!, {%r4-%r5}                     @ Save nonvolatiles.

RtlAtomicExchange64Loop:
    DSB                                         @ Data synchronization barrier.
    ldrexd  %r4, %r5, [%r0]                     @ Get *Address exclusive.
    strexd  %r12, %r2, %r3, [%r0]               @ Store new value exclusive.
    cmp     %r12, #0                            @ See if the write worked.
    bne     RtlAtomicExchange64Loop             @ Try again if it didn't.
    mov     %r0, %r4                            @ Move original low part to R0.
    mov     %r1, %r5                            @ Move original high part to R1.
    ldmia   %sp!, {%r4-%r5}                     @ Restore nonvolatiles.
    DSB                                         @ Another barrier.
    bx      %lr                                 @ Return R0-R1.

END_FUNCTION RtlAtomicExchange64

//
// RTL_API
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

PROTECTED_FUNCTION RtlAtomicCompareExchange32
    DSB                                         @ Data synchronization barrier.
    ldrex   %r3, [%r0]                          @ Get *Address exclusive.
    cmp     %r3, %r2                            @ Compare to CompareValue
    bne     RtlAtomicCompareExchange32End1      @ If not equal, exit with clrex.

    //
    // If the values are equal, do an exclusive store of R1 to the address at
    // R0. Store whether or not the store succeeded in R12 (a volatile
    // register). A 0 in R12 indicates the store succeeded.
    //

    strex   %r12, %r1, [%r0]                    @ Store exclusive.
    cmp     %r12, #0                            @ Compare with 0.
    bne     RtlAtomicCompareExchange32          @ Try again if the store failed.
    b       RtlAtomicCompareExchange32End2      @ Exit without clrex.

RtlAtomicCompareExchange32End1:
    clrex

RtlAtomicCompareExchange32End2:
    mov     %r0, %r3                            @ Move loaded value to R0.
    DSB                                         @ Data synchronization barrier.
    bx      %lr                                 @ Return R0.

END_FUNCTION RtlAtomicCompareExchange32

//
// RTL_API
// ULONGLONG
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

PROTECTED_FUNCTION RtlAtomicCompareExchange64
    stmdb   %sp!, {%r4-%r7}                     @ Save nonvolatiles.
    ldrd    %r4, %r5, [%sp, #16]                @ Get the last argument.

RtlAtomicCompareExchange64Loop:
    DSB                                         @ Data synchronization barrier.
    ldrexd  %r6, %r7, [%r0]                     @ Get *Address exclusive.
    cmp     %r6, %r4                            @ Compare to CompareValue
    bne     RtlAtomicCompareExchange64End1      @ If not equal, exit with clrex.
    cmp     %r7, %r5                            @ Compare to CompareValue
    bne     RtlAtomicCompareExchange64End1      @ If not equal, exit with clrex.

    //
    // If the values are equal, do an exclusive store of R2:R3 to the address at
    // R0. Store whether or not the store succeeded in R12 (a volatile
    // register). A 0 in R12 indicates the store succeeded.
    //

    strexd  %r12, %r2, %r3, [%r0]               @ Store exclusive.
    cmp     %r12, #0                            @ Compare with 0.
    bne     RtlAtomicCompareExchange64Loop      @ Try again if the store failed.
    b       RtlAtomicCompareExchange64End2

RtlAtomicCompareExchange64End1:
    clrex

RtlAtomicCompareExchange64End2:
    mov     %r0, %r6                            @ Move original low part to R0.
    mov     %r1, %r7                            @ Move original high part to R1.
    ldmia   %sp!, {%r4-%r7}                     @ Restore nonvolatiles.
    DSB                                         @ Data synchronization barrier.
    bx      %lr                                 @ Return R0.

END_FUNCTION RtlAtomicCompareExchange64

//
// RTL_API
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

PROTECTED_FUNCTION RtlAtomicAdd32
    DSB                                         @ Data synchronization barrier.
    ldrex   %r2, [%r0]                          @ Get *Address exclusive.
    add     %r3, %r2, %r1                       @ Add increment.
    strex   %r12, %r3, [%r0]                    @ Exclusively store the result.
    cmp     %r12, #0                            @ R12 is 0 on success.
    bne     RtlAtomicAdd32                      @ Try again if store failed.
    mov     %r0, %r2                            @ Return original value.
    DSB                                         @ Data synchronization barrier.
    bx      %lr                                 @ Return.

END_FUNCTION RtlAtomicAdd32

//
// RTL_API
// ULONGLONG
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

PROTECTED_FUNCTION RtlAtomicAdd64
    stmdb   %sp!, {%r4-%r7}                     @ Save nonvolatiles.

RtlAtomicAdd64Loop:
    DSB                                         @ Data synchronization barrier.
    ldrexd  %r4, %r5, [%r0]                     @ Get *Address exclusive.
    adds    %r6, %r4, %r2                       @ Add low word, setting flags.
    adc     %r7, %r5, %r3                       @ Add high word, with carry.
    strexd  %r12, %r6, %r7, [%r0]               @ Exclusively store the result.
    cmp     %r12, #0                            @ R12 is 0 on success.
    bne     RtlAtomicAdd64Loop                  @ Try again if store failed.
    mov     %r0, %r4                            @ Move original low part to R0.
    mov     %r1, %r5                            @ Move original high part to R1.
    ldmia   %sp!, {%r4-%r7}                     @ Restore nonvolatiles.
    DSB                                         @ Data synchronization barrier.
    bx      %lr                                 @ Return.

END_FUNCTION RtlAtomicAdd64

//
// RTL_API
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

PROTECTED_FUNCTION RtlAtomicOr32
    DSB                                         @ Data synchronization barrier.
    ldrex   %r2, [%r0]                          @ Get *Address exclusive.
    orr     %r3, %r2, %r1                       @ OR in mask.
    strex   %r12, %r3, [%r0]                    @ Exclusively store the result.
    cmp     %r12, #0                            @ R12 is 0 on success.
    bne     RtlAtomicOr32                       @ Try again if store failed.
    mov     %r0, %r2                            @ Return original value.
    DSB                                         @ Data synchronization barrier.
    bx      %lr                                 @ Return.

END_FUNCTION RtlAtomicOr32

//
// RTL_API
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

PROTECTED_FUNCTION RtlAtomicOr64
    stmdb   %sp!, {%r4-%r7}                     @ Save nonvolatiles.

RtlAtomicOr64Loop:
    DSB                                         @ Data synchronization barrier.
    ldrexd  %r4, %r5, [%r0]                     @ Get *Address exclusive.
    orr     %r6, %r4, %r2                       @ OR low word.
    orr     %r7, %r5, %r3                       @ OR high word.
    strexd  %r12, %r6, %r7, [%r0]               @ Exclusively store the result.
    cmp     %r12, #0                            @ R12 is 0 on success.
    bne     RtlAtomicOr64Loop                   @ Try again if store failed.
    mov     %r0, %r4                            @ Move original low part to R0.
    mov     %r1, %r5                            @ Move original high part to R1.
    ldmia   %sp!, {%r4-%r7}                     @ Restore nonvolatiles.
    DSB                                         @ Data synchronization barrier.
    bx      %lr                                 @ Return.

END_FUNCTION RtlAtomicOr64

//
// RTL_API
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

PROTECTED_FUNCTION RtlAtomicAnd32
    DSB                                         @ Data synchronization barrier.
    ldrex   %r2, [%r0]                          @ Get *Address exclusive.
    and     %r3, %r2, %r1                       @ AND mask.
    strex   %r12, %r3, [%r0]                    @ Exclusively store the result.
    cmp     %r12, #0                            @ R12 is 0 on success.
    bne     RtlAtomicAnd32                      @ Try again if store failed.
    mov     %r0, %r2                            @ Return original value.
    DSB                                         @ Data synchronization barrier.
    bx      %lr                                 @ Return.

END_FUNCTION RtlAtomicAnd32

//
// RTL_API
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

PROTECTED_FUNCTION RtlAtomicXor32
    DSB                                         @ Data synchronization barrier.
    ldrex   %r2, [%r0]                          @ Get *Address exclusive.
    eor     %r3, %r2, %r1                       @ XOR mask.
    strex   %r12, %r3, [%r0]                    @ Exclusively store the result.
    cmp     %r12, #0                            @ R12 is 0 on success.
    bne     RtlAtomicXor32                      @ Try again if store failed.
    mov     %r0, %r2                            @ Return original value.
    DSB                                         @ Data synchronization barrier.
    bx      %lr                                 @ Return.

END_FUNCTION RtlAtomicXor32

//
// RTL_API
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

PROTECTED_FUNCTION RtlMemoryBarrier
    DMB
    bx      %lr

END_FUNCTION RtlMemoryBarrier

//
// --------------------------------------------------------- Internal Functions
//

