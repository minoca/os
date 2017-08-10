/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

OS_API
INTN
OsForkProcess (
    ULONG Flags,
    PVOID FrameRestoreBase
    )

/*++

Routine Description:

    This routine forks the current process into two separate processes. The
    child process begins executing in the middle of this function.

Arguments:

    Flags - Supplies a bitfield of flags governing the behavior of the newly
        forked process. See FORK_FLAG_* definitions.

    FrameRestoreBase - Supplies an optional pointer to a region of recent
        stack. On vfork operations, the kernel will copy the stack region from
        the supplied pointer up to the current stack pointer into a temporary
        buffer. After the child execs or exits, the kernel will copy that
        region back into the parent process' stack. This is needed so that the
        stack can be used in between the C library and the final system call.

Return Value:

    In the child, returns 0 indicating success.

    In the parent, returns the process ID of the child on success, which is
    always a positive value.

    On failure, returns a KSTATUS code, which is a negative value.

--*/

{

    SYSTEM_CALL_FORK Parameters;
    INTN Result;

    //
    // Perform a full system call to avoid the need to save/restore the
    // non-volatiles.
    //

    Parameters.Flags = Flags;
    Parameters.FrameRestoreBase = FrameRestoreBase;
    Result = OspSystemCallFull(SystemCallForkProcess, &Parameters);
    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

