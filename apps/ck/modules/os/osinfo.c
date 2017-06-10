/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    osinfo.c

Abstract:

    This module provides information about the underlying OS in the os module.

Author:

    Evan Green 31-Jan-2017

Environment:

    C

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "osp.h"

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
    {CkTypeInteger, "isUnix", NULL, CK_IS_UNIX},
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

    struct utsname Utsname;

    if (uname(&Utsname) >= 0) {
        CkPushString(Vm, Utsname.sysname, strlen(Utsname.sysname));
        CkSetVariable(Vm, 0, "system");
        CkPushString(Vm, Utsname.version, strlen(Utsname.version));
        CkSetVariable(Vm, 0, "version");
        CkPushString(Vm, Utsname.release, strlen(Utsname.release));
        CkSetVariable(Vm, 0, "release");
        CkPushString(Vm, Utsname.machine, strlen(Utsname.machine));
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

    PSTR String;
    struct utsname UtsName;

    if (uname(&UtsName) < 0) {
        CkpOsRaiseError(Vm);
        return;
    }

    String = UtsName.nodename;
    CkReturnString(Vm, String, strlen(String));
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

    CHAR Line[256];

    if (getdomainname(Line, sizeof(Line)) < 0) {
        CkpOsRaiseError(Vm);
        return;
    }

    CkReturnString(Vm, Line, strlen(Line));
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

