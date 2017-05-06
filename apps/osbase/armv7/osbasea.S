/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    osbasea.S

Abstract:

    This module implements assembly support for the OS Base library.

Author:

    Evan Green 25-Feb-2013

Environment:

    User Mode

--*/

##
## ------------------------------------------------------------------ Includes
##

#include <minoca/kernel/arm.inc>

##
## --------------------------------------------------------------- Definitions
##

##
## ----------------------------------------------------------------------- Code
##

ASSEMBLY_FILE_HEADER

##
## INTN
## OspSystemCallFull (
##     ULONG SystemCallNumber,
##     PVOID SystemCallParameter
##     )
##

/*++

Routine Description:

    This routine executes a system call which performs a full register
    save/restore.

Arguments:

    SystemCallNumber - Supplies the system call number.

    SystemCallParameter - Supplies the system call parameter.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

FUNCTION OspSystemCallFull
    mov     %r2, #1                 @ Set "full" flag for complete save/restore.
    swi     #0x0                    @ Perform system call.
    bx      %lr                     @ Return.

END_FUNCTION OspSystemCallFull

##
## INTN
## OsSystemCall (
##     ULONG SystemCallNumber,
##     PVOID SystemCallParameter
##     )
##

/*++

Routine Description:

    This routine executes a system call.

Arguments:

    SystemCallNumber - Supplies the system call number.

    SystemCallParameter - Supplies the system call parameter.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

FUNCTION OsSystemCall
    eor     %r2, %r2, %r2           @ Clear "full" flag for fast system calls.
    swi     #0x0                    @ Perform system call.
    bx      %lr                     @ Return.

END_FUNCTION OsSystemCall

##
## VOID
## OspSignalHandler (
##     PSIGNAL_PARAMETERS Parameters,
##     PSIGNAL_CONTEXT Context
##     )
##

/*++

Routine Description:

    This routine is called directly by the kernel when a signal occurs. It
    marshals the parameters and calls the C routine for handling the signal.
    The parameters are stored on the stack with the signal parameters followed
    by the signal context.

Arguments:

    Parameters - Supplies a pointer to the signal parameters.

    Context - Supplies a pointer to the signal context from the kernel.

Return Value:

    None.

--*/

FUNCTION OspSignalHandler
    CFI_DEF_CFA_OFFSET(SIGNAL_PARAMETERS_SIZE + SIGNAL_CONTEXT_SIZE)
    CFI_OFFSET(r0, TRAP_R0)
    CFI_OFFSET(r1, TRAP_R1)
    CFI_OFFSET(r2, TRAP_R2)
    CFI_OFFSET(r3, TRAP_R3)
    CFI_OFFSET(r4, TRAP_R4)
    CFI_OFFSET(r5, TRAP_R5)
    CFI_OFFSET(r6, TRAP_R6)
    CFI_OFFSET(r7, TRAP_R7)
    CFI_OFFSET(r8, TRAP_R8)
    CFI_OFFSET(r9, TRAP_R9)
    CFI_OFFSET(r10, TRAP_R10)
    CFI_OFFSET(r11, TRAP_R11)
    CFI_OFFSET(r12, TRAP_R12)
    CFI_OFFSET(sp, TRAP_USERSP)
    CFI_OFFSET(lr, TRAP_USERLR)
    CFI_OFFSET(pc, TRAP_PC)

    mov     %r0, %sp            @ Get the signal parameters.
    add     %r1, %sp, #SIGNAL_PARAMETERS_SIZE @ Get the signal context.
    bl      OspProcessSignal    @ The parameters are already set up. Just call.
    add     %sp, #SIGNAL_PARAMETERS_SIZE    @ Pop the signal parameters.
    CFI_ADJUST_CFA_OFFSET(-SIGNAL_PARAMETERS_SIZE)
    mov     %r1, %sp            @ Pass a pointer to the signal context.
    mov     %r0, #SystemCallRestoreContext  @ Set up the system call number.
    bl      OspSystemCallFull   @ Execute the system call to restore.
    DEBUGGER_BREAK              @ Execution should never get here.

END_FUNCTION OspSignalHandler

##
## PTHREAD_CONTROL_BLOCK
## OspGetThreadControlBlock (
##     VOID
##     )
##

/*++

Routine Description:

    This routine returns a pointer to the thread control block, a structure
    unique to each thread.

Arguments:

    None.

Return Value:

    Returns a pointer to the current thread's control block.

--*/

FUNCTION OspGetThreadControlBlock
    mrc     p15, 0, %r0, c13, c0, 3     @ Get the TPIDRURO register.
    bx      %lr                         @ Return.

END_FUNCTION OspGetThreadControlBlock

##
## VOID
## OspImArchResolvePltEntry (
##     VOID
##     )
##

/*++

Routine Description:

    This routine implements the slow path for a Procedure Linkable Table entry
    that has not yet been resolved to its target function address. This routine
    is only called once for each PLT entry, as subsequent calls jump directly
    to the destination function address.

Arguments:

    None.

Return Value:

    None. Control jumps directly to the destination function, rather than
    returning.

--*/

FUNCTION OspImArchResolvePltEntry
    CFI_OFFSET(lr, 0)

    ##
    ## Push the original arguments, and an extra register to keep the stack
    ## aligned (since the old lr was already pushed).
    ##

    stmdb   %sp!, {%r0-%r4}         @ Save the original arguments.
    CFI_ADJUST_CFA_OFFSET(20)

    ##
    ## Upon entry to the resolver:
    ## *sp = original lr.
    ## lr = &(GOT[2])
    ## ip = &(GOT[n + 3])
    ##

    ldr     %r0, [%lr, #-4]         @ Get GOT[1] for the context pointer.
    sub     %r1, %ip, %lr           @ &(GOT[n+3])-&(GOT[2]) = n + 1.
    asr     %r1, %r1, #2            @ Divide by 4 to get the actual index.
    sub     %r1, %r1, #1            @ Get rid of that extra one.
    bl      OspImResolvePltEntry    @ Resolve the relocation.
    mov     %ip, %r0                @ Save function in IP.
    ldmia   %sp!, {%r0-%r4,%lr}     @ Pop registers and previously pushed lr.
    CFI_ADJUST_CFA_OFFSET(-24)
    CFI_SAME_VALUE(lr)
    bx      %ip                     @ Jump to the function.

END_FUNCTION OspImArchResolvePltEntry

