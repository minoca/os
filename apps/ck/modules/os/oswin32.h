/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    oswin32.h

Abstract:

    This header contains an airlock between Windows headers and Minoca headers,
    since including windows.h conflicts with the types and definitions in
    minoca/lib/types.h.

Author:

    Evan Green 1-Feb-2017

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the maximum length of each of the strings in the system name
// structure.
//

#define SYSTEM_NAME_STRING_SIZE 80

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the buffer used to name the machine.

Members:

    SystemName - Stores a string describing the name of this implementation of
        the operating system.

    NodeName - Stores a string describing the name of this node within the
        communications network to which this node is attached, if any.

    Release - Stores a string containing the release level of this
        implementation.

    Version - Stores a string containing the version level of this release.

    Machine - Stores the name of the hardware type on which the system is
        running.

    DomainName - Stores the name of the network domain this machine resides in,
        if any.

--*/

typedef struct _SYSTEM_NAME {
    char SystemName[SYSTEM_NAME_STRING_SIZE];
    char NodeName[SYSTEM_NAME_STRING_SIZE];
    char Release[SYSTEM_NAME_STRING_SIZE];
    char Version[SYSTEM_NAME_STRING_SIZE];
    char Machine[SYSTEM_NAME_STRING_SIZE];
    char DomainName[SYSTEM_NAME_STRING_SIZE];
} SYSTEM_NAME, *PSYSTEM_NAME;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

int
CkpWin32GetSystemName (
    PSYSTEM_NAME Name
    );

/*++

Routine Description:

    This routine returns the name and version of the system.

Arguments:

    Name - Supplies a pointer where the name information will be returned.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

