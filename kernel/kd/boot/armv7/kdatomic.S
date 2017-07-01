/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    kdatomic.s

Abstract:

    This module implements ARM specific atomic routines for the Kernel
    Debugging subsystem in the boot environment.

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
    ldr     %r3, [%r0]                          @ Get *Address.
    cmp     %r3, %r2                            @ Compare to CompareValue
    bne     KdpAtomicCompareExchange32End       @ If not equal, exit.
    str     %r1, [%r0]                          @ Store the exchange value.

KdpAtomicCompareExchange32End:
    mov     %r0, %r3                            @ Move loaded value to R0.
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
    ldr     %r2, [%r0]                          @ Get *Address.
    add     %r3, %r2, %r1                       @ Add increment.
    str     %r3, [%r0]                          @ Store the result.
    mov     %r0, %r2                            @ Return original value.
    bx      %lr                                 @ Return.

END_FUNCTION KdpAtomicAdd32

//
// --------------------------------------------------------- Internal Functions
//

