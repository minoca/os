/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    user.c

Abstract:

    This module implement user functionality for the Chalk OS module.

Author:

    Evan Green 2-Aug-2017

Environment:

    C

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// This is needed to get setresuid.
//

#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "osp.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// MacOS doesn't have a "saved" user/group ID.
//

#if defined(__APPLE__)

#define setresuid(_Real, _Effective, _Saved) setreuid(_Real, _Effective)
#define setresgid(_Real, _Effective, _Saved) setregid(_Real, _Effective)

#endif

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
CkpOsGetuid (
    PCK_VM Vm
    );

VOID
CkpOsGetgid (
    PCK_VM Vm
    );

VOID
CkpOsGeteuid (
    PCK_VM Vm
    );

VOID
CkpOsGetegid (
    PCK_VM Vm
    );

VOID
CkpOsSeteuid (
    PCK_VM Vm
    );

VOID
CkpOsSetegid (
    PCK_VM Vm
    );

VOID
CkpOsSetresuid (
    PCK_VM Vm
    );

VOID
CkpOsSetresgid (
    PCK_VM Vm
    );

VOID
CkpOsGetpid (
    PCK_VM Vm
    );

//
// -------------------------------------------------------------------- Globals
//

CK_VARIABLE_DESCRIPTION CkOsUserValues[] = {
    {CkTypeFunction, "getuid", CkpOsGetuid, 0},
    {CkTypeFunction, "getgid", CkpOsGetgid, 0},
    {CkTypeFunction, "geteuid", CkpOsGeteuid, 0},
    {CkTypeFunction, "getegid", CkpOsGetegid, 0},
    {CkTypeFunction, "seteuid", CkpOsSeteuid, 1},
    {CkTypeFunction, "setegid", CkpOsSetegid, 1},
    {CkTypeFunction, "setresuid", CkpOsSetresuid, 3},
    {CkTypeFunction, "setresgid", CkpOsSetresgid, 3},
    {CkTypeFunction, "getpid", CkpOsGetpid, 0},
    {CkTypeInvalid, NULL, NULL, 0}
};

//
// ------------------------------------------------------------------ Functions
//

VOID
CkpOsGetuid (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine implements the getuid call. It takes no arguments, and returns
    the users real user ID. On Windows, returns 0 for a privileged account and
    1000 for a regular user account.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    Returns the real user id.

--*/

{

    CkReturnInteger(Vm, getuid());
    return;
}

VOID
CkpOsGetgid (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine implements the getgid call. It takes no arguments, and returns
    the users real group ID. On Windows, returns 0 for a privileged account and
    1000 for a regular user account.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    Returns the real group id.

--*/

{

    CkReturnInteger(Vm, getgid());
    return;
}

VOID
CkpOsGeteuid (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine implements the geteuid call. It takes no arguments, and returns
    the users effective user ID. On Windows, returns 0 for a privileged account
    and 1000 for a regular user account.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    Returns the effective user id.

--*/

{

    CkReturnInteger(Vm, geteuid());
    return;
}

VOID
CkpOsGetegid (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine implements the getegid call. It takes no arguments, and returns
    the users effective group ID. On Windows, returns 0 for a privileged
    account and 1000 for a regular user account.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    Returns the effective group id.

--*/

{

    CkReturnInteger(Vm, getegid());
    return;
}

VOID
CkpOsSeteuid (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine implements the seteuid call. It takes one argument, the new
    effective user ID to set. Returns 0 on success, or raises an exception on
    failure.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    Returns 0 on success, or raises an exception on failure.

--*/

{

    if (!CkCheckArguments(Vm, 1, CkTypeInteger)) {
        return;
    }

    if (seteuid(CkGetInteger(Vm, 1)) != 0) {
        CkpOsRaiseError(Vm, NULL);
        return;
    }

    CkReturnInteger(Vm, 0);
    return;
}

VOID
CkpOsSetegid (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine implements the setegid call. It takes one argument, the new
    effective group ID to set. Returns 0 on success, or raises an exception on
    failure.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    Returns 0 on success, or raises an exception on failure.

--*/

{

    if (!CkCheckArguments(Vm, 1, CkTypeInteger)) {
        return;
    }

    if (setegid(CkGetInteger(Vm, 1)) != 0) {
        CkpOsRaiseError(Vm, NULL);
        return;
    }

    CkReturnInteger(Vm, 0);
    return;
}

VOID
CkpOsSetresuid (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine implements the setresuid call. It takes three arguments: the
    real, effective, and saved user IDs to set. Returns 0 on success, or
    raises an exception on failure.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    Returns 0 on success, or raises an exception on failure.

--*/

{

    int Result;

    if (!CkCheckArguments(Vm, 3, CkTypeInteger, CkTypeInteger, CkTypeInteger)) {
        return;
    }

    Result = setresuid(CkGetInteger(Vm, 1),
                       CkGetInteger(Vm, 2),
                       CkGetInteger(Vm, 3));

    if (Result != 0) {
        CkpOsRaiseError(Vm, NULL);
        return;
    }

    CkReturnInteger(Vm, 0);
    return;
}

VOID
CkpOsSetresgid (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine implements the setresgid call. It takes three arguments: the
    real, effective, and saved group IDs to set. Returns 0 on success, or
    raises an exception on failure.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    Returns 0 on success, or raises an exception on failure.

--*/

{

    int Result;

    if (!CkCheckArguments(Vm, 3, CkTypeInteger, CkTypeInteger, CkTypeInteger)) {
        return;
    }

    Result = setresgid(CkGetInteger(Vm, 1),
                       CkGetInteger(Vm, 2),
                       CkGetInteger(Vm, 3));

    if (Result != 0) {
        CkpOsRaiseError(Vm, NULL);
        return;
    }

    CkReturnInteger(Vm, 0);
    return;
}

VOID
CkpOsGetpid (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine returns the current process identifier.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    This routine does not return. The process exits.

--*/

{

    CkReturnInteger(Vm, getpid());
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

