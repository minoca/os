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

    Evan Green 6-Nov-2014

Environment:

    Kernel mode

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
// Warning: Don't add functions above MmpCopyUserModeMemory without also
// adjusting the fault detection code, as this function is used as the lower
// border of the user mode functions.
//

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

FUNCTION MmpCopyUserModeMemory
    cmp     %r2, #0                             @ See if byte count is zero.
    beq     MmpCopyUserModeMemoryBytesDone      @ Branch out if so.
    cmp     %r2, #0x4                           @ See if the copy is short.
    blt     MmpCopyUserModeMemoryBytes          @ Do byte copy if so.
    sub     %r3, %r0, %r1                       @ Compare pointer alignment.
    tst     %r3, #3                             @ Test for word agreement.
    bne     MmpCopyUserModeMemoryBytes          @ Branch if not similar.

    //
    // Both buffers have the same alignment, so at least some of the data can
    // be word-copied.
    //

    ands    %r3, %r0, #3                        @ Test for word alignment.
    beq     MmpCopyUserModeMemoryAligned        @ Jump over if aligned.
    rsb     %r3, %r3, #4                        @ Get number of unaligned bytes.
    sub     %r2, %r2, %r3                       @ Remove from total.

    //
    // Copy the non-aligned portion.
    //

MmpCopyUserModeMemoryUnalignedBytes:
    ldrb    %r12, [%r1], #1                     @ Read a byte from the source.
    subs    %r3, %r3, #1                        @ Decrement the loop count.
    strb    %r12, [%r0], #1                     @ Store to the destination.
    bne     MmpCopyUserModeMemoryUnalignedBytes @ Loop.

    //
    // Copy a word at a time.
    //

MmpCopyUserModeMemoryAligned:
    movs    %r3, %r2, asr #2                    @ Get remaining word count.
    beq     MmpCopyUserModeMemoryWordsDone      @ Jump out if none.

MmpCopyUserModeMemoryWords:
    ldr     %r12, [%r1], #4                     @ Load a word.
    subs    %r3, %r3, #1                        @ Decrement word count.
    str     %r12, [%r0], #4                     @ Store a word.
    bne     MmpCopyUserModeMemoryWords          @ Loop to copy more words.

MmpCopyUserModeMemoryWordsDone:
    ands    %r2, %r2, #3                        @ Get byte remainder.
    beq     MmpCopyUserModeMemoryBytesDone      @ Jump out if none.

    //
    // Copy any remaining bytes one at a time.
    //

MmpCopyUserModeMemoryBytes:
    ldrb    %r12, [%r1], #1                     @ Read in a byte.
    subs    %r2, %r2, #1                        @ Decrement loop count.
    strb    %r12, [%r0], #1                     @ Write out a byte.
    bne     MmpCopyUserModeMemoryBytes          @ Loop if not done.

MmpCopyUserModeMemoryBytesDone:
    mov     %r0, #1                             @ Return successfully.
    bx      %lr                                 @ Return.

END_FUNCTION MmpCopyUserModeMemory

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

FUNCTION MmpZeroUserModeMemory
    mov     %r2, %r1                            @ Move the count to parameter 3.
    eor     %r1, %r1, %r1                       @ Set parameter 2 to zero.
    b       MmpSetUserModeMemory                @ Call set memory.

END_FUNCTION MmpZeroUserModeMemory

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

FUNCTION MmpSetUserModeMemory
    cmp     %r2, #0                             @ See if byte count is zero.
    beq     MmpSetUserModeMemoryBytesDone       @ Branch out if so.
    cmp     %r2, #0x4                           @ See if the set is short.
    blt     MmpSetUserModeMemoryBytes           @ Do byte operation if so.

    //
    // Set the unaligned portion at the beginning byte for byte.
    //

    ands    %r3, %r0, #3                        @ Test for word alignment.
    beq     MmpUserUserModeMemoryAligned        @ Jump over if aligned.
    rsb     %r3, %r3, #4                        @ Get number of unaligned bytes.
    sub     %r2, %r2, %r3                       @ Remove from total.

    //
    // Set the unaligned portion.
    //

