/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    gicid.S

Abstract:

    This module implements support for getting the local unit ID for the GIC
    interrupt controller.

Author:

    Evan Green 14-Jul-2013

Environment:

    Kernel mode

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
## ULONG
## HlpGicGetProcessorIdRegister (
##     VOID
##     )
##

/*++

Routine Description:

    This routine retrieves the MPIDR (MultiProcessor ID register).

Arguments:

    None.

Return Value:

    Returns the value of the MPIDR.

--*/

FUNCTION HlpGicGetProcessorIdRegister
    mrc     p15, 0, %r0, %c0, %c0, 5            @ Get the MPIDR
    bx      %lr                                 @

END_FUNCTION HlpGicGetProcessorIdRegister

##
## --------------------------------------------------------- Internal Functions
##


