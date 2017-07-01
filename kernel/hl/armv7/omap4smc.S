/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    omap4smc.S

Abstract:

    This module implements assembly routines necessary for initializing the
    OMAP4.

Author:

    Chris Stevens 14-Jan-2014

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
// ULONG
// HlpOmap4SmcCommand (
//     ULONG Argument1,
//     ULONG Argument2,
//     ULONG Command
//     )
//

/*++

Routine Description:

    This routine executes an SMC command up to secure mode. The OMAP4 defines
    a software interface for non-secure software to request certain services
    using the SMC command.

Arguments:

    Argument1 - Supplies the 32-bit value to put into R0. This is dependent on
        which command is issued.

    Argument2 - Supplies the 32-bit value to put into R1. This is also
        dependent on the command issued.

    Command - Supplies the SMC command number to issue.

Return Value:

    Returns the result as passed back from the SMC command.

--*/

FUNCTION HlpOmap4SmcCommand
    stmdb   sp!, {%r4-%r12, %lr}    @ Save registers.
    mov     %r12, %r2               @ Move command into place.
    DSB                             @ Data synchronization barrier.
    smc     #0                      @ Execute the SMC instruction.
    ldmia   sp!, {%r4-%r12, %lr}    @ Restore registers.
    bx      %lr

END_FUNCTION HlpOmap4SmcCommand
