/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    rtlarch.S

Abstract:

    This module implements x86 processor architecture features not
    implementable in C.

Author:

    Evan Green 25-Sep-2012

Environment:

    Any

--*/

//
// ------------------------------------------------------------------ Includes
//

#include <minoca/kernel/x86.inc>

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

PROTECTED_FUNCTION(RtlDebugBreak)
    int $3                          # Debugger break.
    ret

END_FUNCTION(RtlDebugBreak)

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

PROTECTED_FUNCTION(RtlDebugService)
    mov     4(%esp), %eax           # Set up service request parameter.
    mov     8(%esp), %ecx           # Set up parameter.
    int     $0x21                   # Request debug service.
    ret                             #

END_FUNCTION(RtlDebugService)

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
    pushl   %ebx                    # Save ebx.
    pushl   %esi                    # Save esi.
    movl    28(%esp), %edx          # Move the CompareValue high part into edx.
    movl    24(%esp), %eax          # Move the CompareValue low part into eax.
    movl    20(%esp), %ecx          # Move the Exchange value high part to ecx.
    movl    16(%esp), %ebx          # Move the Exchange value low part to ebx.
    movl    12(%esp), %esi          # Load the address into esi.
    lock cmpxchg8b (%esi)           # Do the atomic compare exchange.
    popl    %esi                    # Restore esi.
    popl    %ebx                    # Restore ebx.
    ret                             # Return value is already in the right spot.

END_FUNCTION(RtlAtomicCompareExchange64)

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

PROTECTED_FUNCTION(RtlAtomicExchange32)
    movl    8(%esp), %eax           # Move ExchangeValue into eax.
    movl    4(%esp), %edx           # Move Address into edx.
    lock xchgl %eax, (%edx)         # Exchange eax with *edx.
    ret                             # Return. The old value is now in eax.

END_FUNCTION(RtlAtomicExchange32)

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
    pushl   %ebx                    # Save ebx.
    pushl   %esi                    # Save esi.
    movl    20(%esp), %ecx          # Move ExchangeValue high into ecx.
    movl    16(%esp), %ebx          # Move ExchangeValue low into ebx.
    movl    12(%esp), %esi          # Move Address into esi.

RtlAtomicExchange64Loop:
    movl    (%esi), %eax            # Read value low into eax.
    movl    4(%esi), %edx           # Read value high into edx.
    lock cmpxchg8b (%esi)           # Compare exchange (since no xchg8b).
    jnz     RtlAtomicExchange64Loop # Try again if it failed.
    popl    %esi                    # Restore esi.
    popl    %ebx                    # Restore ebx.
    ret                             # Return. Result is already in eax.

END_FUNCTION(RtlAtomicExchange64)

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

PROTECTED_FUNCTION(RtlAtomicCompareExchange32)
    movl    12(%esp), %eax          # Move CompareValue into eax.
    movl    8(%esp), %ecx           # Move ExchangeValue into ecx.
    movl    4(%esp), %edx           # Move Address into edx.
    lock cmpxchgl %ecx, (%edx)      # Compare Address to eax, exchange with ecx.
    ret                             # Return. Result is already in eax.

END_FUNCTION(RtlAtomicCompareExchange32)

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

PROTECTED_FUNCTION(RtlAtomicAdd64)
    pushl   %ebx                    # Save ebx.
    pushl   %esi                    # Save esi.
    movl    12(%esp), %esi          # Load the address into esi.

RtlAtomicAdd64Loop:
    movl    (%esi), %eax            # Get the low part of the value at Address.
    movl    4(%esi), %edx           # Get the high part of the value at Address.
    movl    16(%esp), %ebx          # Move the Increment value low part to ebx.
    movl    20(%esp), %ecx          # Move the Increment value high part to ecx.
    add     %eax, %ebx              # Add *Address + Increment low part to ebx.
    adc     %edx, %ecx              # Add *Address + Increment high part to ecx.
    lock cmpxchg8b (%esi)           # Compare *Address to original value.
    jne     RtlAtomicAdd64Loop      # Try again if the value changed.
    popl    %esi                    # Restore esi.
    popl    %ebx                    # Restore ebx.
    ret                             # Original *Address is still in edx:eax.

END_FUNCTION(RtlAtomicAdd64)

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

PROTECTED_FUNCTION(RtlAtomicAdd32)
    movl    4(%esp), %edx           # Move Address into edx.
    movl    8(%esp), %ecx           # Move Increment into ecx.
    pushl   %ebx                    # Save ebx.

