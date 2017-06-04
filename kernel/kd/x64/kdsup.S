/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    kdsup.S

Abstract:

    This module implements kernel debugger support routines on the AMD64
    architecture.

Author:

    Evan Green 2-Jun-2017

Environment:

    Kernel mode

--*/

##
## ------------------------------------------------------------------ Includes
##

#include <minoca/kernel/x64.inc>

##
## --------------------------------------------------------------- Definitions
##


##
## ---------------------------------------------------------------------- Code
##

ASSEMBLY_FILE_HEADER

##
## VOID
## KdpBreak (
##     VOID
##     )
##

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

##
## VOID
## KdpInitializeDebugRegisters (
##     VOID
##     )
##

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
    xorq    %rax, %rax
    movq    %rax, %dr7              # Turn all hardware breakpoints off.
    movq    %rax, %dr0              # Set 0 to the address of all debug
    movq    %rax, %dr1              # registers.
    movq    %rax, %dr2              #
    movq    %rax, %dr3              #
    movq    %rax, %dr6              # Reset the status register.
    ret                             #

END_FUNCTION(KdpInitializeDebugRegisters)

##
## ULONG
## KdpAtomicCompareExchange32 (
##     volatile ULONG *Address,
##     ULONG ExchangeValue,
##     ULONG CompareValue
##     )
##

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
    xorq    %rax, %rax              # Zero out the high part of rax, for safety.
    movl    %edx, %eax              # Move CompareValue into eax.

    ##
    ## Compare Address (rdi) with eax, exchange with esi if equal. Return the
    ## original value in eax.
    ##

    lock cmpxchgl %esi, (%rdi)      # Compare exchange.
    ret                             # Return value is already in the right spot.

END_FUNCTION(KdpAtomicCompareExchange32)

##
## ULONG
## KdpAtomicAdd32 (
##     volatile ULONG *Address,
##     ULONG Increment
##     )
##

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
KdpAtomicAdd32Loop:
    xorq    %rcx, %rcx              # Zero out high part of rcx.
    movl    (%rdi), %ecx            # Read the value.
    movl    %ecx, %eax              # move to eax as the compare value.
    addl    %esi, %ecx              # Add the increment.
    lock cmpxchgl %ecx, (%rdi)      # Compare *rdi to eax, exchange ecx if ok.
    jne     KdpAtomicAdd32Loop      # Try again if the compare failed.
    ret                             # Original *Address is in eax.

END_FUNCTION(KdpAtomicAdd32)

##
## VOID
## KdpDisableInterrupts (
##     VOID
##     )
##

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

