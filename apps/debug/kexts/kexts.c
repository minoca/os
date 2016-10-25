/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    kexts.c

Abstract:

    This module implements kernel debugger extensions.

Author:

    Evan Green 10-Sep-2012

Environment:

    Debug Client

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>
#include <minoca/lib/status.h>
#include <minoca/lib/rtl.h>
#include <minoca/debug/dbgext.h>
#include "memory.h"
#include "objects.h"
#include "threads.h"
#include "acpiext.h"
#include "reslist.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

INT
ExtensionMain (
    PDEBUGGER_CONTEXT Context,
    ULONG ExtensionApiVersion,
    PVOID Token
    )

/*++

Routine Description:

    This routine defines the extension's main routine. This routine will get
    called when the extension is loaded. It is responsible for registering the
    debugger extensions it supports.

Arguments:

    Context - Supplies the application instance context. This must be passed
        into the registration routines.

    ExtensionApiVersion - Supplies the revision of the debugger extension API.

    Token - Supplies a token that uniquely idenfies the extension. This is used
        when registering extensions.

Return Value:

    0 on success.

    Returns an error code on failure. The extension will be unloaded if it
    returns non-zero.

--*/

{

    PSTR Extension;
    PSTR OneLineDescription;
    INT Status;
    INT TotalStatus;

    TotalStatus = 0;
    Extension = "mdl";
    OneLineDescription = "Print the contents of a Memory Descriptor List.";
    Status = DbgRegisterExtension(Context,
                                  Token,
                                  Extension,
                                  OneLineDescription,
                                  ExtMdl);

    if (Status != 0) {
        DbgOut("Error: Unable to register %s.\n", Extension);
        TotalStatus = Status;
    }

    Extension = "object";
    OneLineDescription = "Print the contents of a kernel Object.";
    Status = DbgRegisterExtension(Context,
                                  Token,
                                  Extension,
                                  OneLineDescription,
                                  ExtObject);

    if (Status != 0) {
        DbgOut("Error: Unable to register %s.\n", Extension);
        TotalStatus = Status;
    }

    Extension = "thread";
    OneLineDescription = "Prints the contents of a thread object.";
    Status = DbgRegisterExtension(Context,
                                  Token,
                                  Extension,
                                  OneLineDescription,
                                  ExtThread);

    if (Status != 0) {
        DbgOut("Error: Unable to register %s.\n", Extension);
        TotalStatus = Status;
    }

    Extension = "acpi";
    OneLineDescription = "Provides help debugging ACPI issues.";
    Status = DbgRegisterExtension(Context,
                                  Token,
                                  Extension,
                                  OneLineDescription,
                                  ExtAcpi);

    if (Status != 0) {
        DbgOut("Error: Unable to register %s.\n", Extension);
        TotalStatus = Status;
    }

    Extension = "res";
    OneLineDescription = "Prints resource allocations, requirements, and "
                         "lists.";

    Status = DbgRegisterExtension(Context,
                                  Token,
                                  Extension,
                                  OneLineDescription,
                                  ExtResource);

    if (Status != 0) {
        DbgOut("Error: Unable to register %s.\n", Extension);
        TotalStatus = Status;
    }

    return TotalStatus;
}

//
// --------------------------------------------------------- Internal Functions
//

