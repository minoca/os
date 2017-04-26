/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    entry.S

Abstract:

    This module implements the entry point for the PC/AT boot manager.

Author:

    Evan Green 21-Feb-2014

Environment:

    Kernel mode

--*/

##
## ------------------------------------------------------------------ Includes
##

#include <minoca/kernel/x86.inc>

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
##     VOID
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
    movl    $__bss_start, %edi      # Get the start of the BSS section.
    movl    $_end, %ecx             # Get the end address.
    subl    %edi, %ecx              # Subtract the start to get the BSS size.
    xorl    %eax, %eax              # The byte to write is zero.
    cld                             # Count up.
    rep stosb                       # Store zero bytes to EDI, ECX times.
    jmp     BmPcatApplicationMain   # Jump to the real function.

##
## --------------------------------------------------------- Internal Functions
##