MmpSetUserModeMemoryUnalignedBytes:
    subs    %r3, %r3, #1                        @ Decrement the loop count.
    strb    %r1, [%r0], #1                      @ Store to the destination.
    bne     MmpSetUserModeMemoryUnalignedBytes  @ Loop.

    //
    // Prepare to set words at a time.
    //

MmpUserUserModeMemoryAligned:
    and     %r1, %r1, #0xFF                     @ Get the byte to set.
    orr     %r1, %r1, %r1, lsl #8               @ Copy to second least byte.
    orr     %r1, %r1, %r1, lsl #16              @ Copy low word to high word.
    movs    %r3, %r2, asr #2                    @ Get remaining word count.
    beq     MmpSetUserModeMemoryWordsDone       @ Jump out if none.

    //
    // Set a word at a time.
    //

MmpSetUserModeMemoryWords:
    subs    %r3, %r3, #1                        @ Decrement word count.
    str     %r1, [%r0], #4                      @ Store a word.
    bne     MmpSetUserModeMemoryWords           @ Loop to set more words.

MmpSetUserModeMemoryWordsDone:
    ands    %r2, %r2, #3                        @ Get byte remainder.
    beq     MmpSetUserModeMemoryBytesDone       @ Jump out if none.

    //
    // Set any remaining bytes one at a time.
    //

MmpSetUserModeMemoryBytes:
    subs    %r2, %r2, #1                        @ Decrement loop count.
    strb    %r1, [%r0], #1                      @ Write out a byte.
    bne     MmpSetUserModeMemoryBytes           @ Loop if not done.

MmpSetUserModeMemoryBytesDone:
    mov     %r0, #1                             @ Return successfully.
    bx      %lr                                 @ Return.

END_FUNCTION MmpSetUserModeMemory

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

FUNCTION MmpCompareUserModeMemory
    cmp     %r2, #0                             @ Check for zero byte count.
    beq     MmpCompareUserModeMemoryReturnTrue  @ Return TRUE if so.

MmpCompareUserModeMemoryLoop:
    ldrb    %r3, [%r0], #1                      @ Get first byte.
    ldrb    %r12, [%r1], #1                     @ Get second byte.
    cmp     %r3, %r12                           @ Compare.
    bne     MmpCompareUserModeMemoryReturnFalse @ Break out if not equal.
    subs    %r2, %r2, #1                        @ Decrement loop count.
    bne     MmpCompareUserModeMemoryLoop        @ Compare more bytes.

MmpCompareUserModeMemoryReturnTrue:
    mov     %r0, #1                             @ Set return value to true.
    b       MmpCompareUserModeMemoryReturn      @ Jump to return.

MmpCompareUserModeMemoryReturnFalse:
    mov     %r0, #0                             @ Set return value to false.

MmpCompareUserModeMemoryReturn:
    bx      %lr                                 @ Return.

END_FUNCTION MmpCompareUserModeMemory

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

FUNCTION MmpTouchUserModeMemoryForRead
    mov     %r3, #0x1000            @ Load the page size.

MmpTouchUserModeMemoryForReadLoop:
    ldrb    %r2, [%r0]              @ Do a dummy read.
    cmp     %r1, %r3                @ Compare to a page.
    bgt     MmpTouchUserModeMemoryForReadNextPage   @ Advance a page if bigger.
    cmp     %r1, #0                 @ Compare to zero.
    beq     MmpTouchUserModeMemoryForReadReturn     @ Jump out if zero.
    add     %r0, %r0, %r1           @ Get one past the last address.
    sub     %r0, #1                 @ Subtract one to get the last valid one.
    ldrb    %r2, [%r0]              @ Do a dummy read.
    b       MmpTouchUserModeMemoryForReadReturn     @ Jump out

