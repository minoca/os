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

    Evan Green 18-Aug-2014

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

PROTECTED_FUNCTION RtlCopyMemory
    stmdb   %sp!, {%r0}                         @ Save the destination.
    cmp     %r2, #0                             @ See if byte count is zero.
    beq     RtlCopyMemoryBytesDone              @ Branch out if so.
    cmp     %r2, #0x4                           @ See if the copy is short.
    blt     RtlCopyMemoryBytes                  @ Do byte copy if so.
    sub     %r3, %r0, %r1                       @ Compare pointer alignment.
    tst     %r3, #3                             @ Test for word agreement.
    bne     RtlCopyMemoryBytes                  @ Branch if not similar.

    //
    // Both buffers have the same alignment, so at least some of the data can
    // be word-copied.
    //

    ands    %r3, %r0, #3                        @ Test for word alignment.
    beq     RtlCopyMemoryAligned                @ Jump over if aligned.
    rsb     %r3, %r3, #4                        @ Get number of unaligned bytes.
    sub     %r2, %r2, %r3                       @ Remove from total.

    //
    // Copy the non-aligned portion.
    //

RtlCopyMemoryUnalignedBytes:
    ldrb    %r12, [%r1], #1                     @ Read a byte from the source.
    subs    %r3, %r3, #1                        @ Decrement the loop count.
    strb    %r12, [%r0], #1                     @ Store to the destination.
    bne     RtlCopyMemoryUnalignedBytes         @ Loop.

    //
    // Copy words eight at a time.
    //

RtlCopyMemoryAligned:
    movs    %r3, %r2, asr #5                    @ Get the number of chunks.
    beq     RtlCopyMemoryChunksDone             @ Jump out if none.
    and     %r2, %r2, #0x1F                     @ Get non-chunk remainder.
    stmdb   %sp!, {%r4-%r10}                    @ Save non-volatile registers.

    //
    // Loop copying chunks.
    //

RtlCopyMemoryChunks:
    ldmia   %r1!, {%r4-%r10, %r12}              @ Breathe in a chunk.
    subs    %r3, %r3, #1                        @ Subtract chunk from the count.
    stmia   %r0!, {%r4-%r10, %r12}              @ Breathe out a chunk.
    bne     RtlCopyMemoryChunks                 @ Loop if more chunks.
    ldmia   %sp!, {%r4-%r10}                    @ Restore non-volatiles.

RtlCopyMemoryChunksDone:
    movs    %r3, %r2, asr #2                    @ Get remaining word count.
    beq     RtlCopyMemoryWordsDone              @ Jump out if none.

    //
    // Copy a word at a time.
    //

RtlCopyMemoryWords:
    ldr     %r12, [%r1], #4                     @ Load a word.
    subs    %r3, %r3, #1                        @ Decrement word count.
    str     %r12, [%r0], #4                     @ Store a word.
    bne     RtlCopyMemoryWords                  @ Loop to copy more words.

RtlCopyMemoryWordsDone:
    ands    %r2, %r2, #3                        @ Get byte remainder.
    beq     RtlCopyMemoryBytesDone              @ Jump out if none.

    //
    // Copy any remaining bytes one at a time.
    //

RtlCopyMemoryBytes:
    ldrb    %r12, [%r1], #1                     @ Read in a byte.
    subs    %r2, %r2, #1                        @ Decrement loop count.
    strb    %r12, [%r0], #1                     @ Write out a byte.
    bne     RtlCopyMemoryBytes                  @ Loop if not done.

RtlCopyMemoryBytesDone:
    ldmia   %sp!, {%r0}                         @ Destination is return value.
    bx      %lr                                 @ Return.

END_FUNCTION RtlCopyMemory

//
// RTL_API
// VOID
// RtlZeroMemory (
//     PVOID Buffer,
//     UINTN ByteCount
//     );
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

PROTECTED_FUNCTION RtlZeroMemory
    mov     %r2, %r1                            @ Move the count to parameter 3.
    eor     %r1, %r1, %r1                       @ Set parameter 2 to zero.
    b       RtlSetMemory                        @ Call set memory.

END_FUNCTION RtlZeroMemory

//
// RTL_API
// PVOID
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

    Returns the buffer pointer.

--*/

