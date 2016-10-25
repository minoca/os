/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    info.c

Abstract:

    This module handles getting and setting system information calls.

Author:

    Chris Stevens 18-Jan-2015

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "spp.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
SppGetSetState (
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
SpGetSetSystemInformation (
    BOOL FromKernelMode,
    SP_INFORMATION_TYPE InformationType,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    )

/*++

Routine Description:

    This routine gets or sets system information.

Arguments:

    FromKernelMode - Supplies a boolean indicating whether or not this request
        (and the buffer associated with it) originates from user mode (FALSE)
        or kernel mode (TRUE).

    InformationType - Supplies the information type.

    Data - Supplies a pointer to the data buffer where the data is either
        returned for a get operation or given for a set operation.

    DataSize - Supplies a pointer that on input contains the size of the
        data buffer. On output, contains the required size of the data buffer.

    Set - Supplies a boolean indicating if this is a get operation (FALSE) or
        a set operation (TRUE).

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    switch (InformationType) {
    case SpInformationGetSetState:
        Status = SppGetSetState(Data, DataSize, Set);
        break;

    default:
        Status = STATUS_INVALID_PARAMETER;
        *DataSize = 0;
        break;
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
SppGetSetState (
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    )

/*++

Routine Description:

    This routine gets or sets the system profiler state.

Arguments:

    Data - Supplies a pointer to the data buffer where the data is either
        returned for a get operation or given for a set operation.

    DataSize - Supplies a pointer that on input contains the size of the
        data buffer. On output, contains the required size of the data buffer.

    Set - Supplies a boolean indicating if this is a get operation (FALSE) or
        a set operation (TRUE).

Return Value:

    Status code.

--*/

{

    ULONG ChangedFlags;
    ULONG DisableFlags;
    ULONG EnableFlags;
    PSP_GET_SET_STATE_INFORMATION Information;
    BOOL LockHeld;
    KSTATUS Status;

    Status = PsCheckPermission(PERMISSION_SYSTEM_ADMINISTRATOR);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    Information = Data;
    if (*DataSize < sizeof(SP_GET_SET_STATE_INFORMATION)) {
        *DataSize = sizeof(SP_GET_SET_STATE_INFORMATION);
        return STATUS_DATA_LENGTH_MISMATCH;
    }

    //
    // Return the current set of enabled flags on a get request.
    //

    Information = (PSP_GET_SET_STATE_INFORMATION)Data;
    LockHeld = FALSE;
    if (Set == FALSE) {
        Information->Operation = SpGetSetStateOperationNone;
        Information->ProfilerTypeFlags = SpEnabledFlags;

    } else if (Information->Operation != SpGetSetStateOperationNone) {
        DisableFlags = 0;
        EnableFlags = 0;
        KeAcquireQueuedLock(SpProfilingQueuedLock);
        LockHeld = TRUE;
        switch (Information->Operation) {
        case SpGetSetStateOperationOverwrite:
            ChangedFlags = SpEnabledFlags ^ Information->ProfilerTypeFlags;
            EnableFlags = ChangedFlags & ~SpEnabledFlags;
            DisableFlags = ChangedFlags & SpEnabledFlags;
            break;

        case SpGetSetStateOperationEnable:
            EnableFlags = Information->ProfilerTypeFlags & ~SpEnabledFlags;
            break;

        case SpGetSetStateOperationDisable:
            DisableFlags = Information->ProfilerTypeFlags & SpEnabledFlags;
            break;

        default:
            break;
        }

        if (DisableFlags != 0) {
            Status = SppStopSystemProfiler(DisableFlags);
            if (!KSUCCESS(Status)) {
                goto GetSetStateEnd;
            }
        }

        if (EnableFlags != 0) {
            Status = SppStartSystemProfiler(EnableFlags);
            if (!KSUCCESS(Status)) {
                goto GetSetStateEnd;
            }
        }

        KeReleaseQueuedLock(SpProfilingQueuedLock);
        LockHeld = FALSE;
    }

    Status = STATUS_SUCCESS;

GetSetStateEnd:
    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(SpProfilingQueuedLock);
    }

    return Status;
}

