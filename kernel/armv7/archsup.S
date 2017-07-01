/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    archsup.S

Abstract:

    This module implements ARMv7 processor architecture features not
    implementable in C.

Author:

    Evan Green 11-Aug-2012

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
// VOID
// ArCleanEntireCache (
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

FUNCTION ArCleanEntireCache
    stmdb   %sp!, {%r4-%r11}            @ Save non-volatile registers.
    mrc     p15, 1, %r0, c0, c0, 1      @ Read CLIDR into R0.
    ands    %r3, %r0, #0x7000000        @
    mov     %r3, %r3, LSR #23           @ Cache level value (naturally aligned).
    beq     ArCleanEntireCacheEnd       @
    mov     %r10, #0                    @

ArCleanEntireCacheLoop1:
    add     %r2, %r10, %r10, LSR #1     @ Work out 3 x cache level.
    mov     %r1, %r0, LSR %r2           @ Bottom 3 bits are the Cache Type for
    and     %r1, %r1, #7                @ this level. Get those 3 bits.
    cmp     %r1, #2                     @ Check to see if there's no cache or
    blt     ArCleanEntireCacheSkip      @ only instruction cache at this level.
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
ArCleanEntireCacheLoop2:
    ldr     %r7, =0x00007FFF            @
    ands    %r7, %r7, %r1, LSR #13      @ R7 is the max number of the index size
                                        @ (right aligned).
ArCleanEntireCacheLoop3:
    lsl     %r11, %r9, %r5              @ Factor in the way number and cache
    orr     %r11, %r10, %r11            @ number into R11.
    lsl     %r4, %r7, %r2               @ Factor in the
    orr     %r11, %r11, %r4             @ index number.
    mcr     p15, 0, %r11, c7, c10, 2    @ DCCSW, clean by set/way.
    subs    %r7, %r7, #1                @ Decrement the index.
    bge     ArCleanEntireCacheLoop3     @
    subs    %r9, %r9, #1                @ Decrement the way number.
    bge     ArCleanEntireCacheLoop2     @

ArCleanEntireCacheSkip:
    add     %r10, %r10, #2              @ Increment the cache number.
    cmp     %r3, %r10
    bgt     ArCleanEntireCacheLoop1

ArCleanEntireCacheEnd:
    mcr     p15, 0, %r0, c7, c5, 0      @ Write to ICIALLU
    ldmia   %sp!, {%r4-%r11}            @ Restore non-volatile registers.
    DSB                                 @ Data Synchronization barrier.
    bx      %lr

END_FUNCTION ArCleanEntireCache

//
// VOID
// ArCleanInvalidateEntireCache (
//     VOID
//     )
//

/*++

Routine Description:

    This routine cleans and invalidates the entire data cache.

Arguments:

    None.

Return Value:

    None.

--*/

FUNCTION ArCleanInvalidateEntireCache
    stmdb   %sp!, {%r4-%r11}            @ Save non-volatile registers.
    mrc     p15, 1, %r0, c0, c0, 1      @ Read CLIDR into R0.
    ands    %r3, %r0, #0x7000000        @
    mov     %r3, %r3, LSR #23           @ Cache level value (naturally aligned).
    beq     ArCleanInvalidateEntireCacheEnd       @
    mov     %r10, #0                    @

ArCleanInvalidateEntireCacheLoop1:
    add     %r2, %r10, %r10, LSR #1     @ Work out 3 x cache level.
    mov     %r1, %r0, LSR %r2           @ Bottom 3 bits are the Cache Type for
    and     %r1, %r1, #7                @ this level. Get those 3 bits.
    cmp     %r1, #2                     @ Check to see if there's no cache at
    blt     ArCleanInvalidateEntireCacheSkip   @ this level.
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
ArCleanInvalidateEntireCacheLoop2:
    ldr     %r7, =0x00007FFF            @
    ands    %r7, %r7, %r1, LSR #13      @ R7 is the max number of the index size
                                        @ (right aligned).
ArCleanInvalidateEntireCacheLoop3:
    lsl     %r11, %r9, %r5              @ Factor in the way number and cache
    orr     %r11, %r10, %r11            @ number into R11.
    lsl     %r4, %r7, %r2               @ Factor in the
    orr     %r11, %r11, %r4             @ index number.
    mcr     p15, 0, %r11, c7, c14, 2    @ DCCISW, clean and invalidate set/way.
    subs    %r7, %r7, #1                @ Decrement the index.
    bge     ArCleanInvalidateEntireCacheLoop3
    subs    %r9, %r9, #1                @ Decrement the way number.
    bge     ArCleanInvalidateEntireCacheLoop2

ArCleanInvalidateEntireCacheSkip:
    add     %r10, %r10, #2              @ Increment the cache number.
    cmp     %r3, %r10
    bgt     ArCleanInvalidateEntireCacheLoop1

ArCleanInvalidateEntireCacheEnd:
    mcr     p15, 0, %r0, c7, c5, 0      @ Write to ICIALLU
    DSB
    ldmia   %sp!, {%r4-%r11}            @ Restore non-volatile registers.
    bx      %lr

