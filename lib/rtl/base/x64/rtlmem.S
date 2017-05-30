/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    rtlmem.S

Abstract:

    This module contains memory routines in assembly, for speed.

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
## RTL_API
## PVOID
## RtlCopyMemory (
##     PVOID Destination,
##     PCVOID Source,
##     ULONG ByteCount
##     )
##

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

    ##
    ## It just so happens that the destination is already in rdi, source in
    ## rsi. Move count to rcx and copy.
    ##

    movq    %rdx, %rcx              # Move count to rcx.
    cld                             # Clear the direction flag.
    rep movsb                       # Copy bytes.
    ret                             # Return.

END_FUNCTION(RtlCopyMemory)

##
## RTL_API
## VOID
## RtlZeroMemory (
##     PVOID Buffer,
##     UINTN ByteCount
##     )
##

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

    ##
    ## The buffer address is already in rdi. Move the count to rcx, clear eax,
    ## and zero bytes.
    ##

    movq    %rsi, %rcx              # Move the count to rcx.
    xorq    %rax, %rax              # Zero out rax.
    cld                             # Clear the direction flag.
    rep stosb                       # Zero bytes like there's no tomorrow.
    ret                             # Return.

END_FUNCTION(RtlZeroMemory)

##
## RTL_API
## VOID
## RtlSetMemory (
##     PVOID Buffer,
##     INT Byte,
##     UINTN Count
##     )
##

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

    ##
    ## The buffer address is already in rdi. Move the byte to rax and the count
    ## to rcx, and blast off.
    ##

    movq    %rsi, %rax              # Move the byte to rax.
    movq    %rdx, %rcx              # Move the count.
    cld                             # Clear the direction flag.
    rep stosb                       # Set bytes like the wind.
    ret                             # Return.

END_FUNCTION(RtlSetMemory)

##
## RTL_API
## BOOL
## RtlCompareMemory (
##     PCVOID FirstBuffer,
##     PCVOID SecondBuffer,
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

    FALSE if the buffers are not equal.

--*/

PROTECTED_FUNCTION(RtlCompareMemory)

    ##
    ## The first buffer pointer is already in rdi, the second is already in
    ## rsi. Move the count to rcx and compare.
    ##

    movq    %rdx, %rcx              # Move count to rcx.
    xorq    %rax, %rax              # Zero out the return value.
    cld                             # Clear the direction flag.
    repe cmpsb                      # Compare bytes on fire.
    setz    %al                     # Return TRUE if buffers are equal.
    ret                             # Return.

END_FUNCTION(RtlCompareMemory)

##
## --------------------------------------------------------- Internal Functions
##

