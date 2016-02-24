/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    archsup.S

Abstract:

    This module architecture specific support routines.

Author:

    Evan Green 7-Apr-2014

Environment:

    Firmware

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
## VOID
## EfiMemoryBarrier (
##     VOID
##     )
##

/*++

Routine Description:

    This routine provides a full memory barrier, ensuring that all memory
    accesses occurring before this function complete before any memory accesses
    after this function start.

Arguments:

    None.

Return Value:

    None.

--*/

FUNCTION(EfiMemoryBarrier)
    sub     $4, %esp
    lock add $1, (%esp)
    add     $4, %esp
    ret

END_FUNCTION(EfiMemoryBarrier)

##
## --------------------------------------------------------- Internal Functions
##

