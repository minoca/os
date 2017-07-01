/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    kdatomic.s

Abstract:

    This module implements ARMv6 specific atomic routines for the Kernel
    Debugging subsystem in the kernel environment.

Author:

    Chris Stevens 18-Mar-2014

Environment:

    Kernel mode

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/arm.inc>

//
// ---------------------------------------------------------------- Definitions
//

//
// ----------------------------------------------------------------------- Code
//

ASSEMBLY_FILE_HEADER

//
// ULONG
// KdpAtomicCompareExchange32 (
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

FUNCTION KdpAtomicCompareExchange32
    DSB                                         @ Data synchronization barrier.
    ldrex   %r3, [%r0]                          @ Get *Address exclusive.
    cmp     %r3, %r2                            @ Compare to CompareValue
    bne     KdpAtomicCompareExchange32End1      @ If not equal, exit with clrex.

    //
    // If the values are equal, do an exclusive store of R1 to the address at
    // R0. Store whether or not the store succeeded in R12 (a volatile
    // register). A 0 in R12 indicates the store succeeded.
    //

    strex   %r12, %r1, [%r0]                    @ Store exclusive.
    cmp     %r12, #0                            @ Compare with 0.
    bne     KdpAtomicCompareExchange32          @ Try again if the store failed.
    b       KdpAtomicCompareExchange32End2      @ Branch to exit without clrex.

KdpAtomicCompareExchange32End1:
    clrex

KdpAtomicCompareExchange32End2:
    mov     %r0, %r3                            @ Move loaded value to R0.
    DSB                                         @ Data synchronization barrier.
    bx      %lr                                 @ Return R0.

END_FUNCTION KdpAtomicCompareExchange32

//
// ULONG
// KdpAtomicAdd32 (
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

FUNCTION KdpAtomicAdd32
    DSB                                         @ Data synchronization barrier.
    ldrex   %r2, [%r0]                          @ Get *Address exclusive.
    add     %r3, %r2, %r1                       @ Add increment.
    strex   %r12, %r3, [%r0]                    @ Exclusively store the result.
    cmp     %r12, #0                            @ R12 is 0 on success.
    bne     KdpAtomicAdd32                      @ Try again if store failed.
    mov     %r0, %r2                            @ Return original value.
    DSB                                         @ Data synchronization barrier.
    bx      %lr                                 @ Return.

END_FUNCTION KdpAtomicAdd32

//
// --------------------------------------------------------- Internal Functions
//

