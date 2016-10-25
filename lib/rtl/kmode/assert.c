/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    assert.c

Abstract:

    This module implements assertions for the kernel runtime library.

Author:

    Evan Green 25-Jul-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "rtlp.h"
#include <minoca/kernel/arch.h>
#include <minoca/kernel/hmod.h>
#include <minoca/kernel/kdebug.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

RTL_API
VOID
RtlRaiseAssertion (
    PCSTR Expression,
    PCSTR SourceFile,
    ULONG SourceLine
    )

/*++

Routine Description:

    This routine raises an assertion failure exception. If a debugger is
    connected, it will attempt to connect to the debugger.

Arguments:

    Expression - Supplies the string containing the expression that failed.

    SourceFile - Supplies the string describing the source file of the failure.

    SourceLine - Supplies the source line number of the failure.

Return Value:

    None.

--*/

{

    RtlDebugPrint("\n\n *** Assertion Failure: %s\n *** File: %s, Line %d\n\n",
                  Expression,
                  SourceFile,
                  SourceLine);

    RtlDebugService(EXCEPTION_ASSERTION_FAILURE, NULL);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//
