/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    json.c

Abstract:

    This module implements the JSON Chalk C module.

Author:

    Evan Green 19-May-2017

Environment:

    C

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "jsonp.h"

//
// --------------------------------------------------------------------- Macros
//

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

CK_VARIABLE_DESCRIPTION CkJsonModuleValues[] = {
    {CkTypeFunction, "dumps", CkpJsonEncode, 2},
    {CkTypeFunction, "loads", CkpJsonDecode, 1},
    {CkTypeInvalid, NULL, NULL, 0}
};

//
// ------------------------------------------------------------------ Functions
//

BOOL
CkPreloadJsonModule (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine preloads the JSON module. It is called to make the presence of
    the json module known in cases where the module is statically linked.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    return CkPreloadForeignModule(Vm, "json", NULL, NULL, CkpJsonModuleInit);
}

VOID
CkpJsonModuleInit (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine populates the JSON module namespace.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    CkDeclareVariables(Vm, 0, CkJsonModuleValues);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

