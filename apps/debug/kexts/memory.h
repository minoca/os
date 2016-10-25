/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    memory.h

Abstract:

    This header contains definitions for Memory Management related debugger
    extensions.

Author:

    Evan Green 10-Sep-2012

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
ExtMdl (
    PDEBUGGER_CONTEXT Context,
    PSTR Command,
    ULONG ArgumentCount,
    PSTR *ArgumentValues
    );

/*++

Routine Description:

    This routine prints out the contents of a Memory Descriptor List. Arguments
    to the extension are:

        Address - Supplies the address of the MDL.

Arguments:

    Context - Supplies a pointer to the debugger applicaton context, which is
        an argument to most of the API functions.

    Command - Supplies the subcommand entered. This parameter is unused.

    ArgumentCount - Supplies the number of arguments in the ArgumentValues
        array.

    ArgumentValues - Supplies the values of each argument. This memory will be
        reused when the function returns, so extensions must not touch this
        memory after returning from this call.

Return Value:

    0 if the debugger extension command was successful.

    Returns an error code if a failure occurred along the way.

--*/

