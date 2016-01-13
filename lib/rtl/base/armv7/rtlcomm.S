/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    rtlcomm.S

Abstract:

    This module implements ARM processor architecture features not
    implementable in C that are common amongst different versions of ARM.

Author:

    Chris Stevens 18-Mar-2014

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
## VOID
## RtlDebugBreak (
##     VOID
##     )
##

/*++

Routine Description:

    This routine causes a break into the debugger.

Arguments:

    None.

Return Value:

    None.

--*/

FUNCTION RtlDebugBreak
    DEBUGGER_BREAK                              @ Debugger break.
    bx  %lr                                     @ Return.

END_FUNCTION RtlDebugBreak

##
## VOID
## RtlDebugService (
##     UINTN ServiceRequest,
##     PVOID Parameter
##     )
##

/*++

Routine Description:

    This routine enters the debugger for a service request.

Arguments:

    ServiceRequest - Supplies the reason for entering the debugger.

    Parameter - Supplies the parameter to pass to the debug service routine.

Return Value:

    None.

--*/

FUNCTION RtlDebugService
    DEBUG_SERVICE
    bx      %lr                                 @ Return.

END_FUNCTION RtlDebugService

##
## --------------------------------------------------------- Internal Functions
##

