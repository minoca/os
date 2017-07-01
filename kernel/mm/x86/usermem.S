/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    usermem.S

Abstract:

    This module contains memory manipulation routines to and from user mode
    buffers. The page fault handler knows about these functions specifically
    and may manipulate the instruction pointer if it is found in one of these
    functions. These routines may fail if user mode passes a bad buffer.

Author:

    Evan Green 4-Nov-2014

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
// BOOL
// MmpCopyUserModeMemory (
//     PVOID Destination,
//     PCVOID Source,
//     ULONG ByteCount
//     )
//

/*++

Routine Description:

    This routine copies a section of memory to or from user mode.

Arguments:

    Destination - Supplies a pointer to the buffer where the memory will be
        copied to.

    Source - Supplies a pointer to the buffer to be copied.

    ByteCount - Supplies the number of bytes to copy.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

FUNCTION(MmpCopyUserModeMemory)
    push    %ebp                    # Save the frame register.
    movl    %esp, %ebp              # Make the current stack the new frame.
    pushl   %esi                    # Save registers.
    pushl   %edi                    # Save more registers.
    movl    8(%ebp), %edi           # Load the destination address.
    movl    12(%ebp), %esi          # Load the source address.
    movl    16(%ebp), %ecx          # Load the count.
    cld                             # Clear the direction flag.
    rep movsb                       # Copy bytes like a crazy person.
    movl    $1, %eax                # Return success.
    jmp     MmpUserModeMemoryReturn # Jump to epilog.

END_FUNCTION(MmpCopyUserModeMemory)

//
// BOOL
// MmpZeroUserModeMemory (
//     PVOID Buffer,
//     ULONG ByteCount
//     )
//

/*++

Routine Description:

    This routine zeroes out a section of user mode memory.

Arguments:

    Buffer - Supplies a pointer to the buffer to clear.

    ByteCount - Supplies the number of bytes to zero out.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

FUNCTION(MmpZeroUserModeMemory)
    push    %ebp                    # Save the frame register.
    movl    %esp, %ebp              # Make the current stack the new frame.
    pushl   %esi                    # Save registers.
    pushl   %edi                    # Save more registers.
    movl    8(%ebp), %edi           # Load the buffer address.
    movl    12(%ebp), %ecx          # Load the count.
    xorl    %eax, %eax              # Zero out eax.
    cld                             # Clear the direction flag.
    rep stosb                       # Zero bytes like there's no tomorrow.
    movl    $1, %eax                # Return success.
    jmp     MmpUserModeMemoryReturn # Jump to epilog.

END_FUNCTION(MmpZeroUserModeMemory)

//
// BOOL
// MmpSetUserModeMemory (
//     PVOID Buffer,
//     INT Byte,
//     UINTN Count
//     )
//

/*++

Routine Description:

    This routine writes the given byte value repeatedly into a region of
    user mode memory.

Arguments:

    Buffer - Supplies a pointer to the buffer to set.

    Byte - Supplies the byte to set.

    Count - Supplies the number of bytes to set.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

FUNCTION(MmpSetUserModeMemory)
    push    %ebp                    # Save the frame register.
    movl    %esp, %ebp              # Make the current stack the new frame.
    pushl   %esi                    # Save registers.
    pushl   %edi                    # Save more registers.
    movl    8(%ebp), %edi           # Load the buffer address.
    movl    12(%ebp), %eax          # Load the byte to set.
    movl    16(%ebp), %ecx          # Load the count.
    cld                             # Clear the direction flag.
    rep stosb                       # Set bytes like the wind.
    movl    $1, %eax                # Return success.
    jmp     MmpUserModeMemoryReturn # Jump to epilog.

END_FUNCTION(MmpSetUserModeMemory)

//
// BOOL
// MmpCompareUserModeMemory (
//     PVOID FirstBuffer,
//     PVOID SecondBuffer,
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

    FALSE if the buffers are not equal or on failure.

--*/

FUNCTION(MmpCompareUserModeMemory)
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
    jmp     MmpUserModeMemoryReturn # Jump to epilog.

END_FUNCTION(MmpCompareUserModeMemory)

//
// BOOL
// MmpTouchUserModeMemoryForRead (
//     PVOID Buffer,
//     UINTN Size
//     )
//

/*++

Routine Description:

    This routine touches each page of a user mode buffer to ensure it can be
    read from.

Arguments:

    Buffer - Supplies a pointer to the buffer to probe.

    Size - Supplies the number of bytes to compare.

Return Value:

    TRUE if the buffers are valid.

    FALSE if the buffers are not valid.

--*/

