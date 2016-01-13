/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    rtlarch.S

Abstract:

    This module implements ARMv7 processor architecture features not
    implementable in C for the boot environment.

Author:

    Chris Stevens 18-Mar-2014

Environment:

    Boot

--*/

##
## ------------------------------------------------------------------ Includes
##

#include <minoca/arm.inc>

##
## --------------------------------------------------------------- Definitions
##

##
## ---------------------------------------------------------------------- Code
##

ASSEMBLY_FILE_HEADER

##
## VOID
## RtlMemoryBarrier (
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

FUNCTION RtlMemoryBarrier
    dmb
    bx      %lr

END_FUNCTION RtlMemoryBarrier

##
## --------------------------------------------------------- Internal Functions
##