MmpTouchUserModeMemoryForReadNextPage:
    add     %r0, %r0, %r3           @ Move address to next page.
    sub     %r1, %r1, %r3           @ Subtract a page from the size.
    b       MmpTouchUserModeMemoryForReadLoop   @ Loop

MmpTouchUserModeMemoryForReadReturn:
    mov     %r0, #1                 @ Set success status.
    bx      %lr                     @ Return successfully.

END_FUNCTION MmpTouchUserModeMemoryForRead

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

FUNCTION MmpTouchUserModeMemoryForWrite
    mov     %r3, #0x1000            @ Load the page size.

MmpTouchUserModeMemoryForWriteLoop:
    ldrb    %r2, [%r0]              @ Do a dummy read.
    strb    %r2, [%r0]              @ Do a dummy write.
    cmp     %r1, %r3                @ Compare to a page.
    bgt     MmpTouchUserModeMemoryForWriteNextPage  @ Advance a page if bigger.
    cmp     %r1, #0                 @ Compare to zero.
    beq     MmpTouchUserModeMemoryForWriteReturn    @ Jump out if zero.
    add     %r0, %r0, %r1           @ Get one past the last address.
    sub     %r0, #1                 @ Subtract one to get the last valid one.
    ldrb    %r2, [%r0]              @ Do a dummy read.
    strb    %r2, [%r0]              @ Do a dummy write.
    b       MmpTouchUserModeMemoryForWriteReturn    @ Jump out

MmpTouchUserModeMemoryForWriteNextPage:
    add     %r0, %r0, %r3           @ Move address to next page.
    sub     %r1, %r1, %r3           @ Subtract a page from the size.
    b       MmpTouchUserModeMemoryForWriteLoop      @ Loop

MmpTouchUserModeMemoryForWriteReturn:
    mov     %r0, #1                 @ Set success status.
    bx      %lr                     @ Return successfully.

END_FUNCTION MmpTouchUserModeMemoryForWrite

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

FUNCTION MmUserRead8
    ldrb    %r2, [%r0]              @ Perform the read.
    strb    %r2, [%r1]              @ Write it to the value.
    mov     %r0, #1                 @ Set success status.
    bx      %lr                     @ Return.

END_FUNCTION MmUserRead8

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

FUNCTION MmUserWrite8
    strb    %r1, [%r0]              @ Write the value out.
    mov     %r0, #1                 @ Set success status.
    bx      %lr                     @ Return.

END_FUNCTION MmUserWrite8

//
// BOOL
// MmUserRead16 (
//     PVOID Buffer,
//     PUSHORT Value
//     )
//

/*++

Routine Description:

    This routine performs a 16-bit read from user mode. This is assumed to be
    two-byte aligned.

Arguments:

    Buffer - Supplies a pointer to the buffer to read.

    Value - Supplies a pointer where the read value will be returned.

Return Value:

    TRUE if the read succeeded.

    FALSE if the read failed.

--*/

FUNCTION MmUserRead16
    ldrh    %r2, [%r0]              @ Perform the read.
    strh    %r2, [%r1]              @ Write it to the value.
    mov     %r0, #1                 @ Set success status.
    bx      %lr                     @ Return.

END_FUNCTION MmUserRead16

//
// BOOL
// MmUserWrite16 (
//     PVOID Buffer,
//     USHORT Value
//     )
//

/*++

Routine Description:

    This routine performs a 16-bit write to user mode. This is assumed to be
    two-byte aligned.

Arguments:

    Buffer - Supplies a pointer to the buffer to write to.

    Value - Supplies the value to write.

Return Value:

    TRUE if the write succeeded.

    FALSE if the write failed.

--*/

FUNCTION MmUserWrite16
    strh    %r1, [%r0]              @ Write the value out.
    mov     %r0, #1                 @ Set success status.
    bx      %lr                     @ Return.

