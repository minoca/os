/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    minocaos.c

Abstract:

    This module implements the Minoca operating system dependent portion of the
    Swiss common library.

Author:

    Chris Stevens 13-Aug-2014

Environment:

    User

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <errno.h>
#include <unistd.h>

#include <minoca/lib/minocaos.h>
#include <minoca/lib/mlibc.h>
#include "../swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

#define SWISS_ALLOCATION_TAG 0x73697753 // 'siwS'

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

PSWISS_PROCESS_INFORMATION
SwpCreateSwissProcessInformation (
    PPROCESS_INFORMATION OsProcessInformation
    );

time_t
SwpConvertSystemTimeToUnixTime (
    PSYSTEM_TIME SystemTime
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

int
SwGetProcessIdList (
    pid_t *ProcessIdList,
    size_t *ProcessIdListSize
    )

/*++

Routine Description:

    This routine returns a list of identifiers for the currently running
    processes.

Arguments:

    ProcessIdList - Supplies a pointer to an array of process IDs that is
        filled in by the routine. NULL can be supplied to determine the
        required size.

    ProcessIdListSize - Supplies a pointer that on input contains the size of
        the process ID list, in bytes. On output, it contains the actual size
        of the process ID list needed, in bytes.

Return Value:

    0 on success, or -1 on failure.

--*/

{

    UINTN Size;
    KSTATUS Status;

    Size = *ProcessIdListSize;
    Status = OsGetSetSystemInformation(SystemInformationPs,
                                       PsInformationProcessIdList,
                                       ProcessIdList,
                                       &Size,
                                       FALSE);

    *ProcessIdListSize = Size;
    if (!KSUCCESS(Status)) {
        return -1;
    }

    return 0;
}

int
SwGetProcessInformation (
    pid_t ProcessId,
    PSWISS_PROCESS_INFORMATION *ProcessInformation
    )

/*++

Routine Description:

    This routine gets process information for the specified process.

Arguments:

    ProcessId - Supplies the ID of the process whose information is to be
        gathered.

    ProcessInformation - Supplies a pointer that receives a pointer to process
        information structure. The caller is expected to free the buffer.

Return Value:

    0 on success, or -1 on failure.

--*/

{

    UINTN DataSize;
    PPROCESS_INFORMATION OsProcessInformation;
    PROCESS_INFORMATION OsProcessInformationBuffer;
    INT Result;
    KSTATUS Status;
    PSWISS_PROCESS_INFORMATION SwissProcessInformation;

    assert(sizeof(pid_t) == sizeof(PROCESS_ID));

    Result = 0;
    SwissProcessInformation = NULL;
    OsProcessInformationBuffer.Version = PROCESS_INFORMATION_VERSION;
    OsProcessInformationBuffer.ProcessId = ProcessId;
    OsProcessInformation = &OsProcessInformationBuffer;
    DataSize = sizeof(PROCESS_INFORMATION);
    Status = OsGetSetSystemInformation(SystemInformationPs,
                                       PsInformationProcess,
                                       OsProcessInformation,
                                       &DataSize,
                                       FALSE);

    if (!KSUCCESS(Status) && (Status != STATUS_BUFFER_TOO_SMALL)) {
        Result = -1;
        goto GetProcessInformationEnd;
    }

    //
    // If, for some reason, the stack-allocated process information structure
    // was big enough (e.g. no name or arguments), then copy it.
    //

    if (KSUCCESS(Status)) {
        SwissProcessInformation = SwpCreateSwissProcessInformation(
                                                         OsProcessInformation);

        if (SwissProcessInformation == NULL) {
            Result = -1;
        }

        goto GetProcessInformationEnd;
    }

    OsProcessInformation = OsHeapAllocate(DataSize, SWISS_ALLOCATION_TAG);
    if (OsProcessInformation == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto GetProcessInformationEnd;
    }

    OsProcessInformation->Version = PROCESS_INFORMATION_VERSION;
    OsProcessInformation->ProcessId = ProcessId;
    Status = OsGetSetSystemInformation(SystemInformationPs,
                                       PsInformationProcess,
                                       OsProcessInformation,
                                       &DataSize,
                                       FALSE);

    if (!KSUCCESS(Status)) {
        Result = -1;
        goto GetProcessInformationEnd;
    }

    SwissProcessInformation = SwpCreateSwissProcessInformation(
                                                         OsProcessInformation);

    if (SwissProcessInformation == NULL) {
        Result = -1;
        goto GetProcessInformationEnd;
    }

GetProcessInformationEnd:
    if ((OsProcessInformation != NULL) &&
        (OsProcessInformation != &OsProcessInformationBuffer)) {

        OsHeapFree(OsProcessInformation);
    }

    *ProcessInformation = SwissProcessInformation;
    return Result;
}

void
SwDestroyProcessInformation (
    PSWISS_PROCESS_INFORMATION ProcessInformation
    )

/*++

Routine Description:

    This routine destroys an allocated swiss process information structure.

Arguments:

    ProcessInformation - Supplies a pointer to the process informaiton to
        release.

Return Value:

    None.

--*/

{

    OsHeapFree(ProcessInformation);
    return;
}

int
SwCloseFrom (
    int Descriptor
    )

/*++

Routine Description:

    This routine closes all open file descriptors greater than or equal to
    the given descriptor.

Arguments:

    Descriptor - Supplies the minimum descriptor to close.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    return closefrom(Descriptor);
}

int
SwResetSystem (
    SWISS_REBOOT_TYPE RebootType
    )

/*++

Routine Description:

    This routine resets the running system.

Arguments:

    RebootType - Supplies the type of reboot to perform.

Return Value:

    0 if the reboot was successfully requested.

    Non-zero on error.

--*/

{

    SYSTEM_RESET_TYPE ResetType;
    KSTATUS Status;

    switch (RebootType) {
    case RebootTypeCold:
        ResetType = SystemResetCold;
        break;

    case RebootTypeWarm:
        ResetType = SystemResetWarm;
        break;

    case RebootTypeHalt:
        ResetType = SystemResetShutdown;
        break;

    default:
        return EINVAL;
    }

    Status = OsResetSystem(ResetType);
    if (!KSUCCESS(Status)) {
        return ClConvertKstatusToErrorNumber(Status);
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

PSWISS_PROCESS_INFORMATION
SwpCreateSwissProcessInformation (
    PPROCESS_INFORMATION OsProcessInformation
    )

/*++

Routine Description:

    This routine creates a swiss process information structure based on the
    provided process information.

Arguments:

    OsProcessInformation - Supplies a pointer to a Minoca OS process information
        structure.

Return Value:

    Returns a pointer to a newly allocated Swiss process information structure
    on success, or NULL on failure.

--*/

{

    size_t AllocationSize;
    PVOID Arguments;
    PVOID Buffer;
    ULONGLONG Frequency;
    ULONG Length;
    PSTR Name;
    ULONG Size;
    SYSTEM_TIME StartTime;
    PSWISS_PROCESS_INFORMATION SwissInformation;
    time_t Time;

    //
    // Allocate a Swiss process information structure.
    //

    AllocationSize = sizeof(SWISS_PROCESS_INFORMATION);
    if (OsProcessInformation->NameLength != 0) {
        AllocationSize += OsProcessInformation->NameLength * sizeof(CHAR);
    }

    if (OsProcessInformation->ArgumentsBufferSize != 0) {
        AllocationSize += OsProcessInformation->ArgumentsBufferSize;
    }

    SwissInformation = OsHeapAllocate(AllocationSize, SWISS_ALLOCATION_TAG);
    if (SwissInformation == NULL) {
        return NULL;
    }

    //
    // Convert the given Minoca OS process information structure to a Swiss
    // information structure.
    //

    RtlZeroMemory(SwissInformation, AllocationSize);
    SwissInformation->ProcessId = OsProcessInformation->ProcessId;
    SwissInformation->ParentProcessId = OsProcessInformation->ParentProcessId;
    SwissInformation->ProcessGroupId = OsProcessInformation->ProcessGroupId;
    SwissInformation->SessionId = OsProcessInformation->SessionId;
    SwissInformation->RealUserId = OsProcessInformation->RealUserId;
    SwissInformation->EffectiveUserId = OsProcessInformation->EffectiveUserId;
    SwissInformation->RealGroupId = OsProcessInformation->RealGroupId;
    SwissInformation->EffectiveGroupId = OsProcessInformation->EffectiveGroupId;
    SwissInformation->Priority = OsProcessInformation->Priority;
    SwissInformation->NiceValue = OsProcessInformation->NiceValue;
    SwissInformation->Flags = OsProcessInformation->Flags;

    //
    // Convert the process state.
    //

    switch (OsProcessInformation->State) {
    case ProcessStateReady:
    case ProcessStateRunning:
        SwissInformation->State = SwissProcessStateRunning;
        break;

    case ProcessStateBlocked:
        SwissInformation->State = SwissProcessStateUninterruptibleSleep;
        break;

    case ProcessStateSuspended:
        SwissInformation->State = SwissProcessStateInterruptibleSleep;
        break;

    case ProcessStateExited:
        SwissInformation->State = SwissProcessStateDead;
        break;

    case ProcessStateInvalid:
    default:
        SwissInformation->State = SwissProcessStateUnknown;
        break;
    }

    SwissInformation->ImageSize = OsProcessInformation->ImageSize;
    OsConvertTimeCounterToSystemTime(OsProcessInformation->StartTime,
                                     &StartTime);

    SwissInformation->StartTime = SwpConvertSystemTimeToUnixTime(&StartTime);
    Frequency = OsProcessInformation->Frequency;
    Time = OsProcessInformation->ResourceUsage.KernelCycles / Frequency;
    SwissInformation->KernelTime = Time;
    Time = OsProcessInformation->ResourceUsage.UserCycles / Frequency;
    SwissInformation->UserTime = Time;

    //
    // Copy the name and arguments buffer if they exist.
    //

    Buffer = (PVOID)(SwissInformation + 1);
    Length = OsProcessInformation->NameLength;
    if (Length != 0) {
        SwissInformation->Name = Buffer;
        SwissInformation->NameLength = Length;
        Name = (PSTR)OsProcessInformation;
        Name += OsProcessInformation->NameOffset;
        RtlStringCopy(SwissInformation->Name, Name, Length);
        Buffer += Length;
    }

    Size = OsProcessInformation->ArgumentsBufferSize;
    if (Size != 0) {
        SwissInformation->Arguments = Buffer;
        SwissInformation->ArgumentsSize = Size;
        Arguments = (PVOID)OsProcessInformation;
        Arguments += OsProcessInformation->ArgumentsBufferOffset;
        RtlCopyMemory(SwissInformation->Arguments, Arguments, Size);
        Buffer += Size;
    }

    return SwissInformation;
}

time_t
SwpConvertSystemTimeToUnixTime (
    PSYSTEM_TIME SystemTime
    )

/*++

Routine Description:

    This routine converts the given system time structure into a time_t time
    value. Fractional seconds are truncated.

Arguments:

    SystemTime - Supplies a pointer to the system time structure.

Return Value:

    Returns the time_t value corresponding to the given system time.

--*/

{

    LONGLONG AdjustedSeconds;
    time_t Result;

    AdjustedSeconds = SystemTime->Seconds + SYSTEM_TIME_TO_EPOCH_DELTA;
    Result = (time_t)AdjustedSeconds;
    return Result;
}

