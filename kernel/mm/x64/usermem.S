/*++

Copyright (c) 2017 Minoca Corp.

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

    Evan Green 11-Jun-2017

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
## BOOL
## MmpCopyUserModeMemory (
##     PVOID Destination,
##     PCVOID Source,
##     ULONG ByteCount
##     )
##

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
    movq    %rdx, %rcx              # Move count into rcx.
    cld                             # Clear the direction flag.
    rep movsb                       # Copy bytes from *rsi to *rdi (params!)
    movl    $TRUE, %eax             # Return success.
    ret                             # Just return.

END_FUNCTION(MmpCopyUserModeMemory)

##
## BOOL
## MmpZeroUserModeMemory (
##     PVOID Buffer,
##     ULONG ByteCount
##     )
##

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
    movq    %rsi, %rcx              # Move count into rcx.
    xorl    %eax, %eax              # Clear out eax.
    cld                             # Clear the direction flag.
    rep stosb                       # Zero bytes in *rdi.
    movl    $TRUE, %eax             # Return success.
    ret                             # Return.

END_FUNCTION(MmpZeroUserModeMemory)

##
## BOOL
## MmpSetUserModeMemory (
##     PVOID Buffer,
##     INT Byte,
##     UINTN Count
##     )
##

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
    movq    %rdx, %rcx              # Move count into rcx.
    movl    %esi, %eax              # Move byte into eax.
    cld                             # Clear the direction flag.
    rep stosb                       # Set byte at eax into *rdi.
    movl    $TRUE, %eax             # Return success.
    ret                             # Return.

END_FUNCTION(MmpSetUserModeMemory)

##
## BOOL
## MmpCompareUserModeMemory (
##     PVOID FirstBuffer,
##     PVOID SecondBuffer,
##     UINTN Size
##     )
##

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
    movq    %rdx, %rcx              # Move count into ecx.
    cld                             # Clear the direction flag.
    repe cmpsb                      # Compare bytes at *rdi to *rsi.
    setz    %al                     # Return TRUE if buffers are equal.
    ret                             # Return.

END_FUNCTION(MmpCompareUserModeMemory)

##
## BOOL
## MmpTouchUserModeMemoryForRead (
##     PVOID Buffer,
##     UINTN Size
##     )
##

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
    movl    $TRUE, %eax             # Assume success.

MmpTouchUserModeMemoryForReadLoop:
    movl    (%rdi), %edx            # Do a dummy read.
    cmpq    $0x1000, %rsi           # Compare to a page.
    jg      MmpTouchUserModeMemoryForReadNextPage   # Do a full page.
    cmpq    $0, %rsi                # Compare to zero.
    je      MmpUserModeMemoryReturn # Jump out.
    addq    %rsi, %rdi              # Add the remainder.
    subq    $1, %rdi                # Subtract one for the last address.
    movl    (%rdi), %edx            # Read the last byte.
    jmp     MmpUserModeMemoryReturn

MmpTouchUserModeMemoryForReadNextPage:
    subq    $0x1000, %rsi
    addq    $0x1000, %rdi
    jmp MmpTouchUserModeMemoryForReadLoop

END_FUNCTION(MmpTouchUserModeMemoryForRead)

##
## BOOL
## MmpTouchUserModeMemoryForWrite (
##     PVOID Buffer,
##     UINTN Size
##     )
##

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
    movl    $TRUE, %eax             # Assume success.

MmpTouchUserModeMemoryForWriteLoop:
    orl     $0, (%rdi)              # Do a dummy write.
    cmpq    $0x1000, %rsi           # Compare to a page.
    jg      MmpTouchUserModeMemoryForWriteNextPage   # Do a full page.
    cmpq    $0, %rsi                # Compare to zero.
    je      MmpUserModeMemoryReturn # Jump out.
    addq    %rsi, %rdi              # Add the remainder.
    subq    $1, %rdi                # Subtract one for the last address.
    orl     $0, (%rdi)              # Write the last byte.
    jmp     MmpUserModeMemoryReturn

MmpTouchUserModeMemoryForWriteNextPage:
    subq    $0x1000, %rsi
    addq    $0x1000, %rdi
    jmp MmpTouchUserModeMemoryForWriteLoop

END_FUNCTION(MmpTouchUserModeMemoryForWrite)

##
## BOOL
## MmUserRead8 (
##     PVOID Buffer,
##     PUCHAR Value
##     )
##

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
    movb    (%rdi), %al             # Read from the source.
    movb    %al, (%rsi)             # Save it to the destination.
    movl    $TRUE, %eax             # Result is good.
    ret                             # Return.

END_FUNCTION(MmUserRead8)

##
## BOOL
## MmUserWrite8 (
##     PVOID Buffer,
##     UCHAR Value
##     )
##

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
    movb    %sil, (%rdi)            # Set the byte in there.
    movl    $TRUE, %eax             # Result is good.
    ret                             # Return.

END_FUNCTION(MmUserWrite8)

##
## BOOL
## MmUserRead16 (
##     PVOID Buffer,
##     PUSHORT Value
##     )
##

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
    movw    (%rdi), %ax             # Read from the source.
    movw    %ax, (%rsi)             # Save it to the destination.
    movl    $TRUE, %eax             # Result is good.
    ret                             # Return.

END_FUNCTION(MmUserRead16)

##
## BOOL
## MmUserWrite16 (
##     PVOID Buffer,
##     USHORT Value
##     )
##

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
    movw    %si, (%rdi)             # Set the word in there.
    movl    $TRUE, %eax             # Result is good.
    ret                             # Return.

END_FUNCTION(MmUserWrite16)

##
## BOOL
## MmUserRead32 (
##     PVOID Buffer,
##     PULONG Value
##     )
##

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
    movl    (%rdi), %eax            # Read from the source.
    movl    %eax, (%rsi)            # Save it to the destination.
    movl    $TRUE, %eax             # Result is good.
    ret                             # Return.

END_FUNCTION(MmUserRead32)

##
## BOOL
## MmUserWrite32 (
##     PVOID Buffer,
##     ULONG Value
##     )
##

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
    movl    %esi, (%rdi)            # Set the dword in there.
    movl    $TRUE, %eax             # Result is good.
    ret                             # Return.

END_FUNCTION(MmUserWrite32)

##
## BOOL
## MmUserRead64 (
##     PVOID Buffer,
##     PULONGLONG Value
##     )
##

/*++

Routine Description:

    This routine performs a 64-bit read from user mode. This is assumed to be
    naturally aligned.

Arguments:

    Buffer - Supplies a pointer to the buffer to read.

    Value - Supplies a pointer where the read value will be returned.

Return Value:

    TRUE if the read succeeded.

    FALSE if the read failed.

--*/

