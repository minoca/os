/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    archsup.S

Abstract:

    This module implements assembly-based architecture support routines for the
    ARMv7 platform.

Author:

    Evan Green 27-Feb-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/arm.inc>

//
// ---------------------------------------------------------------- Definitions
//

//
// ---------------------------------------------------------------------- Code
//

ASSEMBLY_FILE_HEADER

//
// VOID
// EfiMemoryBarrier (
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

FUNCTION EfiMemoryBarrier
    DMB
    bx      %lr

END_FUNCTION EfiMemoryBarrier

//
// VOID
// EfipCleanEntireCache (
//     VOID
//     )
//

/*++

Routine Description:

    This routine cleans the entire data cache.

Arguments:

    None.

Return Value:

    None.

--*/

FUNCTION EfipCleanEntireCache
    stmdb   %sp!, {%r4-%r11}            @ Save non-volatile registers.
    mrc     p15, 1, %r0, c0, c0, 1      @ Read CLIDR into R0.
    ands    %r3, %r0, #0x7000000        @
    mov     %r3, %r3, LSR #23           @ Cache level value (naturally aligned).
    beq     EfipCleanEntireCacheEnd     @
    mov     %r10, #0                    @

EfipCleanEntireCacheLoop1:
    add     %r2, %r10, %r10, LSR #1     @ Work out 3 x cache level.
    mov     %r1, %r0, LSR %r2           @ Bottom 3 bits are the Cache Type for
    and     %r1, %r1, #7                @ this level. Get those 3 bits.
    cmp     %r1, #2                     @ Check to see if there's no cache or
    blt     EfipCleanEntireCacheSkip    @ only instruction cache at this level.
    mcr     p15, 2, %r10, c0, c0, 0     @ Write CSSELR from R10.
    ISB                                 @ ISB to sync the change to CCSIDR.
    mrc     p15, 1, %r1, c0, c0, 0      @ Read current CCSIDR
    and     %r2, %r1, #7                @ Extract the line length field.
    add     %r2, %r2, #4                @ Add 4 for the line length offset
    ldr     %r4, =0x3FF                 @ (log2 16 bytes).
    ands    %r4, %r4, %r1, LSR #3       @ R4 is the max number on the way size
                                        @ (right aligned).
    clz     %r5, %r4                    @ R5 is the bit position of way size
                                        @ increment.
    mov     %r9, %r4                    @ R9 is the working copy of the max way
                                        @ size (right aligned).
EfipCleanEntireCacheLoop2:
    ldr     %r7, =0x00007FFF            @
    ands    %r7, %r7, %r1, LSR #13      @ R7 is the max number of the index size
                                        @ (right aligned).
EfipCleanEntireCacheLoop3:
    lsl     %r11, %r9, %r5              @ Factor in the way number and cache
    orr     %r11, %r10, %r11            @ number into R11.
    lsl     %r4, %r7, %r2               @ Factor in the
    orr     %r11, %r11, %r4             @ index number.
    mcr     p15, 0, %r11, c7, c10, 2    @ DCCSW, clean by set/way.
    subs    %r7, %r7, #1                @ Decrement the index.
    bge     EfipCleanEntireCacheLoop3   @
    subs    %r9, %r9, #1                @ Decrement the way number.
    bge     EfipCleanEntireCacheLoop2   @

EfipCleanEntireCacheSkip:
    add     %r10, %r10, #2              @ Increment the cache number.
    cmp     %r3, %r10
    bgt     EfipCleanEntireCacheLoop1

EfipCleanEntireCacheEnd:
    mcr     p15, 0, %r0, c7, c5, 0      @ Write to ICIALLU, invalidate i-cache.
    ldmia   %sp!, {%r4-%r11}            @ Restore non-volatile registers.
    DSB                                 @ Data Synchronization barrier.
    bx      %lr

END_FUNCTION EfipCleanEntireCache

//
// VOID
// EfipInvalidateInstructionCache (
//     VOID
//     )
//

/*++

Routine Description:

    This routine invalidate the processor's instruction only cache, indicating
    that a page containing code has changed.

Arguments:

    None.

Return Value:

    None.

--*/

FUNCTION EfipInvalidateInstructionCache
    DSB
    mcr     p15, 0, %r0, c7, c5, 0      @ ICIALLU, Invalidate I-Cache.
    BPIALL                              @ BPIALL, Invalidate Branch Predictor.
    DSB                                 @ Make instructions finish.
    ISB                                 @ Prevent speculative fetching.
    bx      %lr                         @ Return

END_FUNCTION EfipInvalidateInstructionCache

//
// --------------------------------------------------------- Internal Functions
//

