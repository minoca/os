/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    rtlmem.S

Abstract:

    This module contains memory routines in assembly, for speed.

Author:

    Evan Green 15-Aug-2014

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
// PVOID
// RtlCopyMemory (
//     PVOID Destination,
//     PCVOID Source,
//     UINTN ByteCount
//     )
//

/*++

Routine Description:

    This routine copies a section of memory.

Arguments:

    Destination - Supplies a pointer to the buffer where the memory will be
        copied to.

    Source - Supplies a pointer to the buffer to be copied.

    ByteCount - Supplies the number of bytes to copy.

Return Value:

    Returns the destination pointer.

--*/

PROTECTED_FUNCTION(RtlCopyMemory)
    push    %ebp                    # Save the frame register.
    movl    %esp, %ebp              # Make the current stack the new frame.
    pushl   %esi                    # Save registers.
    pushl   %edi                    # Save more registers.
    movl    8(%ebp), %edi           # Load the destination address.
    movl    12(%ebp), %esi          # Load the source address.
    movl    16(%ebp), %ecx          # Load the count.
    cld                             # Clear the direction flag.
    rep movsb                       # Copy bytes like a crazy person.
    movl    8(%ebp), %eax           # Load the destination to the return value.
    popl    %edi                    # Restore edi.
    popl    %esi                    # Restore esi.
    popl    %ebp                    # Restore frame.
    ret                             # Return.

END_FUNCTION(RtlCopyMemory)

//
// RTL_API
// VOID
// RtlZeroMemory (
//     PVOID Buffer,
//     UINTN ByteCount
//     )
//

/*++

Routine Description:

    This routine zeroes out a section of memory.

Arguments:

    Buffer - Supplies a pointer to the buffer to clear.

    ByteCount - Supplies the number of bytes to zero out.

Return Value:

    None.

--*/

PROTECTED_FUNCTION(RtlZeroMemory)
    push    %ebp                    # Save the frame register.
    movl    %esp, %ebp              # Make the current stack the new frame.
    pushl   %edi                    # Save a register.
    movl    8(%ebp), %edi           # Load the buffer address.
    movl    12(%ebp), %ecx          # Load the count.
    xorl    %eax, %eax              # Zero out eax.
    cld                             # Clear the direction flag.
    rep stosb                       # Zero bytes like there's no tomorrow.
    popl    %edi                    # Restore edi.
    popl    %ebp                    # Restore frame.
    ret                             # Return.

END_FUNCTION(RtlZeroMemory)

//
// RTL_API
// VOID
// RtlSetMemory (
//     PVOID Buffer,
//     INT Byte,
//     UINTN Count
//     )
//

/*++

Routine Description:

    This routine writes the given byte value repeatedly into a region of memory.

Arguments:

    Buffer - Supplies a pointer to the buffer to set.

    Byte - Supplies the byte to set.

    Count - Supplies the number of bytes to set.

Return Value:

    None.

--*/

PROTECTED_FUNCTION(RtlSetMemory)
    push    %ebp                    # Save the frame register.
    movl    %esp, %ebp              # Make the current stack the new frame.
    pushl   %edi                    # Save a register.
    movl    8(%ebp), %edi           # Load the buffer address.
    movl    12(%ebp), %eax          # Load the byte to set.
    movl    16(%ebp), %ecx          # Load the count.
    cld                             # Clear the direction flag.
    rep stosb                       # Set bytes like the wind.
    popl    %edi                    # Restore edi.
    popl    %ebp                    # Restore frame.
    ret                             # Return.

END_FUNCTION(RtlSetMemory)

//
// RTL_API
// BOOL
// RtlCompareMemory (
//     PCVOID FirstBuffer,
//     PCVOID SecondBuffer,
//     UINTN Size
//     )
//

/*++

Routine Description:

    This routine compares two buffers for equality.

Arguments:

    FirstBuffer - Supplies a pointer to the first buffer to compare.

    SecondBuffer - Supplies a pointer to the second buffer to compare.

    Size - Supplies the number of bytes to compare.

Return Value:

    TRUE if the buffers are equal.

    FALSE if the buffers are not equal.

--*/

PROTECTED_FUNCTION(RtlCompareMemory)
    push    %ebp                    # Save the frame register.
    movl    %esp, %ebp              # Make the current stack the new frame.
    pushl   %esi                    # Save registers.
    pushl   %edi                    # Save more registers.
    xorl    %eax, %eax              # Zero out the return value.
    movl    8(%ebp), %edi           # Load the destination address.
    movl    12(%ebp), %esi          # Load the source address.
    movl    16(%ebp), %ecx          # Load the count.
    cld                             # Clear the direction flag.
    repe cmpsb                      # Compare bytes on fire.
    setz    %al                     # Return TRUE if buffers are equal.
    popl    %edi                    # Restore edi.
    popl    %esi                    # Restore esi.
    popl    %ebp                    # Restore frame.
    ret                             # Return.

END_FUNCTION(RtlCompareMemory)

//
// --------------------------------------------------------- Internal Functions
//