FUNCTION(MmpTouchUserModeMemoryForRead)
    push    %ebp                    # Save the frame register.
    movl    %esp, %ebp              # Make the current stack the new frame.
    pushl   %esi                    # Save registers.
    pushl   %edi                    # Save more registers.
    xorl    %eax, %eax              # Zero out the count.
    movl    8(%ebp), %esi           # Load the destination address.
    movl    12(%ebp), %ecx          # Load the count.
    movl    $1, %eax                # Assume success.

MmpTouchUserModeMemoryForReadLoop:
    movl    (%esi), %edx            # Do a dummy read.
    cmpl    $0x1000, %ecx           # Compare to a page.
    jg      MmpTouchUserModeMemoryForReadNextPage   # Do a full page.
    cmpl    $0, %ecx                # Compare to zero.
    je      MmpUserModeMemoryReturn # Jump out.
    addl    %ecx, %esi              # Add the remainder.
    subl    $1, %esi                # Subtract one for the last address.
    movl    (%esi), %edx            # Read the last byte.
    jmp     MmpUserModeMemoryReturn

MmpTouchUserModeMemoryForReadNextPage:
    subl    $0x1000, %ecx
    addl    $0x1000, %esi
    jmp MmpTouchUserModeMemoryForReadLoop

END_FUNCTION(MmpTouchUserModeMemoryForRead)

//
// BOOL
// MmpTouchUserModeMemoryForWrite (
//     PVOID Buffer,
//     UINTN Size
//     )
//

/*++

Routine Description:

    This routine touches each page of a user mode buffer to ensure it can be
    written to.

Arguments:

    Buffer - Supplies a pointer to the buffer to probe.

    Size - Supplies the number of bytes to compare.

Return Value:

    TRUE if the buffers are valid.

    FALSE if the buffers are not valid.

--*/

FUNCTION(MmpTouchUserModeMemoryForWrite)
    push    %ebp                    # Save the frame register.
    movl    %esp, %ebp              # Make the current stack the new frame.
    pushl   %esi                    # Save registers.
    pushl   %edi                    # Save more registers.
    xorl    %eax, %eax              # Zero out the count.
    movl    8(%ebp), %esi           # Load the destination address.
    movl    12(%ebp), %ecx          # Load the count.
    movl    $1, %eax                # Assume success.

MmpTouchUserModeMemoryForWriteLoop:
    orl     $0, (%esi)              # Do a dummy write.
    cmpl    $0x1000, %ecx           # Compare to a page.
    jg      MmpTouchUserModeMemoryForWriteNextPage   # Do a full page.
    cmpl    $0, %ecx                # Compare to zero.
    je      MmpUserModeMemoryReturn # Jump out.
    addl    %ecx, %esi              # Add the remainder.
    subl    $1, %esi                # Subtract one for the last address.
    orl     $0, (%esi)              # Write the last byte.
    jmp     MmpUserModeMemoryReturn

MmpTouchUserModeMemoryForWriteNextPage:
    subl    $0x1000, %ecx
    addl    $0x1000, %esi
    jmp MmpTouchUserModeMemoryForWriteLoop

END_FUNCTION(MmpTouchUserModeMemoryForWrite)

//
// BOOL
// MmUserRead8 (
//     PVOID Buffer,
//     PUCHAR Value
//     )
//

/*++

Routine Description:

    This routine performs a 8-bit read from user mode.

Arguments:

    Buffer - Supplies a pointer to the buffer to read.

    Value - Supplies a pointer where the read value will be returned.

Return Value:

    TRUE if the read succeeded.

    FALSE if the read failed.

--*/

FUNCTION(MmUserRead8)
    push    %ebp                    # Save the frame register.
    movl    %esp, %ebp              # Make the current stack the new frame.
    pushl   %esi                    # Save registers.
    pushl   %edi                    # Save more registers.
    movl    $1, %eax                # Assume success.
    movl    8(%ebp), %esi           # Load the target buffer address.
    movb    (%esi), %cl             # Get the value. This may fault.
    movl    12(%ebp), %edi          # Load the destination pointer.
    movb    %cl, (%edi)             # Move the value to the destination.
    jmp     MmpUserModeMemoryReturn

END_FUNCTION(MmUserRead8)

//
// BOOL
// MmUserWrite8 (
//     PVOID Buffer,
//     UCHAR Value
//     )
//

/*++

Routine Description:

    This routine performs a 8-bit write to user mode.

Arguments:

    Buffer - Supplies a pointer to the buffer to write to.

    Value - Supplies the value to write.

Return Value:

    TRUE if the write succeeded.

    FALSE if the write failed.

--*/

FUNCTION(MmUserWrite8)
    push    %ebp                    # Save the frame register.
    movl    %esp, %ebp              # Make the current stack the new frame.
    pushl   %esi                    # Save registers.
    pushl   %edi                    # Save more registers.
    movl    $1, %eax                # Assume success.
    movl    8(%ebp), %edi           # Load the user buffer address.
    movl    12(%ebp), %ecx          # Load the value to write.
    movb    %cl, (%edi)             # Write the value. This may fault.
    jmp     MmpUserModeMemoryReturn

