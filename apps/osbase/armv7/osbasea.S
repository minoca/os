/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

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

#include <minoca/arm.inc>

##
## --------------------------------------------------------------- Definitions
##

##
## ----------------------------------------------------------------------- Code
##

ASSEMBLY_FILE_HEADER

##
## VOID
## OspSystemCallFull (
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

    None.

--*/

FUNCTION OspSystemCallFull
    swi     #0x0                    @ Perform system call.
    bx      %lr                     @ Return.

END_FUNCTION OspSystemCallFull

##
## VOID
## OspSignalHandler (
##     )
##

/*++

Routine Description:

    This routine is called directly by the kernel when a signal occurs. It
    marshals the parameters and calls the C routine for handling the signal.

Arguments:

    None. The parameters are stored in registers, but do not conform to any C
        calling convention.

Return Value:

    None.

--*/

FUNCTION OspSignalHandler
    stmdb   %sp!, {%r4}         @ Push the fifth parameter onto the stack.
    bl      OspProcessSignal    @ The parameters are already set up. Just call.
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

