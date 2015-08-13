/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    b2709id.S

Abstract:

    This module implements support for getting the local unit ID for the
    BCM2709 interrupt controller.

Author:

    Chris Stevens 19-Apr-2015

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
## HlpBcm2709GetProcessorIdRegister (
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

FUNCTION HlpBcm2709GetProcessorIdRegister
    mrc     p15, 0, %r0, %c0, %c0, 5            @ Get the MPIDR
    bx      %lr                                 @

END_FUNCTION HlpBcm2709GetProcessorIdRegister

##
## --------------------------------------------------------- Internal Functions
##

