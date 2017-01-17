/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    info.c

Abstract:

    This module implements support for handling process subsystem information
    requests.

Author:

    Chris Stevens 13-Aug-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "psp.h"

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
PspGetSetProcessInformation (
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

KSTATUS
PspGetProcessIdListInformation (
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
PsGetSetSystemInformation (
    BOOL FromKernelMode,
    PS_INFORMATION_TYPE InformationType,
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
    case PsInformationProcess:
        Status = PspGetSetProcessInformation(Data, DataSize, Set);
        break;

    case PsInformationProcessIdList:
        Status = PspGetProcessIdListInformation(Data, DataSize, Set);
        break;

    case PsInformationHostName:
    case PsInformationDomainName:
        Status = PspGetSetUtsInformation(FromKernelMode,
                                         InformationType,
                                         Data,
                                         DataSize,
                                         Set);

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
PspGetSetProcessInformation (
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    )

/*++

Routine Description:

    This routine gets or sets process informaiton.

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

    PKPROCESS Process;
    PROCESS_ID ProcessId;
    PPROCESS_INFORMATION ProcessInformation;
    KSTATUS Status;

    if (Set != FALSE) {
        *DataSize = 0;
        return STATUS_ACCESS_DENIED;
    }

    if (Data == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Check the version and size of the supplied data structure.
    //

    if (*DataSize < sizeof(ULONG)) {
        *DataSize = sizeof(PROCESS_INFORMATION);
        return STATUS_BUFFER_TOO_SMALL;
    }

    ProcessInformation = Data;
    if (ProcessInformation->Version < PROCESS_INFORMATION_VERSION) {
        return STATUS_VERSION_MISMATCH;
    }

    if (*DataSize < sizeof(PROCESS_INFORMATION)) {
        *DataSize = sizeof(PROCESS_INFORMATION);
        return STATUS_BUFFER_TOO_SMALL;
    }

    //
    // Get the process ID of the process whose information is to be retrieved.
    //

    ProcessId = ProcessInformation->ProcessId;
    if (ProcessId == -1ULL) {
        Process = PsGetCurrentProcess();
        ProcessId = Process->Identifiers.ProcessId;
    }

    //
    // TODO: Make sure the caller has access to this process's information.
    //

    //
    // Get the process information, or at least the necessary size of the
    // process information.
    //

    Status = PsGetProcessInformation(ProcessId, Data, DataSize);
    return Status;
}

KSTATUS
PspGetProcessIdListInformation (
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    )

/*++

Routine Description:

    This routine gets the list of process identifiers for processes currently
    running on the system.

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

    KSTATUS Status;

    if (Set != FALSE) {
        *DataSize = 0;
        return STATUS_ACCESS_DENIED;
    }

    //
    // Attempt to get the full list of IDs for the currently running processes.
    //

    Status = PspGetProcessIdList(Data, DataSize);
    if (!KSUCCESS(Status)) {
        goto GetProcessIdListInformationEnd;
    }

    Status = STATUS_SUCCESS;

GetProcessIdListInformationEnd:
    return Status;
}

