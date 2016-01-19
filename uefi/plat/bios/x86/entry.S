/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

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

#include <minoca/x86.inc>

##
## ---------------------------------------------------------------------- Code
##

##
## .text specifies that this code belongs in the executable section.
##
## .code32 specifies that this is 32-bit protected mode code.
##

.text
.code32

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

    This routine implements the entry point for the firmware. It jumps to the
    main C routine. It is called by the file system specific boot code.

Arguments:

    None.

Return Value:

    TRUE if interrupts are enabled in the processor.

    FALSE if interrupts are globally disabled.

--*/

_start:
    jmp     EfiBiosMain             # Just do it.

##
## --------------------------------------------------------- Internal Functions
##