END_FUNCTION MmUserWrite16

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

FUNCTION MmUserRead32
    ldr     %r2, [%r0]              @ Perform the read.
    str     %r2, [%r1]              @ Write it to the value.
    mov     %r0, #1                 @ Set success status.
    bx      %lr                     @ Return.

END_FUNCTION MmUserRead32

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

FUNCTION MmUserWrite32
    str     %r1, [%r0]              @ Write the value out.
    mov     %r0, #1                 @ Set success status.
    bx      %lr                     @ Return.

END_FUNCTION MmUserWrite32

//
// BOOL
// MmpInvalidateCacheLine (
//     PVOID Address
//     )
//

/*++

Routine Description:

    This routine invalidates the cache line associated with the given virtual
    address. Note that if there was dirty data in the cache line, it will be
    destroyed.

Arguments:

    Address - Supplies the address whose associated cache line will be
        invalidated.

Return Value:

    TRUE on success.

    FALSE if the address was a user mode one and accessing it caused a bad
    fault.

--*/

FUNCTION MmpInvalidateCacheLine
    mcr     p15, 0, %r0, %cr7, %cr6, 1          @ Write to DCIMVAC.
    mov     %r0, #1                             @ Set success status.
    bx      %lr                                 @

END_FUNCTION MmpInvalidateCacheLine

//
// BOOL
// MmpCleanCacheLine (
//     PVOID Address
//     )
//

/*++

Routine Description:

    This routine flushes a cache line, writing any dirty bits back to the next
    level cache.

Arguments:

    Address - Supplies the address whose associated cache line will be
        cleaned.

Return Value:

    TRUE on success.

    FALSE if the address was a user mode one and accessing it caused a bad
    fault.

--*/

FUNCTION MmpCleanCacheLine
    mcr     p15, 0, %r0, %cr7, %cr10, 1         @ Write to DCCMVAC.
    mov     %r0, #1                             @ Set success status.
    bx      %lr                                 @

END_FUNCTION MmpCleanCacheLine

//
// BOOL
// MmpCleanInvalidateCacheLine (
//     PVOID Address
//     )
//

/*++

Routine Description:

    This routine cleans a cache line to the point of coherency and invalidates
    the cache line associated with this address.

Arguments:

    Address - Supplies the address whose associated cache line will be
        cleaned and invalidated.

Return Value:

    TRUE on success.

    FALSE if the address was a user mode one and accessing it caused a bad
    fault.

--*/

FUNCTION MmpCleanInvalidateCacheLine
    mcr     p15, 0, %r0, %cr7, %cr14, 1         @ Write to DCCIMVAC.
    mov     %r0, #1                             @ Set success status.
    bx      %lr                                 @ Return.

END_FUNCTION MmpCleanInvalidateCacheLine

//
// BOOL
// MmpInvalidateInstructionCacheLine (
//     PVOID Address
//     )
//

/*++

Routine Description:

    This routine invalidates a line in the instruction cache by virtual address.

Arguments:

    Address - Supplies the address whose associated instruction cache line will
        be invalidated.

Return Value:

    TRUE on success.

    FALSE if the address was a user mode one and accessing it caused a bad
    fault.

--*/

FUNCTION MmpInvalidateInstructionCacheLine
    mcr     p15, 0, %r0, %cr7, %cr5, 1          @ Write to ICIMVAU.
    mcr     p15, 0, %r0, %cr7, %cr5, 7          @ Write to BPIMVA
    mov     %r0, #1                             @ Set success status.
    bx      %lr                                 @ Return.

END_FUNCTION MmpInvalidateInstructionCacheLine

//
// This epilog can be used for any of the user mode memory functions. It may
// get jumped to by the page fault handler.
//

FUNCTION MmpUserModeMemoryReturn
    bx      %lr                                 @ Return.

END_FUNCTION MmpUserModeMemoryReturn

//
// --------------------------------------------------------- Internal Functions
//

