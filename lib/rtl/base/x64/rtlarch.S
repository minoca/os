/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    rtlarch.S

Abstract:

    This module implements x64 processor architecture features not
    implementable in C.

Author:

    Evan Green 17-Jan-2015

Environment:

    Any

--*/

##
## ------------------------------------------------------------------ Includes
##

#include <minoca/kernel/x64.inc>

##
## ---------------------------------------------------------------------- Code
##

ASSEMBLY_FILE_HEADER

##
## VOID
## RtlDebugBreak (
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

PROTECTED_FUNCTION(RtlDebugBreak)
    int $3                          # Debugger break.
    ret

END_FUNCTION(RtlDebugBreak)

##
## VOID
## RtlDebugService (
##     UINTN ServiceRequest,
##     PVOID Parameter
##     )
##

/*++

Routine Description:

    This routine enters the debugger for a service request.

Arguments:

    ServiceRequest - Supplies the reason for entering the debugger.

    Parameter - Supplies the parameter to pass to the debug service routine.

Return Value:

    None.

--*/

PROTECTED_FUNCTION(RtlDebugService)
    int     $0x21                   # Request debug service.
    ret                             #

END_FUNCTION(RtlDebugService)

##
## ULONGLONG
## RtlAtomicCompareExchange64 (
##     volatile ULONGLONG *Address,
##     ULONGLONG ExchangeValue,
##     ULONGLONG CompareValue
##     )
##

/*++

Routine Description:

    This routine atomically compares a 64-bit value at the given address with a
    value and exchanges it with another value if they are equal.

Arguments:

    Address - Supplies the address of the value to compare and potentially
        exchange.

    ExchangeValue - Supplies the value to write to Address if the comparison
        returns equality.

    CompareValue - Supplies the value to compare against.

Return Value:

    Returns the original value at the given address.

--*/

PROTECTED_FUNCTION(RtlAtomicCompareExchange64)
    movq    %rdx, %rax              # Move CompareValue into rax.

    ##
    ## Compare Address (rdi) with rax, exchange with rcx if equal. Return the
    ## original value in rax.
    ##

    lock cmpxchgq %rsi, (%rdi)      # Compare exchange.
    ret                             # Return value is already in the right spot.

END_FUNCTION(RtlAtomicCompareExchange64)

##
## ULONG
## RtlAtomicExchange32 (
##     volatile ULONG *Address,
##     ULONG ExchangeValue
##     )
##

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

PROTECTED_FUNCTION(RtlAtomicExchange32)
    xorq    %rax, %rax              # Clear the high part of rax for safety.
    movl    %esi, %eax              # Move the exchange value to eax.
    lock xchgl %eax, (%rdi)         # Exchange what's in *edi with eax.
    ret                             # Return. The old value is now in eax.

END_FUNCTION(RtlAtomicExchange32)

##
## ULONGLONG
## RtlAtomicExchange64 (
##     volatile ULONGLONG *Address,
##     ULONGLONG ExchangeValue
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

PROTECTED_FUNCTION(RtlAtomicExchange64)
    movq    %rsi, %rax              # Move exchange value to eax.
    lock xchgq %rax, (%rdi)         # Exchange what's in *rdi with rax.
    ret                             # Return. Result is already in rax.

END_FUNCTION(RtlAtomicExchange64)

##
## ULONG
## RtlAtomicCompareExchange32 (
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

PROTECTED_FUNCTION(RtlAtomicCompareExchange32)
    xorq    %rax, %rax              # Zero out the high part of rax, for safety.
    movl    %edx, %eax              # Move CompareValue into eax.

    ##
    ## Compare Address (rdi) with eax, exchange with esi if equal. Return the
    ## original value in eax.
    ##

    lock cmpxchgl %esi, (%rdi)      # Compare exchange.
    ret                             # Return value is already in the right spot.

END_FUNCTION(RtlAtomicCompareExchange32)

##
## ULONGLONG
## RtlAtomicAdd64 (
##     volatile ULONGLONG *Address,
##     ULONGLONG Increment
##     )
##

/*++

Routine Description:

    This routine atomically adds the given amount to a 64-bit variable.

Arguments:

    Address - Supplies the address of the value to atomically add to.

    Increment - Supplies the amount to add.

Return Value:

    Returns the value before the atomic addition was performed.

--*/

PROTECTED_FUNCTION(RtlAtomicAdd64)
RtlAtomicAdd64Loop:
    movq    (%rdi), %rcx            # Read the value.
    movq    %rcx, %rax              # Move the value to rax as compare value.
    addq    %rsi, %rcx              # Add the increment.
    lock cmpxchgq %rcx, (%rdi)      # Compare *rdi to rax, exchange rcx if ok.
    jne     RtlAtomicAdd64Loop      # Try again if the compare failed.
    ret                             # Return original value in rax.

END_FUNCTION(RtlAtomicAdd64)

##
## ULONG
## RtlAtomicAdd32 (
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

