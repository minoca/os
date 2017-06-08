/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    entry.S

Abstract:

    This module implements the entry point for the loader.

Author:

    Evan Green 18-Aug-2013

Environment:

    Kernel mode

--*/

##
## ------------------------------------------------------------------ Includes
##

##
## ---------------------------------------------------------------------- Code
##

##
## .text specifies that this code belongs in the executable section.
##

.text

##
## Stick this in the .init section so it ends up at the front of the binary.
##

.section .init

##
## .globl allows this label to be visible to the linker.
##

.globl _start

##
## void
## start (
##     )
##

/*++

Routine Description:

    This routine implements the entry point for the loader. It jumps to the
    main C routine.

Arguments:

    None.

Return Value:

    TRUE if interrupts are enabled in the processor.

    FALSE if interrupts are globally disabled.

--*/

_start:
    jmp     BoMain                  # Just do it.

##
## --------------------------------------------------------- Internal Functions
##

