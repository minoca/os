/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    osbasea.S

Abstract:

    This module implements assembly support for the OS Base library.

Author:

    Evan Green 17-Jan-2015

Environment:

    User Mode

--*/

##
## ------------------------------------------------------------------- Includes
##

#include <minoca/x64.inc>

##
## ---------------------------------------------------------------- Definitions
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

    This routine executes a system call using the traditional "int x" method.
    This method is highly compatible, but slow.

Arguments:

    SystemCallNumber - Supplies the system call number.

    SystemCallParameter - Supplies the system call parameter.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

FUNCTION(OspSystemCallFull)
    syscall                     # Just do that system call, params in rdi, rsi.
    ret                         # Return.

END_FUNCTION(OspSystemCallFull)

##
## VOID
## OspSignalHandler (
##     )
##

/*++

Routine Description:

    This routine is called directly by the kernel when a signal occurs. It
    marshals the parameters and calls the C routine for handling the signal.
    The parameters are stored on the stack with the signal parameters followed
    by the signal context.

Arguments:

    None. The parameters are stored in registers, but do not conform to any C
        calling convention.

Return Value:

    None.

--*/

FUNCTION(OspSignalHandler)
    call    OspProcessSignal    # Call the processing routine.
    int     $3                  # Execution should never get back here.

END_FUNCTION(OspSignalHandler)