PROTECTED_FUNCTION(RtlAtomicAdd32)
RtlAtomicAdd32Loop:
    xorq    %rcx, %rcx              # Zero out high part of rcx.
    movl    (%rdi), %ecx            # Read the value.
    movl    %ecx, %eax              # move to eax as the compare value.
    addl    %esi, %ecx              # Add the increment.
    lock cmpxchgl %ecx, (%rdi)      # Compare *rdi to eax, exchange ecx if ok.
    jne     RtlAtomicAdd32Loop      # Try again if the compare failed.
    ret                             # Original *Address is in eax.

END_FUNCTION(RtlAtomicAdd32)

##
## ULONG
## RtlAtomicOr32 (
##     volatile ULONG *Address,
##     ULONG Mask
##     )
##

/*++

Routine Description:

    This routine atomically ORs the given mask to a 32-bit variable.

Arguments:

    Address - Supplies the address of the value to atomically OR with.

    Mask - Supplies the bitmask to logically OR in to the value.

Return Value:

    Returns the value before the atomic operation was performed.

--*/

PROTECTED_FUNCTION(RtlAtomicOr32)
RtlAtomicOr32Loop:
    xorq    %rcx, %rcx              # Zero out high part of rcx.
    movl    (%rdi), %ecx            # Read the value.
    movl    %ecx, %eax              # move to eax as the compare value.
    orl     %esi, %ecx              # OR in the mask.
    lock cmpxchgl %ecx, (%rdi)      # Compare *rdi to eax, exchange ecx if ok.
    jne     RtlAtomicOr32Loop      # Try again if the compare failed.
    ret                             # Original *Address is in eax.

END_FUNCTION(RtlAtomicOr32)

##
## ULONGLONG
## RtlAtomicOr64 (
##     volatile ULONGLONG *Address,
##     ULONGLONG Mask
##     )
##

/*++

Routine Description:

    This routine atomically ORs the given amount to a 64-bit variable.

Arguments:

    Address - Supplies the address of the value to atomically OR with.

    Mask - Supplies the bitmask to logically OR in to the value.

Return Value:

    Returns the value before the atomic operation was performed.

--*/

PROTECTED_FUNCTION(RtlAtomicOr64)
RtlAtomicOr64Loop:
    movq    (%rdi), %rcx            # Read the value.
    movq    %rcx, %rax              # Move the value to rax as compare value.
    orq     %rsi, %rcx              # Add the increment.
    lock cmpxchgq %rcx, (%rdi)      # Compare *rdi to rax, exchange rcx if ok.
    jne     RtlAtomicOr64Loop       # Try again if the compare failed.
    ret                             # Return original value in rax.

END_FUNCTION(RtlAtomicOr64)

##
## ULONG
## RtlAtomicAnd32 (
##     volatile ULONG *Address,
##     ULONG Mask
##     )
##

/*++

Routine Description:

    This routine atomically ANDs the given mask to a 32-bit variable.

Arguments:

    Address - Supplies the address of the value to atomically AND with.

    Mask - Supplies the bitmask to logically AND in to the value.

Return Value:

    Returns the value before the atomic operation was performed.

--*/

PROTECTED_FUNCTION(RtlAtomicAnd32)
RtlAtomicAnd32Loop:
    xorq    %rcx, %rcx              # Zero out high part of rcx.
    movl    (%rdi), %ecx            # Read the value.
    movl    %ecx, %eax              # move to eax as the compare value.
    andl    %esi, %ecx              # AND in the mask.
    lock cmpxchgl %ecx, (%rdi)      # Compare *rdi to eax, exchange ecx if ok.
    jne     RtlAtomicAnd32Loop      # Try again if the compare failed.
    ret                             # Original *Address is in eax.

END_FUNCTION(RtlAtomicAnd32)

##
## ULONG
## RtlAtomicXor32 (
##     volatile ULONG *Address,
##     ULONG Mask
##     )
##

/*++

Routine Description:

    This routine atomically exclusive ORs the given mask to a 32-bit variable.

Arguments:

    Address - Supplies the address of the value to atomically XOR with.

    Mask - Supplies the bitmask to logically XOR in to the value.

Return Value:

    Returns the value before the atomic operation was performed.

--*/

PROTECTED_FUNCTION(RtlAtomicXor32)
RtlAtomicXor32Loop:
    xorq    %rcx, %rcx              # Zero out high part of rcx.
    movl    (%rdi), %ecx            # Read the value.
    movl    %ecx, %eax              # move to eax as the compare value.
    xorl    %esi, %ecx              # XOR in the mask.
    lock cmpxchgl %ecx, (%rdi)      # Compare *rdi to eax, exchange ecx if ok.
    jne     RtlAtomicXor32Loop      # Try again if the compare failed.
    ret                             # Original *Address is in eax.

END_FUNCTION(RtlAtomicXor32)

##
## VOID
## RtlMemoryBarrier (
##     VOID
##     )
##

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

PROTECTED_FUNCTION(RtlMemoryBarrier)
    lock orq $0, (%esp)
    ret

END_FUNCTION(RtlMemoryBarrier)

##
## --------------------------------------------------------- Internal Functions
##

