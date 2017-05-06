/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    crt0.S

Abstract:

    This module implements the very first code that every C application runs.

Author:

    Evan Green 4-Mar-2013

Environment:

    User Mode (Program startup)

--*/

##
## ------------------------------------------------------------------- Includes
##

#include <minoca/kernel/arm.inc>

##
## ---------------------------------------------------------------- Definitions
##

##
## ----------------------------------------------------------------------- Code
##

ASSEMBLY_FILE_HEADER

##
## VOID
## _start (
##     VOID
##     )
##

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

EXPORTED_FUNCTION _start
    CFI_UNDEFINED(lr)

    ##
    ## Call out to initialize the C Runtime library.
    ##

    bl      ClApplicationInitialize

    ##
    ## Execution should never get here.
    ##

    DEBUGGER_BREAK

    ##
    ## Create an undefined reference to main. There needs to be a reference to
    ## main early because if main is defined in a library the linker will
    ## throw it out if it thinks it's not needed. This call is never actually
    ## executed.
    ##

    blx     main

END_FUNCTION _start