PROTECTED_FUNCTION RtlSetMemory
    stmdb   %sp!, {%r0}                         @ Save the destination.
    cmp     %r2, #0                             @ See if byte count is zero.
    beq     RtlSetMemoryBytesDone               @ Branch out if so.
    cmp     %r2, #0x4                           @ See if the set is short.
    blt     RtlSetMemoryBytes                   @ Do byte operation if so.

    //
    // Set the unaligned portion at the beginning byte for byte.
    //

    ands    %r3, %r0, #3                        @ Test for word alignment.
    beq     RtlSetMemoryAligned                 @ Jump over if aligned.
    rsb     %r3, %r3, #4                        @ Get number of unaligned bytes.
    sub     %r2, %r2, %r3                       @ Remove from total.

    //
    // Set the unaligned portion.
    //

RtlSetMemoryUnalignedBytes:
    subs    %r3, %r3, #1                        @ Decrement the loop count.
    strb    %r1, [%r0], #1                      @ Store to the destination.
    bne     RtlSetMemoryUnalignedBytes          @ Loop.

    //
    // Prepare to set words eight at a time.
    //

RtlSetMemoryAligned:
    and     %r1, %r1, #0xFF                     @ Get the byte to set.
    orr     %r1, %r1, %r1, lsl #8               @ Copy to second least byte.
    orr     %r1, %r1, %r1, lsl #16              @ Copy low word to high word.
    movs    %r3, %r2, asr #5                    @ Get the number of chunks.
    beq     RtlSetMemoryChunksDone              @ Jump out if none.
    and     %r2, %r2, #0x1F                     @ Get non-chunk remainder.
    stmdb   %sp!, {%r4-%r10}                    @ Save non-volatile registers.
    mov     %r4, %r1                            @ Copy value.
    mov     %r5, %r1                            @ Copy value.
    mov     %r6, %r1                            @ Copy value.
    mov     %r7, %r1                            @ Copy value.
    mov     %r8, %r1                            @ Copy value.
    mov     %r9, %r1                            @ Copy value.
    mov     %r10, %r1                           @ Copy value.
    mov     %r12, %r1                           @ Copy value.

    //
    // Loop setting chunks.
    //

RtlSetMemoryChunks:
    subs    %r3, %r3, #1                        @ Subtract chunk from the count.
    stmia   %r0!, {%r4-%r10, %r12}              @ Breathe out a chunk.
    bne     RtlSetMemoryChunks                  @ Loop if more chunks.
    ldmia   %sp!, {%r4-%r10}                    @ Restore non-volatiles.

RtlSetMemoryChunksDone:
    movs    %r3, %r2, asr #2                    @ Get remaining word count.
    beq     RtlSetMemoryWordsDone               @ Jump out if none.

    //
    // Set a word at a time.
    //

RtlSetMemoryWords:
    subs    %r3, %r3, #1                        @ Decrement word count.
    str     %r1, [%r0], #4                      @ Store a word.
    bne     RtlSetMemoryWords                   @ Loop to set more words.

RtlSetMemoryWordsDone:
    ands    %r2, %r2, #3                        @ Get byte remainder.
    beq     RtlSetMemoryBytesDone               @ Jump out if none.

    //
    // Set any remaining bytes one at a time.
    //

RtlSetMemoryBytes:
    subs    %r2, %r2, #1                        @ Decrement loop count.
    strb    %r1, [%r0], #1                      @ Write out a byte.
    bne     RtlSetMemoryBytes                   @ Loop if not done.

RtlSetMemoryBytesDone:
    ldmia   %sp!, {%r0}                         @ Destination is return value.
    bx      %lr                                 @ Return.

END_FUNCTION RtlSetMemory

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

PROTECTED_FUNCTION RtlCompareMemory
    cmp     %r2, #0                             @ Check for zero byte count.
    beq     RtlCompareMemoryReturnTrue          @ Return TRUE if so.

RtlCompareMemoryLoop:
    ldrb    %r3, [%r0], #1                      @ Get first byte.
    ldrb    %r12, [%r1], #1                     @ Get second byte.
    cmp     %r3, %r12                           @ Compare.
    bne     RtlCompareMemoryReturnFalse         @ Break out if not equal.
    subs    %r2, %r2, #1                        @ Decrement loop count.
    bne     RtlCompareMemoryLoop                @ Compare more bytes.

RtlCompareMemoryReturnTrue:
    mov     %r0, #1                             @ Set return value to true.
    b       RtlCompareMemoryReturn              @ Jump to return.

RtlCompareMemoryReturnFalse:
    mov     %r0, #0                             @ Set return value to false.

RtlCompareMemoryReturn:
    bx      %lr                                 @ Return.

END_FUNCTION RtlCompareMemory

//
// --------------------------------------------------------- Internal Functions
//