RtlAtomicAdd32Loop:
    movl    (%edx), %eax            # Get value of Address.
    movl    %ecx, %ebx              # Move Increment to ebx.
    add     %eax, %ebx              # Add *Address + Increment, save to ebx.
    lock cmpxchgl %ebx, (%edx)      # Compare *Address to original, save in ebx.
    jne     RtlAtomicAdd32Loop      # Try again if the value changed.
    popl    %ebx                    # Restore ebx and return.
    ret                             # Original *Address is still in eax.

END_FUNCTION(RtlAtomicAdd32)

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

PROTECTED_FUNCTION(RtlAtomicOr32)
    movl    4(%esp), %edx           # Move Address into edx.
    movl    8(%esp), %ecx           # Move Increment into ecx.
    pushl   %ebx                    # Save ebx.

RtlAtomicOr32Loop:
    movl    (%edx), %eax            # Get value of Address.
    movl    %ecx, %ebx              # Move Mask to ebx.
    orl     %eax, %ebx              # OR *Address | Mask, save to ebx.
    lock cmpxchgl %ebx, (%edx)      # Compare *Address to original, save in ebx.
    jne     RtlAtomicOr32Loop       # Try again if the value changed.
    popl    %ebx                    # Restore ebx and return.
    ret                             # Original *Address is still in eax.

END_FUNCTION(RtlAtomicOr32)

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

PROTECTED_FUNCTION(RtlAtomicOr64)
    pushl   %ebx                    # Save ebx.
    pushl   %esi                    # Save esi.
    movl    12(%esp), %esi          # Load the address into esi.

RtlAtomicOr64Loop:
    movl    (%esi), %eax            # Get the low part of the value at Address.
    movl    4(%esi), %edx           # Get the high part of the value at Address.
    movl    16(%esp), %ebx          # Move the Increment value low part to ebx.
    movl    20(%esp), %ecx          # Move the Increment value high part to ecx.
    orl     %eax, %ebx              # Add *Address | Increment low part to ebx.
    orl     %edx, %ecx              # Add *Address | Increment high part to ecx.
    lock cmpxchg8b (%esi)           # Compare *Address to original value.
    jne     RtlAtomicOr64Loop       # Try again if the value changed.
    popl    %esi                    # Restore esi.
    popl    %ebx                    # Restore ebx.
    ret                             # Original *Address is still in edx:eax.

END_FUNCTION(RtlAtomicOr64)

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

PROTECTED_FUNCTION(RtlAtomicAnd32)
    movl    4(%esp), %edx           # Move Address into edx.
    movl    8(%esp), %ecx           # Move Increment into ecx.
    pushl   %ebx                    # Save ebx.

RtlAtomicAnd32Loop:
    movl    (%edx), %eax            # Get value of Address.
    movl    %ecx, %ebx              # Move Mask to ebx.
    andl    %eax, %ebx              # AND *Address & Mask, save to ebx.
    lock cmpxchgl %ebx, (%edx)      # Compare *Address to original, save in ebx.
    jne     RtlAtomicAnd32Loop      # Try again if the value changed.
    popl    %ebx                    # Restore ebx and return.
    ret                             # Original *Address is still in eax.

END_FUNCTION(RtlAtomicAnd32)

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

    Address - Supplies the address of the value to atomically AND with.

    Mask - Supplies the bitmask to logically XOR in to the value.

Return Value:

    Returns the value before the atomic operation was performed.

--*/

PROTECTED_FUNCTION(RtlAtomicXor32)
    movl    4(%esp), %edx           # Move Address into edx.
    movl    8(%esp), %ecx           # Move Increment into ecx.
    pushl   %ebx                    # Save ebx.

RtlAtomicXor32Loop:
    movl    (%edx), %eax            # Get value of Address.
    movl    %ecx, %ebx              # Move Mask to ebx.
    xorl    %eax, %ebx              # XOR *Address ^ Mask, save to ebx.
    lock cmpxchgl %ebx, (%edx)      # Compare *Address to original, save in ebx.
    jne     RtlAtomicXor32Loop      # Try again if the value changed.
    popl    %ebx                    # Restore ebx and return.
    ret                             # Original *Address is still in eax.

END_FUNCTION(RtlAtomicXor32)

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

PROTECTED_FUNCTION(RtlMemoryBarrier)
    lock orl $0, (%esp)
    ret

END_FUNCTION(RtlMemoryBarrier)

//
// --------------------------------------------------------- Internal Functions
//

