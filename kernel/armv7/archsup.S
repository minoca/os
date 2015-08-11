/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

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

##
## ------------------------------------------------------------------ Includes
##

#include <minoca/arm.inc>

##
## --------------------------------------------------------------- Definitions
##

##
## ---------------------------------------------------------------------- Code
##

ASSEMBLY_FILE_HEADER

##
## VOID
## ArCleanEntireCache (
##     VOID
##     )
##

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
    isb                                 @ ISB to sync the change to CCSIDR.
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
    dsb                                 @ Data Synchronization barrier.
    bx      %lr

END_FUNCTION ArCleanEntireCache

##
## VOID
## ArCleanInvalidateEntireCache (
##     VOID
##     )
##

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
    isb                                 @ ISB to sync the change to CCSIDR.
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
    dsb
    ldmia   %sp!, {%r4-%r11}            @ Restore non-volatile registers.
    bx      %lr

END_FUNCTION ArCleanInvalidateEntireCache

##
## VOID
## ArInvalidateInstructionCache (
##     VOID
##     )
##

/*++

Routine Description:

    This routine invalidate the processor's instruction only cache, indicating
    that a page containing code has changed.

Arguments:

    None.

Return Value:

    None.

--*/

FUNCTION ArInvalidateInstructionCache
    dsb
    mcr     p15, 0, %r0, c7, c5, 0      @ ICIALLU, Invalidate I-Cache.
    mcr     p15, 0, %r0, c7, c5, 6      @ BPIALL, Invalidate Branch Predictor.
    dsb                                 @ Make instructions finish.
    isb                                 @ Prevent speculative fetching.
    bx      %lr                         @ Return

END_FUNCTION ArInvalidateInstructionCache

##
## VOID
## ArInvalidateTlbEntry (
##     PVOID Address
##     )
##

/*++

Routine Description:

    This routine invalidates one TLB entry corresponding to the given virtual
    address.

Arguments:

    Address - Supplies the virtual address whose associated TLB entry will be
        invalidated.

Return Value:

    None.

--*/

FUNCTION ArInvalidateTlbEntry
    dsb                                         @ Ensure changes are visible.
    mcr     p15, 0, %r0, %cr8, %cr7, 1          @ Write to TLBIMVA.
    mcr     p15, 0, %r0, %cr7, %cr5, 6          @ Write to BPIALL (branch pred).
    dsb                                         @ Data synchronization barrier.
    isb                                         @ Instruction sync barrier.
    bx      %lr                                 @

END_FUNCTION ArInvalidateTlbEntry

##
## VOID
## ArInvalidateEntireTlb (
##     VOID
##     )
##

/*++

Routine Description:

    This routine invalidates the entire TLB.

Arguments:

    None.

Return Value:

    None.

--*/

FUNCTION ArInvalidateEntireTlb
    dsb                                         @ Ensure changes are visible.
    mcr     p15, 0, %r0, c8, c7, 0              @ TLBIALL (invalidate TLB).
    mcr     p15, 0, %r0, c7, c5, 6              @ Write to BPIALL (branch pred).
    dsb                                         @ Data synchronization barrier.
    isb                                         @ Instruction sync barrier.
    bx      %lr                                 @

END_FUNCTION ArInvalidateEntireTlb

##
## ULONG
## ArLockTlbEntry (
##     ULONG TlbEntry,
##     PVOID VirtualAddress,
##     ULONG NextTlbEntry
##     )
##

/*++

Routine Description:

    This routine locks a translation in the TLB. This translation will stick
    even across total TLB invalidates.

Arguments:

    TlbEntry - Supplies the base and victim number of the TLB entry to lock.

    VirtualAddress - Supplies the virtual address that should be locked in the
        TLB. The association to physical address will be created by touching
        that address, so the address had better be mapped.

    NextTlbEntry - Supplies the base and victim number to set after locking the
        entry.

Return Value:

    Returns the value of the lockdown register after the TLB miss was forced.
    The lowest bit of this value should be set. If it is not, this indicates
    that TLB lockdown is not supported.

--*/

FUNCTION ArLockTlbEntry
    orr     %r0, %r0, #1                        @ Set lock flag.
    mcr     p15, 0, %r1, %cr8, %cr7, 1          @ Invalidate the TLB entry.
    mcr     p15, 0, %r0, %cr10, %cr0, 0         @ Write lockdown register.
    mcr     p15, 0, %r1, %cr10, %cr1, 0         @ Prefetch data TLB.
    ldr     %r3, [%r1]                          @ Also do standard load.
    mrc     p15, 0, %r0, %cr10, %cr0, 0         @ Read the lockdown register.
    bic     %r2, %r2, #1                        @ Clear lock flag.
    mcr     p15, 0, %r2, %cr10, %cr0, 0         @ Write lockdown register.
    dsb                                         @ Make instructions finish.
    isb                                         @ Prevent speculative fetching.
    bx      %lr                                 @ Return.

END_FUNCTION ArLockTlbEntry

##
## VOID
## ArSerializeExecution (
##     VOID
##     )
##

/*++

Routine Description:

    This routine acts a serializing instruction, preventing the processor
    from speculatively executing beyond this point.

Arguments:

    None.

Return Value:

    None.

--*/

FUNCTION ArSerializeExecution
    dsb
    isb
    bx      %lr

END_FUNCTION ArSerializeExecution

##
## ULONG
## ArGetMultiprocessorIdRegister (
##     VOID
##     )
##

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

##
## ULONG
## ArGetPerformanceControlRegister (
##     VOID
##     )
##

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

##
## VOID
## ArSetPerformanceControlRegister (
##     ULONG Value
##     )
##

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

##
## VOID
## ArClearPerformanceInterruptRegister (
##     ULONG Value
##     )
##

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

##
## VOID
## ArSetPerformanceUserEnableRegister (
##     ULONG Value
##     )
##

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

##
## ULONG
## ArGetPerformanceCounterEnableRegister (
##     VOID
##     )
##

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

##
## VOID
## ArSetCycleCountEnableRegister (
##     ULONG Value
##     )
##

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

##
## ULONG
## ArGetCycleCountRegister (
##     VOID
##     )
##

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

##
## VOID
## ArSetCycleCountRegister (
##     ULONG Value
##     )
##

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

##
## VOID
## ArWaitForInterrupt (
##     VOID
##     )
##

/*++

Routine Description:

    This routine halts the processor until the next interrupt comes in. This
    routine should be called with interrupts disabled, and will return with
    interrupts enabled.

Arguments:

    None.

Return Value:

    None.

--*/

FUNCTION ArWaitForInterrupt
    dsb                                         @ Ensure everything is done.
    wfi                                         @ Wait for interrupt.
    cpsie   if                                  @ Enable interrupts.
    bx      %lr

END_FUNCTION ArWaitForInterrupt

##
## --------------------------------------------------------- Internal Functions
##

