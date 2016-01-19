/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    fenva.S

Abstract:

    This module implements assembly support functionality for ARM floating
    point (VFP).

Author:

    Evan Green 11-Nov-2015

Environment:

    User Mode C Library

--*/

##
## ------------------------------------------------------------------- Includes
##

#include <minoca/arm.inc>

##
## ---------------------------------------------------------------- Definitions
##

##
## ----------------------------------------------------------------------- Code
##

ASSEMBLY_FILE_HEADER
.fpu vfpv3

##
## unsigned int
## ClpGetFpscr (
##     void
##     )
##

/*++

Routine Description:

    This routine returns the VFP floating point status and control register
    (FPSCR).

Arguments:

    None.

Return Value:

    Returns the FPSCR.

--*/

FUNCTION ClpGetFpscr
    vmrs    %r0, FPSCR
    bx      %lr

END_FUNCTION ClpGetFpscr

##
## void
## ClpSetFpscr (
##     unsigned int Value
##     )
##

/*++

Routine Description:

    This routine sets the VFP floating point status and control register
    (FPSCR).

Arguments:

    Value - Supplies the new value to set.

Return Value:

    None.

--*/

FUNCTION ClpSetFpscr
    vmsr    FPSCR, %r0
    bx      %lr

END_FUNCTION ClpSetFpscr

