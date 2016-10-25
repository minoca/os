/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    consio.h

Abstract:

    This header contains definitions for standard input and output in the
    debugger.

Author:

    Evan Green 30-Dec-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

INT
DbgrInitializeConsoleIo (
    PDEBUGGER_CONTEXT Context
    );

/*++

Routine Description:

    This routine initializes console I/O for the debugger.

Arguments:

    Context - Supplies a pointer to the debugger context.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

VOID
DbgrDestroyConsoleIo (
    PDEBUGGER_CONTEXT Context
    );

/*++

Routine Description:

    This routine destroys console I/O for the debugger.

Arguments:

    Context - Supplies a pointer to the debugger context.

Return Value:

    None.

--*/

