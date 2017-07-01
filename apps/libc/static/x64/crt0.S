/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    crt0.S

Abstract:

    This module implements the very first code that every C application runs.

Author:

    Evan Green 18-Jan-2015

Environment:

    User Mode (Program startup)

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ----------------------------------------------------------------------- Code
//

//
// .text specifies that this code belongs in the executable section.
//

.text

//
// .globl allows this label to be visible to the linker.
//

.globl _start

//
// VOID
// start (
//     VOID
//     )
//

/*++

Routine Description:

    This routine is the default entry point for a user mode program in the ELF
    binary format. It is responsible for initializing the C library and
    calling main.

Arguments:

    None.

Return Value:

    None.

--*/

_start:

    //
    // Call out to initialize the C Runtime library. The kernel puts the
    // thread parameter both on the stack and in ECX.
    //

    jmp     ClApplicationInitialize

    //
    // Execution should never get here.
    //

    int $3
    ret

