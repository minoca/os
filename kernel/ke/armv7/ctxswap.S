/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ctxswap.S

Abstract:

    This module implements context switching on the ARMv7 processor.

Author:

    Evan Green 25-Aug-2012

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

.equ CONTEXT_SWAP_MAGIC, 0x4A3A2A1A

//
// ---------------------------------------------------------------------- Code
//

ASSEMBLY_FILE_HEADER

//
// VOID
// KepContextSwap (
//     PVOID *SavedStackLocation,
//     PVOID NewStack,
//     ULONGLONG NewThreadPointer,
//     BOOL FirstTime
//     )
//

/*++

Routine Description:

    This routine switches context to the given thread.

Arguments:

    SavedStackLocation - Supplies a pointer where the old stack pointer will
        be saved.

    NewStack - Supplies the new stack address.

    NewThreadPointer - Supplies the new thread pointer data.

    FirstTime - Supplies a boolean indicating whether the thread has never been
        run before.

Return Value:

    None.

--*/

.equ FirstTime, 0x2C

FUNCTION KepContextSwap

    //
    // Save state of the old thread, including flags and general registers.
    //

    mrs     %r12, CPSR              @ Get and save flags.
    stmdb   %sp!, {%r4-%r12, %r14}  @ Save registers.
    ldr     %r12, =CONTEXT_SWAP_MAGIC   @ Load the magic constant.
    stmdb   %sp!, {%r12}            @ Push it on the stack.
    ldr     %r4, [%sp, #FirstTime]  @ Get the FirstTime parameter.
    str     %sp, [%r0]              @ Save the old stack.
    DMB                             @ Ensure that save completed.

    //
    // Set up the new user mode thread pointer registers.
    //

    mcr     p15, 0, %r2, c13, c0, 3                @ Set the TPIDRURO register.
    mcr     p15, 0, %r3, c13, c0, 2                @ Set the TPIDRURW register.

    //
    // Switch to the new thread's kernel stack, effectively freezing the old
    // thread. Then perform any work that must be completed on the new stack.
    // Touch the stack before switching to it to trigger any top-level page
    // table updates needed to run the new stack on the old TTBR.
    //

    ldr     %r0, [%r1]              @ Touch the new stack to trigger faults.
    mov     %sp, %r1                @ Switch to the new stack.

.if THUMB
    mov     %r7, #0                 @ Zero out FP so the call stack stops here.
.else
    mov     %r11, #0                @ Zero out FP so the call stack stops here.
.endif

    //
    // Perform any post-stack switch work needed on the old thread.
    //

    bl      KepPostContextSwapWork  @ Perform post stack switch work.

    //
    // Figure out if this is the new thread's first run, and jump over if not.
    //

    cmp     %r4, #FALSE
    beq     ContextSwapRestore

ContextSwapFirstRun:
    mov     %r0, %sp                @ Set the trap frame parameter.
    bl      KepPreThreadStartWork   @ Perform any thread initialization.

    //
    // If this is the thread's first run, the stack is set up with a trap frame.
    // Restore it in the same manner as coming off an interrupt.
    //

    ARM_EXIT_INTERRUPT

ContextSwapRestore:

    //
    // Restore the registers pushed during the context save, but first validate
    // that the magic constant is on the top of the stack. If it's not, there
    // are some serious problems, as in all likelihood garbage is about to be
    // popped out.
    //

    ldr     %r12, =CONTEXT_SWAP_MAGIC  @ Load the magic constant.
    ldmia   %sp!, {%r11}               @ Pop the value off the stack.
    cmp     %r11, %r12                 @ Compare values.
    beq     ContextSwapRestoreValidStack    @ Jump over break if valid.
    DEBUGGER_BREAK                     @ Breaking here is bad news.

ContextSwapRestoreValidStack:

    //
    // Pop the registers and the flags.
    //

    ldmia   %sp!, {%r4-%r12, %r14}  @ Breaking here is *bad*. Look up!
    msr     CPSR_cxsf, %r12

    //
    // Return back to the caller.
    //

    bx      %lr

END_FUNCTION KepContextSwap

