/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/kdebug.h>

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

    Status = PsSignalAllProcesses(TRUE, SIGNAL_REQUEST_TERMINATION, NULL);
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

        PsSignalAllProcesses(TRUE, SIGNAL_KILL, NULL);
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
            RtlDebugPrint("Warning: Still %d processes alive after kill "
                          "signal!\n",
                          ProcessCount - 1);

            RtlDebugPrint("Data loss is possible. Proceeding with reset "
                          "anyway.\n");

            ASSERT(FALSE);
        }
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
    Status = HlResetSystem(ResetType, NULL, 0);
    KdConnect();
    RtlDebugPrint("System reset unsuccessful: %d\n", Status);
    return Status;
}

INTN
KeSysResetSystem (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine implements the system call for resetting the system.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This stores the system reset type. It is passed to the
        kernel in a register.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    SYSTEM_RESET_TYPE ResetType;
    KSTATUS Status;

    ResetType = (SYSTEM_RESET_TYPE)SystemCallParameter;

    //
    // Perform some validation here since the actual return status won't be
    // waited on by this thread.
    //

    if ((ResetType == SystemResetInvalid) ||
        (ResetType >= SystemResetTypeCount)) {

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
                                      (PVOID)ResetType);

SysResetSystemEnd:
    return Status;
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