END_FUNCTION ArCleanInvalidateEntireCache

//
// ULONG
// ArGetMultiprocessorIdRegister (
//     VOID
//     )
//

/*++

Routine Description:

    This routine gets the Multiprocessor ID register (MPIDR).

Arguments:

    None.

Return Value:

    Returns the value of the MPIDR.

--*/

FUNCTION ArGetMultiprocessorIdRegister
    mrc     p15, 0, %r0, %c0, %c0, 5            @ Get the MPIDR
    bx      %lr                                 @

END_FUNCTION ArGetMultiprocessorIdRegister

//
// ULONG
// ArGetPerformanceControlRegister (
//     VOID
//     )
//

/*++

Routine Description:

    This routine retrieves the PMCR (Performance Monitor Control Register).

Arguments:

    None.

Return Value:

    Returns the value of the PMCR.

--*/

FUNCTION ArGetPerformanceControlRegister
    mrc     p15, 0, %r0, %c9, %c12, 0           @ Get the PMCR.
    bx      %lr                                 @

END_FUNCTION ArGetPerformanceControlRegister

//
// VOID
// ArSetPerformanceControlRegister (
//     ULONG Value
//     )
//

/*++

Routine Description:

    This routine sets the PMCR (Performance Monitor Control Register).

Arguments:

    Value - Supplies the value to set in the PMCR.

Return Value:

    None.

--*/

FUNCTION ArSetPerformanceControlRegister
    mcr     p15, 0, %r0, %c9, %c12, 0           @ Set the PMCR.
    bx      %lr                                 @

END_FUNCTION ArSetPerformanceControlRegister

//
// VOID
// ArClearPerformanceInterruptRegister (
//     ULONG Value
//     )
//

/*++

Routine Description:

    This routine sets the PMINTENCLR (Performance Monitor Interrupt Clear)
    register.

Arguments:

    Value - Supplies the value to set in the PMINTENCLR.

Return Value:

    None.

--*/

FUNCTION ArClearPerformanceInterruptRegister
    mcr     p15, 0, %r0, %c9, %c14, 2           @ Set the PMINTENCLR.
    bx      %lr                                 @

END_FUNCTION ArClearPerformanceInterruptRegister

//
// VOID
// ArSetPerformanceUserEnableRegister (
//     ULONG Value
//     )
//

/*++

Routine Description:

    This routine sets the PMUSERENR (Performance Monitor User Enable Register).

Arguments:

    Value - Supplies the value to set in the PMUSERENR.

Return Value:

    None.

--*/

FUNCTION ArSetPerformanceUserEnableRegister
    mcr     p15, 0, %r0, %c9, %c14, 0           @ Set the PMUSERENR.
    bx      %lr                                 @

END_FUNCTION ArSetPerformanceUserEnableRegister

//
// ULONG
// ArGetPerformanceCounterEnableRegister (
//     VOID
//     )
//

/*++

Routine Description:

    This routine retrieves the PMCNTENSET (Performance Monitor Counter Enable
    Set) register.

Arguments:

    None.

Return Value:

    Returns the value of the PMCNTENSET.

--*/

FUNCTION ArGetPerformanceCounterEnableRegister
    mrc     p15, 0, %r0, %c9, %c12, 1           @ Get the PMCNTENSET register.
    bx      %lr                                 @

END_FUNCTION ArGetPerformanceCounterEnableRegister

//
// VOID
// ArSetCycleCountEnableRegister (
//     ULONG Value
//     )
//

/*++

Routine Description:

    This routine sets the PMCNTENSET (Performance Monitor Counter Enable
    Set) register.

Arguments:

    Value - Supplies the value to set in the PMCNTENSET register.

Return Value:

    None.

--*/

FUNCTION ArSetPerformanceCounterEnableRegister
    mcr     p15, 0, %r0, %c9, %c12, 1           @ Set the PMCNTENSET register.
    bx      %lr                                 @

END_FUNCTION ArSetPerformanceCounterEnableRegister

//
// ULONG
// ArGetCycleCountRegister (
//     VOID
//     )
//

/*++

Routine Description:

    This routine retrieves the PMCCNTR (Performance Monitor Cycle Counter)
    register.

Arguments:

    None.

Return Value:

    Returns the value of the PMCCNTR.

--*/

FUNCTION ArGetCycleCountRegister
    mrc     p15, 0, %r0, %c9, %c13, 0           @ Get the PMCCNTR register.
    bx      %lr                                 @

END_FUNCTION ArGetCycleCountRegister

//
// VOID
// ArSetCycleCountRegister (
//     ULONG Value
//     )
//

/*++

Routine Description:

    This routine sets the PMCCNTR (Performance Monitor Cycle Counter) register.

Arguments:

    Value - Supplies the value to set in the PMCCNTR register.

Return Value:

    None.

--*/

FUNCTION ArSetCycleCountRegister
    mcr     p15, 0, %r0, %c9, %c13, 0           @ Set the PMCCNTR register.
    bx      %lr                                 @

END_FUNCTION ArSetCycleCountRegister

//
// --------------------------------------------------------- Internal Functions
//

