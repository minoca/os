/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    wininfo.c

Abstract:

    This module provides information about the underlying OS in the os module
    on a Windows machine.

Author:

    Evan Green 31-Jan-2017

Environment:

    C

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <string.h>

#include "osp.h"
#include "oswin32.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
CkpOsHostname (
    PCK_VM Vm
    );

VOID
CkpOsDomainName (
    PCK_VM Vm
    );

//
// -------------------------------------------------------------------- Globals
//

CK_VARIABLE_DESCRIPTION CkOsInfoModuleValues[] = {
    {CkTypeInteger, "isUnix", NULL, 0},
    {CkTypeFunction, "getHostname", CkpOsHostname, 0},
    {CkTypeFunction, "getDomainName", CkpOsDomainName, 0},
    {CkTypeInvalid, NULL, NULL, 0}
};

//
// ------------------------------------------------------------------ Functions
//

VOID
CkpOsInitializeInfo (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine initializes the OS information functions and globals.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    SYSTEM_NAME SystemName;

    if (CkpWin32GetSystemName(&SystemName) == 0) {
        CkPushString(Vm, SystemName.SystemName, strlen(SystemName.SystemName));
        CkSetVariable(Vm, 0, "system");
        CkPushString(Vm, SystemName.Release, strlen(SystemName.Release));
        CkSetVariable(Vm, 0, "release");
        CkPushString(Vm, SystemName.Version, strlen(SystemName.Version));
        CkSetVariable(Vm, 0, "version");
        CkPushString(Vm, SystemName.Machine, strlen(SystemName.Machine));
        CkSetVariable(Vm, 0, "machine");
    }

    CkDeclareVariables(Vm, 0, CkOsInfoModuleValues);
    return;
}

VOID
CkpOsHostname (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine takes no arguments, and returns the hostname.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    SYSTEM_NAME SystemName;

    if (CkpWin32GetSystemName(&SystemName) == 0) {
        CkReturnString(Vm, SystemName.NodeName, strlen(SystemName.NodeName));

    } else {
        CkReturnString(Vm, "", 0);
    }

    return;
}

VOID
CkpOsDomainName (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine takes no arguments, and returns the domain name of the system.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    SYSTEM_NAME SystemName;

    if (CkpWin32GetSystemName(&SystemName) == 0) {
        CkReturnString(Vm,
                       SystemName.DomainName,
                       strlen(SystemName.DomainName));

    } else {
        CkReturnString(Vm, "", 0);
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