FUNCTION(MmUserRead64)
    movq    (%rdi), %rax            # Read from the source.
    movq    %rax, (%rsi)            # Save it to the destination.
    movl    $TRUE, %eax             # Result is good.
    ret                             # Return.

END_FUNCTION(MmUserRead64)

##
## BOOL
## MmUserWrite64 (
##     PVOID Buffer,
##     ULONGLONG Value
##     )
##

/*++

Routine Description:

    This routine performs a 64-bit write to user mode. This is assumed to be
    naturally aligned.

Arguments:

    Buffer - Supplies a pointer to the buffer to write to.

    Value - Supplies the value to write.

Return Value:

    TRUE if the write succeeded.

    FALSE if the write failed.

--*/

FUNCTION(MmUserWrite64)
    movq    %rsi, (%rdi)            # Set the dword in there.
    movl    $TRUE, %eax             # Result is good.
    ret                             # Return.

END_FUNCTION(MmUserWrite64)

##
## This common epilog is both jumped to by the memory routines directly
## (sometimes), as well as routed to by the page fault code if it detects a
## fault in one of the user mode memory routines.
##

FUNCTION(MmpUserModeMemoryReturn)
    ret                             # Return.

END_FUNCTION(MmpUserModeMemoryReturn)

##
## --------------------------------------------------------- Internal Functions
##

