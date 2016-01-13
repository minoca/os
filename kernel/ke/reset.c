/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    reset.c

Abstract:

    This module implements support for rebooting the system.

Author:

    Evan Green 16-Apr-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel.h>
#include <minoca/kdebug.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the amount of time to wait for processes to end after a signal was
// sent to them, in seconds.
//

#define RESET_SYSTEM_PROCESS_SIGNAL_TIMEOUT 30

//
// Define the amount of time to wait between checking the process count to see
// if all processes have exited, in microseconds.
//

#define RESET_SYSTEM_SIGNAL_POLL_INTERVAL (20 * MICROSECONDS_PER_MILLISECOND)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
KepSysResetSystemWorkItem (
    PVOID Parameter
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KERNEL_API
KSTATUS
KeResetSystem (
    SYSTEM_RESET_TYPE ResetType
    )

/*++

Routine Description:

    This routine attempts to reboot the system. This routine must be called
    from low level.

Arguments:

    ResetType - Supplies the desired system reset type. If the given type is
        not supported and a cold reset is, then a cold reset will be
        performed.

Return Value:

    Does not return on success, the system is reset.

    STATUS_INVALID_PARAMETER if an invalid reset type was supplied.

    STATUS_NOT_SUPPORTED if the system cannot be reset.

    STATUS_UNSUCCESSFUL if the system did not reset.

--*/

{

    PSTR Description;
    ULONG FinalProcessCount;
    ULONGLONG Frequency;
    ULONG ProcessCount;
    KSTATUS Status;
    ULONGLONG Timeout;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Frequency = HlQueryTimeCounterFrequency();
    switch (ResetType) {
    case SystemResetWarm:
        Description = "warm reset";
        break;

    case SystemResetShutdown:
        Description = "shutdown";
        break;

    case SystemResetCold:
        Description = "cold reset";
        break;

    default:

        ASSERT(FALSE);

        return STATUS_INVALID_PARAMETER;
    }

    //
    // Send all processes a polite termination request.
    //

    RtlDebugPrint("System going down for %s. "
                  "Sending all processes a termination signal...\n",
                  Description);

    Status = PsSignalAllProcesses(SIGNAL_REQUEST_TERMINATION);
    if (KSUCCESS(Status)) {
        Timeout = KeGetRecentTimeCounter() +
                  (Frequency * RESET_SYSTEM_PROCESS_SIGNAL_TIMEOUT);

        //
        // Wait for the number of processes to drop to one (just the kernel
        // process).
        //

        do {
            ProcessCount = PsGetProcessCount();
            if (ProcessCount <= 1) {
                break;
            }

            KeDelayExecution(TRUE, FALSE, RESET_SYSTEM_SIGNAL_POLL_INTERVAL);

        } while (KeGetRecentTimeCounter() <= Timeout);
    }

    ProcessCount = PsGetProcessCount();
    if (ProcessCount != 1) {
        RtlDebugPrint("Still %d processes alive. Sending kill signal...\n",
                      ProcessCount - 1);

        PsSignalAllProcesses(SIGNAL_KILL);
    }

    Timeout = KeGetRecentTimeCounter() +
              (Frequency * RESET_SYSTEM_PROCESS_SIGNAL_TIMEOUT);

    //
    // Wait for the number of processes to drop to one (just the kernel
    // process).
    //

    do {
        ProcessCount = PsGetProcessCount();
        if (ProcessCount <= 1) {
            break;
        }

        KeDelayExecution(TRUE, FALSE, RESET_SYSTEM_SIGNAL_POLL_INTERVAL);

    } while (KeGetRecentTimeCounter() <= Timeout);

    ProcessCount = PsGetProcessCount();
    if (ProcessCount != 1) {
        RtlDebugPrint("Warning: Still %d processes alive after kill signal!\n",
                      ProcessCount - 1);

        RtlDebugPrint("Data loss is possible. Proceeding with reset anyway.\n");

        ASSERT(FALSE);
    }

    Status = IoFlush(INVALID_HANDLE, 0, 0, FLUSH_FLAG_ALL_SYNCHRONOUS);
    if (!KSUCCESS(Status)) {
        RtlDebugPrint("Warning: Flush failure!\n");
        RtlDebugPrint("Data loss is possible. Proceeding with reset anyway.\n");

        ASSERT(FALSE);
    }

    //
    // Do a final check to make sure no processes sprung up.
    //

    if (ProcessCount <= 1) {
        FinalProcessCount = PsGetProcessCount();
        if (FinalProcessCount != 1) {
            RtlDebugPrint("Warning: Process count increased to %d after kill "
                          "signal was sent!\n",
                          FinalProcessCount - 1);

            ASSERT(FALSE);
        }
    }

    KdDisconnect();
    Status = HlResetSystem(ResetType);
    KdConnect();
    RtlDebugPrint("System reset unsuccessful: %x\n", Status);
    return Status;
}

VOID
KeSysResetSystem (
    ULONG SystemCallNumber,
    PVOID SystemCallParameter,
    PTRAP_FRAME TrapFrame,
    PULONG ResultSize
    )

/*++

Routine Description:

    This routine implements the system call for resetting the system.

Arguments:

    SystemCallNumber - Supplies the system call number that was requested.

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

    TrapFrame - Supplies a pointer to the trap frame generated by this jump
        from user mode to kernel mode.

    ResultSize - Supplies a pointer where the system call routine returns the
        size of the parameter structure to be copied back to user mode. The
        value returned here must be no larger than the original parameter
        structure size. The default is the original size of the parameters.

Return Value:

    None.

--*/

{

    PSYSTEM_CALL_RESET_SYSTEM Parameters;
    KSTATUS Status;

    ASSERT(SystemCallNumber == SystemCallResetSystem);

    Parameters = SystemCallParameter;

    //
    // Perform some validation here since the actual return status won't be
    // waited on by this thread.
    //

    if ((Parameters->ResetType == SystemResetInvalid) ||
        (Parameters->ResetType >= SystemResetTypeCount)) {

        Status = STATUS_INVALID_PARAMETER;
        goto SysResetSystemEnd;
    }

    Status = PsCheckPermission(PERMISSION_REBOOT);
    if (!KSUCCESS(Status)) {
        goto SysResetSystemEnd;
    }

    Status = KeCreateAndQueueWorkItem(NULL,
                                      WorkPriorityNormal,
                                      KepSysResetSystemWorkItem,
                                      (PVOID)(Parameters->ResetType));

SysResetSystemEnd:
    Parameters->Status = Status;
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
KepSysResetSystemWorkItem (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine implements the work item used to get the reset system call off
    of a user mode thread.

Arguments:

    Parameter - Supplies a parameter that in this case represents the actual
        reset type itself.

Return Value:

    None.

--*/

{

    SYSTEM_RESET_TYPE ResetType;

    ResetType = (SYSTEM_RESET_TYPE)Parameter;
    KeResetSystem(ResetType);
    return;
}