END_FUNCTION(MmUserWrite8)

//
// BOOL
// MmUserRead16 (
//     PVOID Buffer,
//     PUSHORT Value
//     )
//

/*++

Routine Description:

    This routine performs a 16-bit read from user mode.

Arguments:

    Buffer - Supplies a pointer to the buffer to read.

    Value - Supplies a pointer where the read value will be returned.

Return Value:

    TRUE if the read succeeded.

    FALSE if the read failed.

--*/

FUNCTION(MmUserRead16)
    push    %ebp                    # Save the frame register.
    movl    %esp, %ebp              # Make the current stack the new frame.
    pushl   %esi                    # Save registers.
    pushl   %edi                    # Save more registers.
    movl    $1, %eax                # Assume success.
    movl    8(%ebp), %esi           # Load the target buffer address.
    movw    (%esi), %cx             # Get the value. This may fault.
    movl    12(%ebp), %edi          # Load the destination pointer.
    movw    %cx, (%edi)             # Move the value to the destination.
    jmp     MmpUserModeMemoryReturn

END_FUNCTION(MmUserRead16)

//
// BOOL
// MmUserWrite16 (
//     PVOID Buffer,
//     USHORT Value
//     )
//

/*++

Routine Description:

    This routine performs a 16-bit write to user mode.

Arguments:

    Buffer - Supplies a pointer to the buffer to write to.

    Value - Supplies the value to write.

Return Value:

    TRUE if the write succeeded.

    FALSE if the write failed.

--*/

FUNCTION(MmUserWrite16)
    push    %ebp                    # Save the frame register.
    movl    %esp, %ebp              # Make the current stack the new frame.
    pushl   %esi                    # Save registers.
    pushl   %edi                    # Save more registers.
    movl    $1, %eax                # Assume success.
    movl    8(%ebp), %edi           # Load the user buffer address.
    movl    12(%ebp), %ecx          # Load the value to write.
    movw    %cx, (%edi)             # Write the value. This may fault.
    jmp     MmpUserModeMemoryReturn

END_FUNCTION(MmUserWrite16)

//
// BOOL
// MmUserRead32 (
//     PVOID Buffer,
//     PULONG Value
//     )
//

/*++

Routine Description:

    This routine performs a 32-bit read from user mode. This is assumed to be
    naturally aligned.

Arguments:

    Buffer - Supplies a pointer to the buffer to read.

    Value - Supplies a pointer where the read value will be returned.

Return Value:

    TRUE if the read succeeded.

    FALSE if the read failed.

--*/

FUNCTION(MmUserRead32)
    push    %ebp                    # Save the frame register.
    movl    %esp, %ebp              # Make the current stack the new frame.
    pushl   %esi                    # Save registers.
    pushl   %edi                    # Save more registers.
    movl    $1, %eax                # Assume success.
    movl    8(%ebp), %esi           # Load the target buffer address.
    movl    (%esi), %ecx            # Get the value. This may fault.
    movl    12(%ebp), %edi          # Load the destination pointer.
    movl    %ecx, (%edi)            # Move the value to the destination.
    jmp     MmpUserModeMemoryReturn

END_FUNCTION(MmUserRead32)

//
// BOOL
// MmUserWrite32 (
//     PVOID Buffer,
//     ULONG Value
//     )
//

/*++

Routine Description:

    This routine performs a 32-bit write to user mode. This is assumed to be
    naturally aligned.

Arguments:

    Buffer - Supplies a pointer to the buffer to write to.

    Value - Supplies the value to write.

Return Value:

    TRUE if the write succeeded.

    FALSE if the write failed.

--*/

FUNCTION(MmUserWrite32)
    push    %ebp                    # Save the frame register.
    movl    %esp, %ebp              # Make the current stack the new frame.
    pushl   %esi                    # Save registers.
    pushl   %edi                    # Save more registers.
    movl    $1, %eax                # Assume success.
    movl    8(%ebp), %edi           # Load the user buffer address.
    movl    12(%ebp), %ecx          # Load the value to write.
    movl    %ecx, (%edi)            # Write the value. This may fault.
    jmp     MmpUserModeMemoryReturn

END_FUNCTION(MmUserWrite32)

//
// This common epilog is both jumped to by the memory routines directly, as
// well as routed to by the page fault code if it detects a fault in one of the
// user mode memory routines.
//

FUNCTION(MmpUserModeMemoryReturn)
    popl    %edi                    # Restore edi.
    popl    %esi                    # Restore esi.
    popl    %ebp                    # Restore frame.
    ret                             # Return.

END_FUNCTION(MmpUserModeMemoryReturn)

//
// --------------------------------------------------------- Internal Functions
//

