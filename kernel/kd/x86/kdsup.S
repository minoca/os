/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    kdsup.S

Abstract:

    This module implements kernel debugger support routines on the x86
    architecture.

Author:

    Evan Green 21-Sep-2012

Environment:

    Kernel mode

--*/

//
// ------------------------------------------------------------------ Includes
//

#include <minoca/kernel/x86.inc>

//
// --------------------------------------------------------------- Definitions
//


//
// ---------------------------------------------------------------------- Code
//

//
// .text specifies that this code belongs in the executable section.
//
// .code32 specifies that this is 32-bit protected mode code.
//

.text
.code32

//
// VOID
// KdpBreak (
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

FUNCTION(KdpBreak)
    int $3                          # Debugger break.
    ret

END_FUNCTION(KdpBreak)

//
// VOID
// KdpInitializeDebugRegisters (
//     VOID
//     )
//

/*++

Routine Description:

    This routine is called during debugger initialization. It resets all
    hardware debug registers.

Arguments:

    None.

Return Value:

    None.

--*/

FUNCTION(KdpInitializeDebugRegisters)
    movl    $0, %eax                #
    movl    %eax, %dr7              # Turn all hardware breakpoints off.
    movl    %eax, %dr0              # Set 0 to the address of all debug
    movl    %eax, %dr1              # registers.
    movl    %eax, %dr2              #
    movl    %eax, %dr3              #
    movl    %eax, %dr6              # Reset the status register.
    ret                             #

END_FUNCTION(KdpInitializeDebugRegisters)

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

FUNCTION(KdpAtomicCompareExchange32)
    movl    12(%esp), %eax          # Move CompareValue into eax.
    movl    8(%esp), %ecx           # Move ExchangeValue into ecx.
    movl    4(%esp), %edx           # Move Address into edx.
    lock cmpxchgl %ecx, (%edx)      # Compare Address to eax, exchange with ecx.
    ret                             # Return. Result is already in eax.

END_FUNCTION(KdpAtomicCompareExchange32)

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

FUNCTION(KdpAtomicAdd32)
    movl    4(%esp), %edx           # Move Address into edx.
    movl    8(%esp), %ecx           # Move Increment into ecx.
    pushl   %ebx                    # Save ebx.

KdpAtomicAdd32Loop:
    movl    (%edx), %eax            # Get value of Address.
    movl    %ecx, %ebx              # Move Increment to ebx.
    add     %eax, %ebx              # Add *Address + Increment, save to ebx.
    lock cmpxchgl %ebx, (%edx)      # Compare *Address to original, save in ebx.
    jne     KdpAtomicAdd32Loop      # Try again if the value changed.
    popl    %ebx                    # Restore ebx and return.
    ret                             # Original *Address is still in eax.

END_FUNCTION(KdpAtomicAdd32)

//
// VOID
// KdpDisableInterrupts (
//     VOID
//     )
//

/*++

Routine Description:

    This routine disables all interrupts on the current processor.

Arguments:

    None.

Return Value:

    None.

--*/

FUNCTION(KdpDisableInterrupts)
    cli                             # Clear the interrupt flag.
    ret

END_FUNCTION(KdpDisableInterrupts)

