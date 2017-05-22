/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    jsonp.h

Abstract:

    This header contains internal definitions for the Chalk JSON module.

Author:

    Evan Green 19-May-2017

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>
#include <minoca/lib/chalk.h>

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the maximum nesting level supported for JSON data. This is arbitrary.
//

#define CK_JSON_MAX_RECURSION 5000

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

VOID
CkpJsonModuleInit (
    PCK_VM Vm
    );

/*++

Routine Description:

    This routine populates the JSON module namespace.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

VOID
CkpJsonEncode (
    PCK_VM Vm
    );

/*++

Routine Description:

    This routine implements the entry point into the JSON encoder. It takes
    two arguments: the object to encode, and the amount to indent objects by.
    If indent is less than or equal to zero, then the object will be encoded
    with no whitespace.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    Returns a string containing the JSON representation of the object on
    success.

    Raises an exception on failure.

--*/

VOID
CkpJsonDecode (
    PCK_VM Vm
    );

/*++

Routine Description:

    This routine implements the entry point into the JSON decoder. It takes
    one argument which can be in one of two forms. It can either be a JSON
    string to decode, or a list whose first element contains a JSON string to
    decode. In list form, a substring containing the remaining data will be
    returned in the list's first element. It returns the deserialized object,
    or raises an exception.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    Returns the deserialized object on success.

    Raises an exception on failure.

--*/

