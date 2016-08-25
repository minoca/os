/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    syscall.c

Abstract:

    This module implements support infrastructure for system calls in the OS
    base library.

Author:

    Evan Green 11-Nov-2014

Environment:

    User Mode

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "../osbasep.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

INTN
OspSysenterSystemCall (
    ULONG SystemCallNumber,
    PVOID SystemCallParameter
    );

//
// -------------------------------------------------------------------- Globals
//

POS_SYSTEM_CALL OsSystemCall = OspSystemCallFull;

//
// ------------------------------------------------------------------ Functions
//

VOID
OspSetUpSystemCalls (
    VOID
    )

/*++

Routine Description:

    This routine sets up the system call handler.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PUSER_SHARED_DATA UserData;

    UserData = OspGetUserSharedData();
    if ((UserData->ProcessorFeatures & X86_FEATURE_SYSENTER) != 0) {
        OsSystemCall = OspSysenterSystemCall;
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

