/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    process.c

Abstract:

    This module implements support routines for processes in the kernel.

Author:

    Evan Green 6-Aug-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "psp.h"
#include <minoca/debug/dbgproto.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the number of times to try and get the process list.
//

#define PROCESS_LIST_TRIES 100

//
// Define the fudge factor to add to the reported allocation to account for
// new processes sneaking in between calls.
//

#define PROCESS_LIST_FUDGE_FACTOR 2

//
// Define the maximum process name length including the null terminator.
// Process names are the decimal representation of the process ID, which is
// a ULONG.
//

#define MAX_PROCESS_NAME_LENGTH 11

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
PspDestroyProcess (
    PVOID Object
    );

VOID
PspProcessChildrenOfTerminatingProcess (
    PKPROCESS Process
    );

VOID
PspLoaderThread (
    PVOID Context
    );

KSTATUS
PspLoadExecutable (
    PCSTR BinaryName,
    PIMAGE_FILE_INFORMATION File,
    PIMAGE_BUFFER Buffer,
    PPROCESS_START_DATA StartData
    );

VOID
PspHandleTableLookupCallback (
    PHANDLE_TABLE HandleTable,
    HANDLE Descriptor,
    PVOID HandleValue
    );

KSTATUS
PspDebugEnable (
    PKPROCESS Process,
    PKPROCESS TracingProcess
    );

KSTATUS
PspDebugPrint (
    PKPROCESS Process,
    PPROCESS_DEBUG_COMMAND Command
    );

KSTATUS
PspDebugIssueCommand (
    PKPROCESS IssuingProcess,
    PKPROCESS TargetProcess,
    PPROCESS_DEBUG_COMMAND Command
    );

KSTATUS
PspCreateDebugDataIfNeeded (
    PKPROCESS Process
    );

VOID
PspDestroyDebugData (
    PPROCESS_DEBUG_DATA DebugData
    );

VOID
PspDebugGetLoadedModules (
    PSYSTEM_CALL_DEBUG Command
    );

VOID
PspDebugGetThreadList (
    PSYSTEM_CALL_DEBUG Command
    );

KSTATUS
PspGetAllProcessInformation (
    PVOID Buffer,
    PUINTN BufferSize
    );

KSTATUS
PspGetProcessInformation (
    PKPROCESS Process,
    PPROCESS_INFORMATION Buffer,
    PUINTN BufferSize
    );

VOID
PspGetProcessTimes (
    PKPROCESS Process,
    PULONGLONG UserTime,
    PULONGLONG KernelTime,
    PULONGLONG ChildrenUserTime,
    PULONGLONG ChildrenKernelTime
    );

VOID
PspGetProcessResourceUsage (
    PKPROCESS Process,
    BOOL IncludeProcess,
    BOOL IncludeChildren,
    PRESOURCE_USAGE Usage
    );

VOID
PspReadResourceUsage (
    PRESOURCE_USAGE Destination,
    PRESOURCE_USAGE Source
    );

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

POBJECT_HEADER PsProcessDirectory = NULL;
PQUEUED_LOCK PsProcessListLock;
LIST_ENTRY PsProcessListHead;
ULONG PsProcessCount;
volatile PROCESS_ID PsNextProcessId = 0;
PKPROCESS PsKernelProcess;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
PsGetAllProcessInformation (
    ULONG AllocationTag,
    PVOID *Buffer,
    PUINTN BufferSize
    )

/*++

Routine Description:

    This routine returns information about the active processes in the system.

Arguments:

    AllocationTag - Supplies the allocation tag to use for the allocation
        this routine will make on behalf of the caller.

    Buffer - Supplies a pointer where a non-paged pool buffer will be returned
        containing the array of process information. The caller is responsible
        for freeing this memory from non-paged pool. The type returned here
        will be an array (where each element may be a different size) of
        PROCESS_INFORMATION structures.

    BufferSize - Supplies a pointer where the size of the buffer in bytes
        will be returned on success.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES if memory could not be allocated for the
    information buffer.

    STATUS_BUFFER_TOO_SMALL if the process list is so volatile that it cannot
    be sized. This is only returned in extremely rare cases, as this routine
    makes multiple attempts.

--*/

{

    PVOID Allocation;
    UINTN Size;
    KSTATUS Status;
    ULONG Try;

    Allocation = NULL;
    Size = 0;
    Status = STATUS_BUFFER_TOO_SMALL;
    for (Try = 0; Try < PROCESS_LIST_TRIES; Try += 1) {
        Status = PspGetAllProcessInformation(Allocation, &Size);
        if (KSUCCESS(Status)) {
            break;
        }

        if (Status != STATUS_BUFFER_TOO_SMALL) {
            goto GetAllProcessInformationEnd;
        }

        ASSERT(Size != 0);

        if (Allocation != NULL) {
            MmFreeNonPagedPool(Allocation);
            Allocation = NULL;
        }

        Size = Size * PROCESS_LIST_FUDGE_FACTOR;
        Allocation = MmAllocateNonPagedPool(Size, AllocationTag);
        if (Allocation == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto GetAllProcessInformationEnd;
        }
    }

GetAllProcessInformationEnd:
    if (!KSUCCESS(Status)) {
        if (Allocation != NULL) {
            MmFreeNonPagedPool(Allocation);
            Allocation = NULL;
        }

        Size = 0;
    }

    *Buffer = Allocation;
    *BufferSize = Size;
    return Status;
}

KSTATUS
PsGetProcessInformation (
    PROCESS_ID ProcessId,
    PPROCESS_INFORMATION Buffer,
    PUINTN BufferSize
    )

/*++

Routine Description:

    This routine returns information about a given process.

Arguments:

    ProcessId - Supplies the ID of the process to get the information for.

    Buffer - Supplies an optional pointer to a buffer to write the data into.
        This buffer must be non-paged.

    BufferSize - Supplies a pointer that on input contains the size of the
        input buffer. On output, returns the size needed to contain the data.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NO_SUCH_PROCESS if the given process ID does not correspond to any
    known process.

    STATUS_BUFFER_TOO_SMALL if a buffer was supplied but was not big enough to
    contain all the information.

--*/

{

    PKPROCESS Process;
    KSTATUS Status;

    Process = PspGetProcessById(ProcessId);
    if (Process == NULL) {
        return STATUS_NO_SUCH_PROCESS;
    }

    Status = PspGetProcessInformation(Process, Buffer, BufferSize);
    ObReleaseReference(Process);
    return Status;
}

KSTATUS
PsGetProcessIdentity (
    PROCESS_ID ProcessId,
    PTHREAD_IDENTITY Identity
    )

/*++

Routine Description:

    This routine gets the identity of the process, which is simply that of
    an arbitrary thread in the process.

Arguments:

    ProcessId - Supplies the ID of the process to get the information for.

    Identity - Supplies a pointer where the process identity will be returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NO_SUCH_PROCESS if the given process ID does not correspond to any
    known process.

--*/

{

    PKPROCESS Process;
    KSTATUS Status;

    Process = PspGetProcessById(ProcessId);
    if (Process == NULL) {
        return STATUS_NO_SUCH_PROCESS;
    }

    Status = PspGetProcessIdentity(Process, Identity);
    ObReleaseReference(Process);
    return Status;
}

INTN
PsSysForkProcess (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine duplicates the current process, including all allocated
    address space and open file handles. Only the current thread's execution
    continues in the new process.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    Process ID of the child on success (a positive integer).

    Error status code on failure (a negative integer).

--*/

{

    PKTHREAD CurrentThread;
    PKPROCESS NewProcess;
    INTN NewProcessId;
    PSYSTEM_CALL_FORK Parameters;
    KSTATUS Status;

    CurrentThread = KeGetCurrentThread();
    NewProcess = NULL;
    Parameters = (PSYSTEM_CALL_FORK)SystemCallParameter;
    Status = PspCopyProcess(CurrentThread->OwningProcess,
                            CurrentThread,
                            CurrentThread->TrapFrame,
                            Parameters->Flags,
                            &NewProcess);

    if (!KSUCCESS(Status)) {
        RtlDebugPrint("Failed to fork %d\n", Status);
        return Status;
    }

    NewProcessId = NewProcess->Identifiers.ProcessId;
    ObReleaseReference(NewProcess);

    //
    // Yield to the child. This alleviates extra work during image section
    // isolation that the parent must do if it triggers copy-on-write before
    // the child. Plus, in the majority of cases, the forking parent is just
    // going to wait on its new child.
    //

    KeYield();
    return NewProcessId;
}

INTN
PsSysExecuteImage (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine duplicates the current process, including all allocated
    address space and open file handles. Only the current thread's execution
    continues in the new process.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    The architecture-specific return register from the reset thread context on
    success. This is necessary because the architecture-specific system call
    assembly routines do not restore the return register out of the trap frame
    in order to allow a system call to return a value via a register. If an
    architecture does not need to pass anything to the new thread in its return
    register, then it should return 0.

    Error status code on failure.

--*/

{

    IMAGE_BUFFER Buffer;
    PSTR CurrentDirectory;
    UINTN CurrentDirectorySize;
    IMAGE_FILE_INFORMATION File;
    CHAR FirstCharacter;
    IMAGE_FORMAT Format;
    PPROCESS_ENVIRONMENT NewEnvironment;
    PSTR NewName;
    UINTN NewNameSize;
    PPROCESS_ENVIRONMENT OldEnvironment;
    PSTR OverrideName;
    UINTN OverrideNameSize;
    PSYSTEM_CALL_EXECUTE_IMAGE Parameters;
    BOOL PastPointOfNoReturn;
    PKPROCESS Process;
    INTN ReturnValue;
    PROCESS_START_DATA StartData;
    KSTATUS Status;
    PKTHREAD Thread;

    RtlZeroMemory(&Buffer, sizeof(IMAGE_BUFFER));
    File.Handle = INVALID_HANDLE;
    NewEnvironment = NULL;
    Parameters = (PSYSTEM_CALL_EXECUTE_IMAGE)SystemCallParameter;
    PastPointOfNoReturn = FALSE;
    ReturnValue = 0;
    Thread = KeGetCurrentThread();
    Process = Thread->OwningProcess;
    OverrideName = NULL;
    OverrideNameSize = 0;
    CurrentDirectory = NULL;

    ASSERT(Process != PsGetKernelProcess());

    //
    // Fail if there are more than one threads running.
    //

    if (Process->ThreadCount != 1) {
        RtlDebugPrint("Failing an exec with >1 threads.\n");
        Status = STATUS_INVALID_CONFIGURATION;
        goto SysExecuteProcessEnd;
    }

    //
    // Check to see if the image name is a relative path. If so, create an
    // absolute path and pass that as an override to copy environment.
    //

    if (Parameters->Environment.ImageNameLength != 0) {
        Status = MmCopyFromUserMode(&FirstCharacter,
                                    Parameters->Environment.ImageName,
                                    sizeof(CHAR));

        if (!KSUCCESS(Status)) {
            goto SysExecuteProcessEnd;
        }

        if (FirstCharacter != PATH_SEPARATOR) {
            Status = IoGetCurrentDirectory(TRUE,
                                           FALSE,
                                           &CurrentDirectory,
                                           &CurrentDirectorySize);

            if (!KSUCCESS(Status)) {
                goto SysExecuteProcessEnd;
            }

            OverrideNameSize = CurrentDirectorySize +
                               Parameters->Environment.ImageNameLength;

            OverrideName = MmAllocatePagedPool(OverrideNameSize,
                                               PS_ALLOCATION_TAG);

            if (OverrideName == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto SysExecuteProcessEnd;
            }

            //
            // Copy the current directory up to but not including the NULL
            // terminator. Set '/' where the NULL terminator using the space
            // for the NULL terminator and then copy in the image name from
            // user mode, which includes a NULL terminator.
            //

            RtlCopyMemory(OverrideName,
                          CurrentDirectory,
                          CurrentDirectorySize - 1);

            OverrideName[CurrentDirectorySize - 1] = PATH_SEPARATOR;
            MmCopyFromUserMode(OverrideName + CurrentDirectorySize,
                               Parameters->Environment.ImageName,
                               Parameters->Environment.ImageNameLength);
        }
    }

    //
    // Create the new environment in kernel mode.
    //

    Status = PsCopyEnvironment(&(Parameters->Environment),
                               &NewEnvironment,
                               TRUE,
                               NULL,
                               OverrideName,
                               OverrideNameSize);

    if (!KSUCCESS(Status)) {
        goto SysExecuteProcessEnd;
    }

    //
    // Check to see if the destination image exists.
    //

    Status = ImGetExecutableFormat(NewEnvironment->ImageName,
                                   Process,
                                   &File,
                                   &Buffer,
                                   &Format);

    if (!KSUCCESS(Status)) {
        goto SysExecuteProcessEnd;
    }

    //
    // Close everything marked for "close on execute".
    //

    Status = IoCloseHandlesOnExecute(Process);
    if (!KSUCCESS(Status)) {
        goto SysExecuteProcessEnd;
    }

    //
    // Destroy all timers.
    //

    PspDestroyProcessTimers(Process);

    //
    // Unload all images and free all memory associated with this image.
    // Blocked and ignored signals are inherited across the exec. Handled
    // signals are reset to the default.
    //

    Process->SignalHandlerRoutine = NULL;
    INITIALIZE_SIGNAL_SET(Process->HandledSignals);
    PspSetThreadUserStackSize(Thread, 0);
    PspImUnloadAllImages(Process);
    MmCleanUpProcessMemory(Process);
    NewName = RtlStringFindCharacterRight(NewEnvironment->ImageName,
                                          '/',
                                          NewEnvironment->ImageNameLength);

    if (NewName != NULL) {
        NewName += 1;
        NewNameSize = NewEnvironment->ImageNameLength -
                      ((UINTN)NewName -
                       (UINTN)(NewEnvironment->ImageName));

    } else {
        NewName = NewEnvironment->ImageName;
        NewNameSize = NewEnvironment->ImageNameLength;
    }

    //
    // Transfer the environment carefully as process information queries may
    // be looking at it.
    //

    OldEnvironment = Process->Environment;
    KeAcquireQueuedLock(Process->QueuedLock);
    Process->Environment = NewEnvironment;
    Process->BinaryName = NewName;
    Process->BinaryNameSize = NewNameSize;

    //
    // Mark that the process has executed an image.
    //

    Process->Flags |= PROCESS_FLAG_EXECUTED_IMAGE;
    KeReleaseQueuedLock(Process->QueuedLock);
    PsDestroyEnvironment(OldEnvironment);
    NewEnvironment = NULL;
    PastPointOfNoReturn = TRUE;

    //
    // Reinitialize the user accounting structure, which may still have old
    // unmapped reservations in it.
    //

    Status = MmReinitializeUserAccounting(Process->AddressSpace->Accountant);
    if (!KSUCCESS(Status)) {
        goto SysExecuteProcessEnd;
    }

    //
    // Remap the user shared data page.
    //

    Status = MmMapUserSharedData(Process->AddressSpace);
    if (!KSUCCESS(Status)) {
        goto SysExecuteProcessEnd;
    }

    //
    // Perform security context changes for the new executable.
    //

    PspPerformExecutePermissionChanges(File.Handle);

    //
    // Load up the new image.
    //

    Status = PspLoadExecutable(Process->Environment->ImageName,
                               &File,
                               &Buffer,
                               &StartData);

    if (!KSUCCESS(Status)) {
        RtlDebugPrint("Failed to exec %s: %d.\n",
                      Process->Environment->ImageName,
                      Status);

        goto SysExecuteProcessEnd;
    }

    File.Handle = INVALID_HANDLE;
    Process->Environment->StartData = &StartData;
    Thread->ThreadRoutine = StartData.EntryPoint;

    //
    // Reset the thread in preparation for execution.
    //

    Status = PspResetThread(Thread, Thread->TrapFrame, &ReturnValue);
    Process->Environment->StartData = NULL;
    if (!KSUCCESS(Status)) {
        goto SysExecuteProcessEnd;
    }

    //
    // If the process is being traced, send a trap signal to the tracer process.
    //

    if ((Process->DebugData != NULL) &&
        (Process->DebugData->TracingProcess != NULL)) {

        PsSignalProcess(Process, SIGNAL_TRAP, NULL);
    }

    Status = STATUS_SUCCESS;

SysExecuteProcessEnd:
    if (File.Handle != INVALID_HANDLE) {
        IoClose(File.Handle);
    }

    if (CurrentDirectory != NULL) {
        MmFreePagedPool(CurrentDirectory);
    }

    if (OverrideName != NULL) {
        MmFreePagedPool(OverrideName);
    }

    if (!KSUCCESS(Status)) {
        if (PastPointOfNoReturn != FALSE) {
            PspSetProcessExitStatus(Process,
                                    CHILD_SIGNAL_REASON_KILLED,
                                    SIGNAL_BUS_ERROR);

            PsSignalProcess(Process, SIGNAL_KILL, NULL);
        }

        //
        // On failure, the status code should be the return value.
        //

        ReturnValue = Status;
    }

    if (NewEnvironment != NULL) {
        PsDestroyEnvironment(NewEnvironment);
    }

    return ReturnValue;
}

INTN
PsSysGetSetProcessId (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine gets or sets identifiers associated with the calling process.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or the requested process ID on success.

    Error status code on failure.

--*/

{

    PKPROCESS OtherProcess;
    PSYSTEM_CALL_GET_SET_PROCESS_ID Parameters;
    PKPROCESS Process;
    PROCESS_GROUP_ID ProcessGroupId;
    INTN Result;
    KSTATUS Status;
    PKTHREAD Thread;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Parameters = (PSYSTEM_CALL_GET_SET_PROCESS_ID)SystemCallParameter;
    Thread = KeGetCurrentThread();
    Process = Thread->OwningProcess;
    Status = STATUS_SUCCESS;

    //
    // Only a few types of IDs can be set.
    //

    if (Parameters->Set != FALSE) {
        switch (Parameters->ProcessIdType) {
        case ProcessIdProcessGroup:
            ProcessGroupId = Parameters->NewValue;
            if ((Parameters->ProcessId == 0) ||
                (Parameters->ProcessId == Process->Identifiers.ProcessId)) {

                if (ProcessGroupId == 0) {
                    ProcessGroupId = Process->Identifiers.ProcessId;
                }

                Status = PspJoinProcessGroup(Process, ProcessGroupId, FALSE);

            } else {
                if (ProcessGroupId == 0) {
                    ProcessGroupId = Parameters->ProcessId;
                }

                KeAcquireQueuedLock(Process->QueuedLock);
                OtherProcess = PspGetChildProcessById(Process,
                                                      Parameters->ProcessId);

                KeReleaseQueuedLock(Process->QueuedLock);
                if (OtherProcess == NULL) {
                    Status = STATUS_NO_SUCH_PROCESS;

                } else {

                    ASSERT(OtherProcess->Parent == Process);

                    Status = PspJoinProcessGroup(OtherProcess,
                                                 ProcessGroupId,
                                                 FALSE);

                    ObReleaseReference(OtherProcess);
                }
            }

            break;

        case ProcessIdSession:
            if (Parameters->ProcessId == 0) {
                Status = PspJoinProcessGroup(Process,
                                             Process->Identifiers.ProcessId,
                                             TRUE);

            } else {
                Status = STATUS_INVALID_PARAMETER;
            }

            break;

        default:
            Status = STATUS_INVALID_PARAMETER;
            break;
        }

        Result = Status;

    //
    // The caller wants to get an ID.
    //

    } else {
        switch (Parameters->ProcessIdType) {
        case ProcessIdProcess:
            Result = Process->Identifiers.ProcessId;
            break;

        case ProcessIdThread:
            Result = Thread->ThreadId;
            break;

        case ProcessIdProcessGroup:
        case ProcessIdSession:
            if ((Parameters->ProcessId == 0) ||
                (Parameters->ProcessId == Process->Identifiers.ProcessId)) {

                if (Parameters->ProcessIdType == ProcessIdProcessGroup) {
                    Result = Process->Identifiers.ProcessGroupId;

                } else {
                    Result = Process->Identifiers.SessionId;
                }

            } else {
                OtherProcess = PspGetProcessById(Parameters->ProcessId);
                if (OtherProcess == NULL) {
                    Result = STATUS_NO_SUCH_PROCESS;

                } else {

                    //
                    // If the found process doesn't yet have a process group or
                    // left its process group, then act like it wasn't found.
                    // It is either on its way in or on its way out.
                    //

                    if (OtherProcess->ProcessGroup != NULL) {
                        if (Parameters->ProcessIdType ==
                            ProcessIdProcessGroup) {

                            Result = OtherProcess->Identifiers.ProcessGroupId;

                        } else {
                            Result = OtherProcess->Identifiers.SessionId;
                        }

                    } else {
                        Result = STATUS_NO_SUCH_PROCESS;
                    }

                    ObReleaseReference(OtherProcess);
                }
            }

            break;

        case ProcessIdParentProcess:
            Result = Process->Identifiers.ParentProcessId;
            break;

        default:
            Result = STATUS_INVALID_PARAMETER;
            break;
        }
    }

    return Result;
}

INTN
PsSysDebug (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine implements the user mode debug interface.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    PKPROCESS CurrentProcess;
    PKPROCESS Parent;
    PSYSTEM_CALL_DEBUG Request;
    KSTATUS Status;
    PKPROCESS TargetProcess;

    CurrentProcess = PsGetCurrentProcess();
    Request = (PSYSTEM_CALL_DEBUG)SystemCallParameter;
    TargetProcess = NULL;
    switch (Request->Command.Command) {
    case DebugCommandEnableDebugging:
        KeAcquireQueuedLock(CurrentProcess->QueuedLock);
        Parent = CurrentProcess->Parent;
        if (Parent != NULL) {
            ObAddReference(Parent);
        }

        KeReleaseQueuedLock(CurrentProcess->QueuedLock);
        if (Parent == NULL) {
            Request->Command.Status = STATUS_TOO_LATE;

        } else {
            Request->Command.Status = PspDebugEnable(CurrentProcess, Parent);
            ObReleaseReference(Parent);
        }

        break;

    case DebugCommandPrint:
        Request->Command.Status = PspDebugPrint(CurrentProcess,
                                                &(Request->Command));

        break;

    case DebugCommandReportModuleChange:
        if (Request->Command.Size < sizeof(PROCESS_DEBUG_MODULE_CHANGE)) {
            Request->Command.Size = sizeof(PROCESS_DEBUG_MODULE_CHANGE);
            Request->Command.Status = STATUS_DATA_LENGTH_MISMATCH;

        } else {
            Request->Command.Status =
                         PspProcessUserModeModuleChange(Request->Command.Data);
        }

        break;

    case DebugCommandContinue:
    case DebugCommandReadMemory:
    case DebugCommandWriteMemory:
    case DebugCommandSwitchThread:
    case DebugCommandGetBreakInformation:
    case DebugCommandSetBreakInformation:
    case DebugCommandGetSignalInformation:
    case DebugCommandSetSignalInformation:
    case DebugCommandSingleStep:
    case DebugCommandRangeStep:

        //
        // First, look up the process.
        //

        TargetProcess = PspGetProcessById(Request->Process);
        if ((TargetProcess == NULL) ||
            (TargetProcess->DebugData == NULL) ||
            (TargetProcess->DebugData->TracingProcess != CurrentProcess)) {

            Request->Command.Status = STATUS_INVALID_PARAMETER;
            goto SysDebugEnd;
        }

        Status = PspDebugIssueCommand(CurrentProcess,
                                      TargetProcess,
                                      &(Request->Command));

        if (!KSUCCESS(Status)) {
            Request->Command.Status = Status;
            goto SysDebugEnd;
        }

        break;

    case DebugCommandGetLoadedModules:
        PspDebugGetLoadedModules(Request);
        break;

    case DebugCommandGetThreadList:
        PspDebugGetThreadList(Request);
        break;

    default:

        //
        // The user mode debugger asked for something the kernel doesn't
        // understand.
        //

        ASSERT(FALSE);

        Request->Command.Status = STATUS_INVALID_PARAMETER;
        break;
    }

SysDebugEnd:
    if (TargetProcess != NULL) {
        ObReleaseReference(TargetProcess);
    }

    return Request->Command.Status;
}

INTN
PsSysExitProcess (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine terminates the current process.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This stores the exit status for the process. It is
        passed to the kernel in a register.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    PKPROCESS Process;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Process = PsGetCurrentProcess();

    ASSERT(Process != PsGetKernelProcess());

    PspSetProcessExitStatus(Process,
                            CHILD_SIGNAL_REASON_EXITED,
                            (UINTN)SystemCallParameter);

    PsSignalProcess(Process, SIGNAL_KILL, NULL);
    return STATUS_SUCCESS;
}

INTN
PsSysGetResourceUsage (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine returns the resource usage for a process or thread.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    PKPROCESS CurrentProcess;
    PSYSTEM_CALL_GET_RESOURCE_USAGE Parameters;
    PKPROCESS Process;
    KSTATUS Status;
    PKTHREAD Thread;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Parameters = (PSYSTEM_CALL_GET_RESOURCE_USAGE)SystemCallParameter;
    if ((Parameters->Request == ResourceUsageRequestProcess) ||
        (Parameters->Request == ResourceUsageRequestProcessChildren)) {

        CurrentProcess = PsGetCurrentProcess();
        if ((Parameters->Id == -1ULL) ||
            (Parameters->Id == CurrentProcess->Identifiers.ProcessId)) {

            Process = CurrentProcess;
            ObAddReference(Process);

        } else {

            //
            // If the process is not a direct child of the caller, then the
            // caller must have the resources permission.
            //

            Process = PspGetProcessById(Parameters->Id);
            if ((Process == NULL) || (Process->Parent != CurrentProcess)) {
                Status = PsCheckPermission(PERMISSION_RESOURCES);
                if (!KSUCCESS(Status)) {
                    goto GetResourceUsageEnd;
                }
            }
        }

        if (Process == NULL) {
            Status = STATUS_NO_SUCH_PROCESS;
            goto GetResourceUsageEnd;
        }

        if (Parameters->Request == ResourceUsageRequestProcess) {
            PspGetProcessResourceUsage(Process,
                                       TRUE,
                                       FALSE,
                                       &(Parameters->Usage));

        } else {
            PspGetProcessResourceUsage(Process,
                                       FALSE,
                                       TRUE,
                                       &(Parameters->Usage));

        }

        ObReleaseReference(Process);

    } else if (Parameters->Request == ResourceUsageRequestThread) {
        if (Parameters->Id == -1ULL) {
            Thread = KeGetCurrentThread();
            ObAddReference(Thread);

        } else {
            Thread = PspGetThreadById(PsGetCurrentProcess(), Parameters->Id);
        }

        if (Thread == NULL) {
            Status = STATUS_NO_SUCH_THREAD;
            goto GetResourceUsageEnd;
        }

        PspGetThreadResourceUsage(Thread, &(Parameters->Usage));
        ObReleaseReference(Thread);

    } else {
        Status = STATUS_INVALID_PARAMETER;
        goto GetResourceUsageEnd;
    }

    Parameters->Frequency = HlQueryProcessorCounterFrequency();
    Status = STATUS_SUCCESS;

GetResourceUsageEnd:
    return Status;
}

INTN
PsSysSetUmask (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine sets the file permission mask for the current process.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    PSYSTEM_CALL_SET_UMASK Parameters;
    PKPROCESS Process;

    Parameters = (PSYSTEM_CALL_SET_UMASK)SystemCallParameter;
    Process = PsGetCurrentProcess();
    Parameters->Mask &= FILE_PERMISSION_MASK;
    Parameters->Mask = RtlAtomicExchange32(&(Process->Umask), Parameters->Mask);
    return STATUS_SUCCESS;
}

PKPROCESS
PsCreateProcess (
    PCSTR CommandLine,
    ULONG CommandLineSize,
    PVOID RootDirectoryPathPoint,
    PVOID WorkingDirectoryPathPoint,
    PVOID SharedMemoryDirectoryPathPoint
    )

/*++

Routine Description:

    This routine creates a new process and executes the given binary image.
    This routine must be called at low level.

Arguments:

    CommandLine - Supplies a pointer to a string containing the command line to
        invoke (the executable and any arguments).

    CommandLineSize - Supplies the size of the command line in bytes, including
        the null terminator.

    RootDirectoryPathPoint - Supplies an optional pointer to the path point of
        the root directory to set for the new process.

    WorkingDirectoryPathPoint - Supplies an optional pointer to the path point
        of the working directory to set for the process.

    SharedMemoryDirectoryPathPoint - Supplies an optional pointer to the path
        point of the shared memory object directory to set for the process.

Return Value:

    Returns a pointer to the new process, or NULL if the process could not be
    created. This process will contain a reference that the caller must
    explicitly release.

--*/

{

    PPROCESS_ENVIRONMENT Environment;
    PSTR *EnvironmentArray;
    UINTN EnvironmentCount;
    PKERNEL_ARGUMENT KernelArgument;
    PKPROCESS KernelProcess;
    PKPROCESS NewProcess;
    KSTATUS Status;
    THREAD_CREATION_PARAMETERS ThreadParameters;
    UINTN ValueIndex;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Environment = NULL;
    EnvironmentArray = NULL;
    EnvironmentCount = 0;
    NewProcess = NULL;

    //
    // Loop through once to figure out how many environment variables are going
    // be set based on the kernel command line.
    //

    KernelArgument = NULL;
    do {
        KernelArgument = KeGetKernelArgument(KernelArgument,
                                             PS_KERNEL_ARGUMENT_COMPONENT,
                                             PS_KERNEL_ARGUMENT_ENVIRONMENT);

        if (KernelArgument != NULL) {
            EnvironmentCount += KernelArgument->ValueCount;
        }

    } while (KernelArgument != NULL);

    if (EnvironmentCount > 0) {
        EnvironmentCount += 1;

        //
        // Allocate an array of pointers and fill them in with the ps.env
        // kernel command line parameters.
        //

        EnvironmentArray = MmAllocatePagedPool(sizeof(PSTR) * EnvironmentCount,
                                               PS_ALLOCATION_TAG);

        if (EnvironmentArray == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto CreateProcessEnd;
        }

        KernelArgument = NULL;
        EnvironmentCount = 0;
        while (TRUE) {
            KernelArgument = KeGetKernelArgument(
                                               KernelArgument,
                                               PS_KERNEL_ARGUMENT_COMPONENT,
                                               PS_KERNEL_ARGUMENT_ENVIRONMENT);

            if (KernelArgument == NULL) {
                break;
            }

            for (ValueIndex = 0;
                 ValueIndex < KernelArgument->ValueCount;
                 ValueIndex += 1) {

                EnvironmentArray[EnvironmentCount] =
                                            KernelArgument->Values[ValueIndex];

                EnvironmentCount += 1;
            }
        }

        EnvironmentArray[EnvironmentCount] = NULL;
        Status = PsCreateEnvironment(CommandLine,
                                     CommandLineSize,
                                     EnvironmentArray,
                                     EnvironmentCount,
                                     &Environment);

        if (!KSUCCESS(Status)) {
            goto CreateProcessEnd;
        }
    }

    NewProcess = PspCreateProcess(CommandLine,
                                  CommandLineSize,
                                  Environment,
                                  NULL,
                                  NULL,
                                  RootDirectoryPathPoint,
                                  WorkingDirectoryPathPoint,
                                  SharedMemoryDirectoryPathPoint);

    if (NewProcess == NULL) {
        Status = STATUS_UNSUCCESSFUL;
        goto CreateProcessEnd;
    }

    NewProcess->Umask = PS_DEFAULT_UMASK;
    KernelProcess = PsGetKernelProcess();
    NewProcess->Realm.Uts = KernelProcess->Realm.Uts;
    PspUtsRealmAddReference(NewProcess->Realm.Uts);

    //
    // Give this process it's own new session.
    //

    Status = PspJoinProcessGroup(NewProcess,
                                 NewProcess->Identifiers.ProcessId,
                                 TRUE);

    if (!KSUCCESS(Status)) {
        goto CreateProcessEnd;
    }

    RtlZeroMemory(&ThreadParameters, sizeof(THREAD_CREATION_PARAMETERS));
    ThreadParameters.Process = NewProcess;
    ThreadParameters.Name = "PspLoaderThread";
    ThreadParameters.NameSize = sizeof("PspLoaderThread");
    ThreadParameters.ThreadRoutine = PspLoaderThread;
    Status = PsCreateThread(&ThreadParameters);
    if (!KSUCCESS(Status)) {
        goto CreateProcessEnd;
    }

    Status = STATUS_SUCCESS;

CreateProcessEnd:
    if (Environment != NULL) {
        PsDestroyEnvironment(Environment);
    }

    if (EnvironmentArray != NULL) {
        MmFreePagedPool(EnvironmentArray);
    }

    if (!KSUCCESS(Status)) {
        if (NewProcess != NULL) {

            //
            // If the routine failed, then a thread was never launched. As such,
            // nothing will clean up the new process. "Terminate" it now.
            //

            PspProcessTermination(NewProcess);
            ObReleaseReference(NewProcess);
            NewProcess = NULL;
        }
    }

    return NewProcess;
}

PKPROCESS
PsGetCurrentProcess (
    VOID
    )

/*++

Routine Description:

    This routine returns the currently running process.

Arguments:

    None.

Return Value:

    Returns a pointer to the current process.

--*/

{

    PKTHREAD Thread;

    Thread = KeGetCurrentThread();
    if (Thread == NULL) {
        return NULL;
    }

    return Thread->OwningProcess;
}

PKPROCESS
PsGetKernelProcess (
    VOID
    )

/*++

Routine Description:

    This routine returns a pointer to the system process.

Arguments:

    None.

Return Value:

    Returns a pointer to the system process.

--*/

{

    return PsKernelProcess;
}

ULONG
PsGetProcessCount (
    VOID
    )

/*++

Routine Description:

    This routine returns the number of active processes in the system. This
    count includes the kernel process (and therefore is never zero). This
    information is stale as soon as it is received, and as such is only useful
    in limited scenarios.

Arguments:

    None.

Return Value:

    Returns the number of active processes in the system.

--*/

{

    return PsProcessCount;
}

VOID
PsIterateProcess (
    PROCESS_ID_TYPE Type,
    PROCESS_ID Match,
    PPROCESS_ITERATOR_ROUTINE IteratorFunction,
    PVOID Context
    )

/*++

Routine Description:

    This routine iterates over all processes in the process ID list.

Arguments:

    Type - Supplies the type of identifier to match on. Valid values are
        process ID, process group, or session.

    Match - Supplies the process, process group, or session ID to match on.
        Supply -1 to iterate over all processes.

    IteratorFunction - Supplies a pointer to the function to call for each
        matching process.

    Context - Supplies an opaque pointer that will be passed into the iterator
        function on each iteration.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PKPROCESS Process;
    BOOL Stop;

    Stop = FALSE;
    KeAcquireQueuedLock(PsProcessListLock);
    CurrentEntry = PsProcessListHead.Next;
    if (Match == -1) {
        while ((Stop == FALSE) && (CurrentEntry != &PsProcessListHead)) {
            Process = LIST_VALUE(CurrentEntry, KPROCESS, ListEntry);
            CurrentEntry = CurrentEntry->Next;
            Stop = IteratorFunction(Context, Process);
        }

    } else {
        while ((Stop == FALSE) && (CurrentEntry != &PsProcessListHead)) {
            Process = LIST_VALUE(CurrentEntry, KPROCESS, ListEntry);
            CurrentEntry = CurrentEntry->Next;
            switch (Type) {
            case ProcessIdProcess:
                if (Process->Identifiers.ProcessId != Match) {
                    continue;
                }

                //
                // Found the one process with this ID, so call the iterator and
                // break.
                //

                IteratorFunction(Context, Process);
                goto IterateProcessEnd;

            case ProcessIdProcessGroup:
                if (Process->Identifiers.ProcessGroupId != Match) {
                    continue;
                }

                break;

            case ProcessIdSession:
                if (Process->Identifiers.SessionId != Match) {
                    continue;
                }

                break;

            default:

                ASSERT(FALSE);

                break;
            }

            Stop = IteratorFunction(Context, Process);
        }

    }

IterateProcessEnd:
    KeReleaseQueuedLock(PsProcessListLock);
    return;
}

VOID
PsHandleUserModeFault (
    PVOID VirtualAddress,
    ULONG FaultFlags,
    PTRAP_FRAME TrapFrame,
    PKPROCESS Process
    )

/*++

Routine Description:

    This routine handles a user mode fault where no image section seems to back
    the faulting address or a write attempt was made to a read-only image
    section.

Arguments:

    VirtualAddress - Supplies the virtual address that caused the fault.

    FaultFlags - Supplies the fault information.

    TrapFrame - Supplies a pointer to the trap frame.

    Process - Supplies the process that caused the fault.

Return Value:

    None.

--*/

{

    PVOID InstructionPointer;
    PSIGNAL_QUEUE_ENTRY Signal;
    ULONG SignalNumber;
    PKTHREAD Thread;

    Thread = KeGetCurrentThread();

    ASSERT(Thread->OwningProcess == Process);

    SignalNumber = SIGNAL_ACCESS_VIOLATION;
    if (((FaultFlags & FAULT_FLAG_PROTECTION_FAULT) != 0) ||
        ((FaultFlags & FAULT_FLAG_OUT_OF_BOUNDS) != 0)) {

        SignalNumber = SIGNAL_BUS_ERROR;
    }

    //
    // If the fault originated from kernel mode, that's bad news. Take the
    // system down.
    //

    if (ArIsTrapFrameFromPrivilegedMode(TrapFrame) != FALSE) {
        InstructionPointer = ArGetInstructionPointer(TrapFrame);
        KeCrashSystem(CRASH_PAGE_FAULT,
                      (UINTN)VirtualAddress,
                      (UINTN)InstructionPointer,
                      (UINTN)TrapFrame,
                      STATUS_NOT_FOUND);
    }

    //
    // Allocate a signal queue entry. The process dies if the allocation
    // fails.
    //

    Signal = MmAllocatePagedPool(sizeof(SIGNAL_QUEUE_ENTRY),
                                 PS_ALLOCATION_TAG);

    if (Signal == NULL) {
        PspSetProcessExitStatus(Process,
                                CHILD_SIGNAL_REASON_KILLED,
                                SignalNumber);

        PsSignalProcess(Process, SIGNAL_KILL, NULL);
        return;
    }

    RtlZeroMemory(Signal, sizeof(SIGNAL_QUEUE_ENTRY));
    Signal->Parameters.SignalNumber = SignalNumber;
    if ((VirtualAddress < KERNEL_VA_START) &&
        ((FaultFlags & FAULT_FLAG_PAGE_NOT_PRESENT) != 0)) {

        Signal->Parameters.SignalCode = ACCESS_VIOLATION_MAPPING_ERROR;

    } else {
        Signal->Parameters.SignalCode = ACCESS_VIOLATION_PERMISSION_ERROR;
    }

    Signal->Parameters.FromU.FaultingAddress = VirtualAddress;
    Signal->Parameters.Parameter = (UINTN)ArGetInstructionPointer(TrapFrame);
    Signal->CompletionRoutine = PsDefaultSignalCompletionRoutine;
    PsSignalThread(Thread, SignalNumber, Signal, TRUE);
    return;
}

KSTATUS
PspCopyProcess (
    PKPROCESS Process,
    PKTHREAD MainThread,
    PTRAP_FRAME TrapFrame,
    ULONG Flags,
    PKPROCESS *CreatedProcess
    )

/*++

Routine Description:

    This routine creates a copy of the given process. It copies all images,
    image sections, and open file handles, but copies only the main thread. This
    routine must only be called at low level.

Arguments:

    Process - Supplies a pointer to the process to copy.

    MainThread - Supplies a pointer to the thread to copy (which is assumed to
        be owned by the process to be copied).

    TrapFrame - Supplies a pointer to the trap frame of the interrupted main
        thread.

    Flags - Supplies a bitfield of flags governing the creation of the new
        process. See FORK_FLAG_* definitions.

    CreatedProcess - Supplies an optional pointer that will receive a pointer to
        the created process on success.

Return Value:

    Status code.

--*/

{

    PPATH_POINT CurrentDirectory;
    PATH_POINT CurrentDirectoryCopy;
    PKTHREAD NewMainThread;
    PKPROCESS NewProcess;
    PPATH_POINT RootDirectory;
    PATH_POINT RootDirectoryCopy;
    PPATH_POINT SharedMemoryDirectory;
    PATH_POINT SharedMemoryDirectoryCopy;
    KSTATUS Status;
    PKPROCESS TracingProcess;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    CurrentDirectory = NULL;
    RootDirectory = NULL;
    SharedMemoryDirectory = NULL;
    TracingProcess = NULL;

    //
    // Get the processes root and current directories. Add references in case a
    // pending change directory is coming in, which would release the
    // references held inherently by this process.
    //

    KeAcquireQueuedLock(Process->Paths.Lock);
    if (Process->Paths.CurrentDirectory.PathEntry != NULL) {
        IO_COPY_PATH_POINT(&CurrentDirectoryCopy,
                           &(Process->Paths.CurrentDirectory));

        IO_PATH_POINT_ADD_REFERENCE(&CurrentDirectoryCopy);
        CurrentDirectory = &CurrentDirectoryCopy;
    }

    if (Process->Paths.Root.PathEntry != NULL) {
        IO_COPY_PATH_POINT(&RootDirectoryCopy, &(Process->Paths.Root));
        IO_PATH_POINT_ADD_REFERENCE(&RootDirectoryCopy);
        RootDirectory = &RootDirectoryCopy;
    }

    if (Process->Paths.SharedMemoryDirectory.PathEntry != NULL) {
        IO_COPY_PATH_POINT(&SharedMemoryDirectoryCopy,
                           &(Process->Paths.SharedMemoryDirectory));

        IO_PATH_POINT_ADD_REFERENCE(&SharedMemoryDirectoryCopy);
        SharedMemoryDirectory = &SharedMemoryDirectoryCopy;
    }

    KeReleaseQueuedLock(Process->Paths.Lock);
    NewProcess = PspCreateProcess(Process->BinaryName,
                                  Process->BinaryNameSize,
                                  Process->Environment,
                                  &(Process->Identifiers),
                                  Process->ControllingTerminal,
                                  RootDirectory,
                                  CurrentDirectory,
                                  SharedMemoryDirectory);

    if (CurrentDirectory != NULL) {
        IO_PATH_POINT_RELEASE_REFERENCE(CurrentDirectory);
    }

    if (RootDirectory != NULL) {
        IO_PATH_POINT_RELEASE_REFERENCE(RootDirectory);
    }

    if (SharedMemoryDirectory != NULL) {
        IO_PATH_POINT_RELEASE_REFERENCE(SharedMemoryDirectory);
    }

    if (NewProcess == NULL) {
        Status = STATUS_UNSUCCESSFUL;
        goto CopyProcessEnd;
    }

    //
    // Set the parent, join the parent's children and then the parent's process
    // group. The new process must be on the parent's list of children before
    // joining the process group in case there is a race to change the parent's
    // process group (perhaps a request from the grandparent). Changing a
    // process group requires notifying all the children with non-null process
    // groups.
    //

    NewProcess->Parent = Process;
    KeAcquireQueuedLock(Process->QueuedLock);
    NewProcess->SignalHandlerRoutine = Process->SignalHandlerRoutine;
    NewProcess->HandledSignals = Process->HandledSignals;
    NewProcess->IgnoredSignals = Process->IgnoredSignals;
    NewProcess->Umask = Process->Umask;
    INSERT_BEFORE(&(NewProcess->SiblingListEntry), &(Process->ChildListHead));

    //
    // Check for a tracing process while the lock is held. An exiting tracer
    // sets this pointer to NULL with the tracee's lock held.
    //

    if ((Process->DebugData != NULL) &&
        (Process->DebugData->TracingProcess != NULL)) {

        TracingProcess = Process->DebugData->TracingProcess;
        ObAddReference(TracingProcess);
    }

    KeReleaseQueuedLock(Process->QueuedLock);
    PspAddProcessToParentProcessGroup(NewProcess);

    //
    // If this process' controlling terminal was cleared during the process
    // creation, clear out the new child as well, as the clearing may have
    // happened before the new child was added to the global list.
    //

    if (Process->ControllingTerminal == NULL) {
        NewProcess->ControllingTerminal = NULL;
    }

    //
    // Copy the realms or create new ones if specified.
    //

    if ((Flags & FORK_FLAG_REALM_UTS) != 0) {
        NewProcess->Realm.Uts = PspCreateUtsRealm(Process->Realm.Uts);
        if (NewProcess->Realm.Uts == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto CopyProcessEnd;
        }

    } else {
        NewProcess->Realm.Uts = Process->Realm.Uts;
        PspUtsRealmAddReference(NewProcess->Realm.Uts);
    }

    //
    // Add the tracing process if needed.
    //

    if (TracingProcess != NULL) {
        Status = PspDebugEnable(NewProcess, TracingProcess);
        if (!KSUCCESS(Status)) {
            goto CopyProcessEnd;
        }
    }

    //
    // Copy the process handle table.
    //

    Status = IoCopyProcessHandles(Process, NewProcess);
    if (!KSUCCESS(Status)) {
        goto CopyProcessEnd;
    }

    //
    // Copy the process address space.
    //

    Status = MmCloneAddressSpace(Process->AddressSpace,
                                 NewProcess->AddressSpace);

    if (!KSUCCESS(Status)) {
        goto CopyProcessEnd;
    }

    //
    // Copy the image list.
    //

    Status = PspImCloneProcessImages(Process, NewProcess);
    if (!KSUCCESS(Status)) {
        goto CopyProcessEnd;
    }

    //
    // Clone the main thread, which will kick off the new process.
    //

    NewMainThread = PspCloneThread(NewProcess, MainThread, TrapFrame);
    if (NewMainThread == NULL) {
        Status = STATUS_UNSUCCESSFUL;
        goto CopyProcessEnd;
    }

CopyProcessEnd:
    if (!KSUCCESS(Status)) {
        if (NewProcess != NULL) {

            //
            // If the routine failed, then a thread was never launched. As such,
            // nothing will clean up the new process. "Terminate" it now.
            //

            PspRemoveProcessFromLists(NewProcess);
            PspProcessTermination(NewProcess);
            ObReleaseReference(NewProcess);
            NewProcess = NULL;
        }
    }

    if (CreatedProcess != NULL) {
        *CreatedProcess = NewProcess;
    }

    if (TracingProcess != NULL) {
        ObReleaseReference(TracingProcess);
    }

    return Status;
}

PKPROCESS
PspCreateProcess (
    PCSTR CommandLine,
    ULONG CommandLineSize,
    PPROCESS_ENVIRONMENT SourceEnvironment,
    PPROCESS_IDENTIFIERS Identifiers,
    PVOID ControllingTerminal,
    PPATH_POINT RootDirectory,
    PPATH_POINT WorkingDirectory,
    PPATH_POINT SharedMemoryDirectory
    )

/*++

Routine Description:

    This routine creates a new process with no threads.

Arguments:

    CommandLine - Supplies a pointer to a string containing the command line to
        invoke (the executable and any arguments).

    CommandLineSize - Supplies the size of the command line in bytes, including
        the null terminator.

    SourceEnvironment - Supplies an optional pointer to the initial environment.
        The image name and arguments will be replaced with those given on the
        command line.

    Identifiers - Supplies an optional pointer to the parent process
        identifiers.

    ControllingTerminal - Supplies a pointer to the controlling terminal to set
        for this process.

    RootDirectory - Supplies a pointer to the root directory path point for
        this process. Processes cannot go farther up in the directory hierarchy
        than this without a link. A reference will be added to the path entry
        and mount point of this path point.

    WorkingDirectory - Supplies a pointer to the path point to use for the
        working directory. A reference will be added to the path entry and
        mount point of this path point.

    SharedMemoryDirectory - Supplies a pointer to the path point to use as the
        shared memory object root. A reference will be added to the path entry
        and mount point of this path point.

Return Value:

    Returns a pointer to the new process, or NULL if the process could not be
    created.

--*/

{

    PCSTR BinaryName;
    ULONG BinaryNameSize;
    PPROCESS_ENVIRONMENT Environment;
    PSTR FoundName;
    BOOL KernelProcess;
    PKPROCESS NewProcess;
    CHAR ObjectName[MAX_PROCESS_NAME_LENGTH];
    ULONG ObjectNameLength;
    PROCESS_ID ProcessId;
    KSTATUS Status;

    Environment = NULL;
    KernelProcess = FALSE;
    NewProcess = NULL;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    if (PsKernelProcess == NULL) {
        KernelProcess = TRUE;
        BinaryName = CommandLine;
        BinaryNameSize = CommandLineSize;

    } else {
        if (SourceEnvironment != NULL) {
            Status = PsCopyEnvironment(SourceEnvironment,
                                       &Environment,
                                       FALSE,
                                       NULL,
                                       NULL,
                                       0);

        } else {
            Status = PsCreateEnvironment(CommandLine,
                                         CommandLineSize,
                                         NULL,
                                         0,
                                         &Environment);
        }

        if (!KSUCCESS(Status)) {
            goto CreateProcessEnd;
        }

        BinaryName = Environment->ImageName;
        BinaryNameSize = Environment->ImageNameLength;
    }

    //
    // Create the object name using the next process ID. If any future steps
    // fail, then the process ID is lost. So be it. If allocations are failing,
    // then the process was doomed even if it got created. The hexidecimal
    // string is cheaper to calculate (the formatter gets to shift rather than
    // divide).
    //
    // TODO: Prevent colliding with existing process and process group IDs.
    //

    ProcessId = RtlAtomicAdd32((PULONG)&PsNextProcessId, 1);
    ObjectNameLength = RtlPrintToString(ObjectName,
                                        MAX_PROCESS_NAME_LENGTH,
                                        CharacterEncodingDefault,
                                        "0x%x",
                                        ProcessId);

    //
    // Create the process object.
    //

    NewProcess = ObCreateObject(ObjectProcess,
                                PsProcessDirectory,
                                ObjectName,
                                ObjectNameLength,
                                sizeof(KPROCESS),
                                PspDestroyProcess,
                                0,
                                PS_ALLOCATION_TAG);

    if (NewProcess == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateProcessEnd;
    }

    INITIALIZE_LIST_HEAD(&(NewProcess->ImageListHead));
    INITIALIZE_LIST_HEAD(&(NewProcess->ChildListHead));
    INITIALIZE_LIST_HEAD(&(NewProcess->SignalListHead));
    INITIALIZE_LIST_HEAD(&(NewProcess->UnreapedChildList));
    INITIALIZE_LIST_HEAD(&(NewProcess->TimerList));
    KeInitializeSpinLock(&(NewProcess->ChildSignalLock));
    if (Identifiers != NULL) {
        NewProcess->Identifiers.ParentProcessId = Identifiers->ProcessId;
        NewProcess->Identifiers.ProcessGroupId = Identifiers->ProcessGroupId;
        NewProcess->Identifiers.SessionId = Identifiers->SessionId;
    }

    NewProcess->QueuedLock = KeCreateQueuedLock();
    if (NewProcess->QueuedLock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateProcessEnd;
    }

    NewProcess->StopEvent = KeCreateEvent(NULL);
    if (NewProcess->StopEvent == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateProcessEnd;
    }

    FoundName = RtlStringFindCharacterRight(BinaryName,
                                             '/',
                                             BinaryNameSize);

    if (FoundName != NULL) {
        FoundName += 1;
        BinaryNameSize -= ((UINTN)FoundName - (UINTN)BinaryName);
        BinaryName = FoundName;
    }

    NewProcess->BinaryName = BinaryName;
    NewProcess->BinaryNameSize = BinaryNameSize;
    INITIALIZE_LIST_HEAD(&(NewProcess->ThreadListHead));
    NewProcess->ThreadCount = 0;

    //
    // Create an address space for the new process.
    //

    NewProcess->AddressSpace = MmCreateAddressSpace();
    if (NewProcess->AddressSpace == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateProcessEnd;
    }

    //
    // Create a handle table.
    //

    if (KernelProcess == FALSE) {
        NewProcess->HandleTable = ObCreateHandleTable(
                                                 NewProcess,
                                                 PspHandleTableLookupCallback);

        if (NewProcess->HandleTable == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto CreateProcessEnd;
        }
    }

    //
    // Set up the paths.
    //

    NewProcess->Paths.Lock = KeCreateQueuedLock();
    if (NewProcess->Paths.Lock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateProcessEnd;
    }

    if (RootDirectory != NULL) {

        ASSERT(RootDirectory->PathEntry != NULL);
        ASSERT(RootDirectory->MountPoint != NULL);

        IO_COPY_PATH_POINT(&(NewProcess->Paths.Root), RootDirectory);
        IO_PATH_POINT_ADD_REFERENCE(RootDirectory);
    }

    if (WorkingDirectory == NULL) {
        WorkingDirectory = RootDirectory;
    }

    if (WorkingDirectory != NULL) {

        ASSERT(WorkingDirectory->PathEntry != NULL);
        ASSERT(WorkingDirectory->MountPoint != NULL);

        IO_COPY_PATH_POINT(&(NewProcess->Paths.CurrentDirectory),
                           WorkingDirectory);

        IO_PATH_POINT_ADD_REFERENCE(WorkingDirectory);
    }

    if (SharedMemoryDirectory != NULL) {

        ASSERT(SharedMemoryDirectory->PathEntry != NULL);
        ASSERT(SharedMemoryDirectory->MountPoint != NULL);

        IO_COPY_PATH_POINT(&(NewProcess->Paths.SharedMemoryDirectory),
                           SharedMemoryDirectory);

        IO_PATH_POINT_ADD_REFERENCE(SharedMemoryDirectory);
    }

    //
    // Set the controlling terminal before adding the process to the list.
    // The session leader needs to acquire the process list lock in order to
    // iterate over every process in the session. The controlling terminal of
    // the parent process will need to be double checked to ensure it didn't
    // get cleared (and this process was missed in the clearing).
    //

    NewProcess->ControllingTerminal = ControllingTerminal;

    //
    // Insert the process into the global list.
    //

    NewProcess->Environment = Environment;
    Environment = NULL;
    NewProcess->Identifiers.ProcessId = ProcessId;
    NewProcess->StartTime = HlQueryTimeCounter();
    KeAcquireQueuedLock(PsProcessListLock);
    INSERT_AFTER(&(NewProcess->ListEntry), &PsProcessListHead);
    PsProcessCount += 1;
    KeReleaseQueuedLock(PsProcessListLock);
    SpProcessNewProcess(NewProcess->Identifiers.ProcessId);
    Status = STATUS_SUCCESS;

CreateProcessEnd:
    if (!KSUCCESS(Status)) {
        if (NewProcess != NULL) {
            if (NewProcess->AddressSpace != NULL) {
                MmDestroyAddressSpace(NewProcess->AddressSpace);
                NewProcess->AddressSpace = NULL;
            }

            if (NewProcess->HandleTable != NULL) {
                ObDestroyHandleTable(NewProcess->HandleTable);
                NewProcess->HandleTable = NULL;
            }

            ObReleaseReference(NewProcess);
            NewProcess = NULL;
        }

        if (Environment != NULL) {
            PsDestroyEnvironment(Environment);
        }
    }

    return NewProcess;
}

PKPROCESS
PspGetProcessById (
    PROCESS_ID ProcessId
    )

/*++

Routine Description:

    This routine returns the process with the given process ID, and increments
    the reference count on the process returned.

Arguments:

    ProcessId - Supplies the process ID to search for.

Return Value:

    Returns a pointer to the process with the corresponding ID. The reference
    count will be increased by one.

    NULL if no such process could be found.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PKPROCESS FoundProcess;
    PKPROCESS Process;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    FoundProcess = NULL;
    KeAcquireQueuedLock(PsProcessListLock);
    CurrentEntry = PsProcessListHead.Next;
    while (CurrentEntry != &(PsProcessListHead)) {
        Process = LIST_VALUE(CurrentEntry, KPROCESS, ListEntry);
        if (Process->Identifiers.ProcessId == ProcessId) {
            FoundProcess = Process;
            ObAddReference(FoundProcess);
            break;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    KeReleaseQueuedLock(PsProcessListLock);
    return FoundProcess;
}

PKPROCESS
PspGetChildProcessById (
    PKPROCESS Parent,
    PROCESS_ID ProcessId
    )

/*++

Routine Description:

    This routine returns the child process with the given process ID, and
    increments the reference count on the process returned. It assumes the
    caller holds the parent's queued lock.

Arguments:

    Parent - Supplies a pointer to the parent process whose children are
        searched.

    ProcessId - Supplies the child process ID to search for.

Return Value:

    Returns a pointer to the child process with the corresponding ID. The
    reference count will be increased by one.

    NULL if no such process could be found.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PKPROCESS FoundProcess;
    PKPROCESS Process;

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT(KeIsQueuedLockHeld(Parent->QueuedLock) != FALSE);

    if (LIST_EMPTY(&(Parent->ChildListHead)) != FALSE) {
        return NULL;
    }

    FoundProcess = NULL;
    CurrentEntry = Parent->ChildListHead.Next;
    while (CurrentEntry != &(Parent->ChildListHead)) {
        Process = LIST_VALUE(CurrentEntry, KPROCESS, SiblingListEntry);
        if (Process->Identifiers.ProcessId == ProcessId) {
            FoundProcess = Process;
            ObAddReference(FoundProcess);
            break;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    return FoundProcess;
}

VOID
PspWaitOnStopEvent (
    PKPROCESS Process,
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine waits on the stop event and potentially services any tracer
    requests.

Arguments:

    Process - Supplies a pointer to the current process.

    TrapFrame - Supplies a pointer to the user mode trap frame.

Return Value:

    None.

--*/

{

    BOOL BreakOut;
    PPROCESS_DEBUG_COMMAND Command;
    BOOL CommandHandled;
    PKTHREAD CurrentThread;
    PPROCESS_DEBUG_DATA DebugData;
    ULONG ThreadsStopped;

    CurrentThread = KeGetCurrentThread();
    DebugData = Process->DebugData;
    BreakOut = FALSE;
    while (TRUE) {
        CommandHandled = FALSE;
        KeWaitForEvent(Process->StopEvent, FALSE, WAIT_TIME_INDEFINITE);

        //
        // Skip the rest of this if there's no debugger that's ever been
        // connected.
        //

        if (DebugData == NULL) {
            break;
        }

        //
        // This assignment is only inside the loop to optimize the normal case
        // where debug data is NULL.
        //

        Command = &(DebugData->DebugCommand);

        //
        // If it's a switch thread command to this thread, then take over.
        //

        if ((Command->Command == DebugCommandSwitchThread) &&
            (Command->U.Thread == CurrentThread->ThreadId)) {

            DebugData->DebugLeaderThread = CurrentThread;
            Command->Status = STATUS_SUCCESS;
            CommandHandled = TRUE;

        //
        // Otherwise if this is the thread leader, process a bunch of commands.
        //

        } else if (DebugData->DebugLeaderThread == CurrentThread) {
            CommandHandled = TRUE;
            switch (Command->Command) {
            case DebugCommandReadMemory:
                Command->Status = MmCopyFromUserMode(Command->Data,
                                                     Command->U.Address,
                                                     Command->Size);

                break;

            case DebugCommandWriteMemory:
                Command->Status = MmUserModeDebuggerWrite(Command->Data,
                                                          Command->U.Address,
                                                          Command->Size);

                break;

            //
            // Don't do anything here as the new debug leader is already waking
            // up and assigning itself (above).
            //

            case DebugCommandSwitchThread:
                CommandHandled = FALSE;
                break;

            case DebugCommandGetBreakInformation:
                Command->Status = PspArchGetDebugBreakInformation(TrapFrame);
                break;

            case DebugCommandSetBreakInformation:
                Command->Status = PspArchSetDebugBreakInformation(TrapFrame);
                break;

            case DebugCommandGetSignalInformation:
                RtlCopyMemory(Command->Data,
                              &(DebugData->TracerSignalInformation),
                              sizeof(SIGNAL_PARAMETERS));

                Command->Status = STATUS_SUCCESS;
                break;

            case DebugCommandSetSignalInformation:
                RtlCopyMemory(&(DebugData->TracerSignalInformation),
                              Command->Data,
                              sizeof(SIGNAL_PARAMETERS));

                Command->Status = STATUS_SUCCESS;
                break;

            case DebugCommandSingleStep:
                Command->Status = PspArchSetOrClearSingleStep(TrapFrame, TRUE);
                if (!KSUCCESS(Command->Status)) {
                    break;
                }

                DebugData->TracerSignalInformation.SignalNumber =
                                                      Command->SignalToDeliver;

                Command->Status = STATUS_SUCCESS;
                break;

            case DebugCommandContinue:
                DebugData->TracerSignalInformation.SignalNumber =
                                                      Command->SignalToDeliver;

                Command->Status = STATUS_SUCCESS;
                break;

            case DebugCommandRangeStep:

                ASSERT(Command->Size == sizeof(PROCESS_DEBUG_BREAK_RANGE));

                RtlCopyMemory(&(DebugData->BreakRange),
                              Command->Data,
                              sizeof(PROCESS_DEBUG_BREAK_RANGE));

                DebugData->TracerSignalInformation.SignalNumber =
                                                      Command->SignalToDeliver;

                Command->Status = PspArchSetOrClearSingleStep(TrapFrame, TRUE);
                if (!KSUCCESS(Command->Status)) {
                    break;
                }

                break;

            case DebugCommandInvalid:

                //
                // This must have come from a kill and continue. Setting this
                // event is actually bad, as it might race with issuing a
                // command.
                //

                KeSignalEvent(Process->StopEvent, SignalOptionUnsignal);

                //
                // TODO: Rework Ps debug to have the tracer do all the work.
                //

                ASSERT(Command->Command == DebugCommandInvalid);

                break;

            default:
                break;
            }

        //
        // This is not the leader thread. Unless the debugger wants to
        // continue, yield to the leader.
        //

        } else {
            if ((Command->Command != DebugCommandContinue) &&
                (Command->Command != DebugCommandSingleStep) &&
                (Command->Command != DebugCommandRangeStep)) {

                KeYield();
            }
        }

        //
        // There are a couple commands that every thread processes.
        //

        switch (Command->Command) {
        case DebugCommandContinue:
        case DebugCommandSingleStep:
        case DebugCommandRangeStep:
            BreakOut = TRUE;
            break;

        //
        // The debug leader handles these commands, other threads ignore them.
        //

        case DebugCommandInvalid:
        case DebugCommandReadMemory:
        case DebugCommandWriteMemory:
        case DebugCommandSwitchThread:
        case DebugCommandGetBreakInformation:
        case DebugCommandSetBreakInformation:
        case DebugCommandGetSignalInformation:
        case DebugCommandSetSignalInformation:
            break;

        default:

            ASSERT(FALSE);

            BreakOut = TRUE;
            break;
        }

        //
        // If this thread handled the command, then signal to the tracer that
        // the command is finished, and reset the stop event if the command was
        // not a go. Delays before here may cause the other threads to spin
        // around here a few times, but it should be relatively minimal.
        //

        if (CommandHandled != FALSE) {
            if (BreakOut == FALSE) {
                KeSignalEvent(Process->StopEvent, SignalOptionUnsignal);
            }

            Command->PreviousCommand = Command->Command;
            if (BreakOut == FALSE) {
                Command->Command = DebugCommandInvalid;
            }

            KeSignalEvent(DebugData->DebugCommandCompleteEvent,
                          SignalOptionSignalAll);
        }

        if (BreakOut != FALSE) {
            break;
        }
    }

    //
    // Indicate this thread is out and ready to continue.
    //

    ThreadsStopped = RtlAtomicAdd32(&(Process->StoppedThreadCount), -1) - 1;

    ASSERT(ThreadsStopped < 0x10000000);

    if (DebugData != NULL) {

        //
        // If this was the last thread out, signal the all stopped event.
        // Otherwise, run through. The leader should be waiting on the stop
        // event inside the tracer break handler.
        //

        if (ThreadsStopped == 0) {
            KeSignalEvent(DebugData->AllStoppedEvent, SignalOptionSignalAll);
        }
    }

    return;
}

BOOL
PspSetProcessExitStatus (
    PKPROCESS Process,
    USHORT ExitReason,
    UINTN ExitStatus
    )

/*++

Routine Description:

    This routine sets the process exit status and flags if they are not already
    set.

Arguments:

    Process - Supplies a pointer to the process that is exiting.

    ExitReason - Supplies the reason code for the child exit.

    ExitStatus - Supplies the exit status to set.

Return Value:

    TRUE if the values were set in the process.

    FALSE if an exit status was already set in the process.

--*/

{

    BOOL WasSet;

    WasSet = FALSE;
    KeAcquireQueuedLock(Process->QueuedLock);
    if (Process->ExitReason == 0) {
        Process->ExitReason = ExitReason;
        Process->ExitStatus = ExitStatus;
        WasSet = TRUE;
    }

    KeReleaseQueuedLock(Process->QueuedLock);
    return WasSet;
}

KSTATUS
PspGetProcessList (
    PKPROCESS **Array,
    PULONG ArraySize
    )

/*++

Routine Description:

    This routine returns an array of pointers to all the processes in the
    system. This array may be incomplete if additional processes come in while
    the array is being created.

Arguments:

    Array - Supplies a pointer where an array of pointers to processes will be
        returned. Each process in the array will have its reference count
        incremented. The caller is responsible for releasing the references
        and freeing the array from non-paged pool.

    ArraySize - Supplies a pointer where the number of elements in the array
        will be returned on success.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES on allocation failure.

--*/

{

    PLIST_ENTRY CurrentEntry;
    UINTN MaxProcessCount;
    PKPROCESS Process;
    PKPROCESS *ProcessArray;
    UINTN ProcessCount;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    *Array = NULL;
    *ArraySize = 0;
    MaxProcessCount = PsProcessCount * PROCESS_LIST_FUDGE_FACTOR;
    ProcessArray = MmAllocateNonPagedPool(MaxProcessCount * sizeof(PKPROCESS),
                                          PS_ALLOCATION_TAG);

    if (ProcessArray == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    ProcessCount = 0;
    KeAcquireQueuedLock(PsProcessListLock);
    CurrentEntry = PsProcessListHead.Next;
    while ((CurrentEntry != &PsProcessListHead) &&
           (ProcessCount < MaxProcessCount)) {

        Process = LIST_VALUE(CurrentEntry, KPROCESS, ListEntry);
        ProcessArray[ProcessCount] = Process;
        ObAddReference(Process);
        ProcessCount += 1;
        CurrentEntry = CurrentEntry->Next;
    }

    KeReleaseQueuedLock(PsProcessListLock);
    *Array = ProcessArray;
    *ArraySize = ProcessCount;
    return STATUS_SUCCESS;
}

VOID
PspDestroyProcessList (
    PKPROCESS *Array,
    ULONG ArrayCount
    )

/*++

Routine Description:

    This routine destroys a process array, releasing the reference on each
    process and freeing the array from non-paged pool.

Arguments:

    Array - Supplies an array of pointers to processes to destroy.

    ArrayCount - Supplies the number of elements in the array.

Return Value:

    None.

--*/

{

    ULONG Index;

    for (Index = 0; Index < ArrayCount; Index += 1) {
        ObReleaseReference(Array[Index]);
    }

    MmFreeNonPagedPool(Array);
    return;
}

KSTATUS
PspGetProcessIdList (
    PPROCESS_ID Array,
    PUINTN ArraySize
    )

/*++

Routine Description:

    This routine fills in the given array with process IDs from the currently
    running processes.

Arguments:

    Array - Supplies a pointer to an array where process IDs will be stored on
        success.

    ArraySize - Supplies a pointer where on input will contain the size of the
        supplied array. On output, it will return the actual size of the array
        needed.

Return Value:

    Status code.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PKPROCESS Process;
    UINTN ProcessCount;
    UINTN RequiredArraySize;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // First check the process count outside the lock to see if there is even
    // a chance.
    //

    RequiredArraySize = PsProcessCount * sizeof(PROCESS_ID);
    if ((Array == NULL) || (*ArraySize < RequiredArraySize)) {
        *ArraySize = RequiredArraySize;
        return STATUS_BUFFER_TOO_SMALL;
    }

    //
    // Acquire the lock, test the size again, and if it's OK, copy the process
    // IDs into the array.
    //

    ProcessCount = 0;
    KeAcquireQueuedLock(PsProcessListLock);
    RequiredArraySize = PsProcessCount * sizeof(PROCESS_ID);
    if (*ArraySize < RequiredArraySize) {
        Status = STATUS_BUFFER_TOO_SMALL;
        goto GetProcessIdListEnd;
    }

    CurrentEntry = PsProcessListHead.Next;
    while (CurrentEntry != &PsProcessListHead) {
        Process = LIST_VALUE(CurrentEntry, KPROCESS, ListEntry);
        Array[ProcessCount] = Process->Identifiers.ProcessId;
        ProcessCount += 1;
        CurrentEntry = CurrentEntry->Next;
    }

    Status = STATUS_SUCCESS;

GetProcessIdListEnd:
    KeReleaseQueuedLock(PsProcessListLock);
    *ArraySize = RequiredArraySize;
    return Status;
}

VOID
PspProcessTermination (
    PKPROCESS Process
    )

/*++

Routine Description:

    This routine is called when the final thread in a process terminates.

Arguments:

    Process - Supplies a pointer to the process to gut.

Return Value:

    None.

--*/

{

    PPATH_POINT PathPoint;

    //
    // Proceed to destroy the process structures.
    //

    PspDestroyProcessTimers(Process);
    PspImUnloadAllImages(Process);
    IoCloseProcessHandles(Process, 0);
    MmCleanUpProcessMemory(Process);
    if (PsIsSessionLeader(Process)) {
        IoTerminalDisassociate(Process);
    }

    //
    // Remove the process from its process group and then notify its children
    // of its termination. This must happen in this order as the children need
    // to be iterated in order to potentially notify their process groups of a
    // leaving outside parent.
    //

    PspRemoveProcessFromProcessGroup(Process);
    PspProcessChildrenOfTerminatingProcess(Process);
    if (Process->Paths.CurrentDirectory.PathEntry != NULL) {
        IO_PATH_POINT_RELEASE_REFERENCE(&(Process->Paths.CurrentDirectory));
        Process->Paths.CurrentDirectory.PathEntry = NULL;
        Process->Paths.CurrentDirectory.MountPoint = NULL;
    }

    if (Process->Paths.Root.PathEntry != NULL) {
        IO_PATH_POINT_RELEASE_REFERENCE(&(Process->Paths.Root));
        Process->Paths.Root.PathEntry = NULL;
        Process->Paths.Root.MountPoint = NULL;
    }

    if (Process->Paths.SharedMemoryDirectory.PathEntry != NULL) {
        PathPoint = (PPATH_POINT)&(Process->Paths.SharedMemoryDirectory);
        IO_PATH_POINT_RELEASE_REFERENCE(PathPoint);
        PathPoint->PathEntry = NULL;
        PathPoint->MountPoint = NULL;
    }

    if (Process->Environment != NULL) {
        PsDestroyEnvironment(Process->Environment);
        Process->Environment = NULL;
        Process->BinaryName = NULL;
        Process->BinaryNameSize = 0;
    }

    if (Process->HandleTable != NULL) {
        ObDestroyHandleTable(Process->HandleTable);
        Process->HandleTable = NULL;
    }

    if (Process->Realm.Uts != NULL) {
        PspUtsRealmReleaseReference(Process->Realm.Uts);
        Process->Realm.Uts = NULL;
    }

    //
    // There should only be one remaining page mapped: the shared user data
    // page.
    //

    ASSERT(Process->AddressSpace->ResidentSet <= 1);

    return;
}

VOID
PspGetThreadResourceUsage (
    PKTHREAD Thread,
    PRESOURCE_USAGE Usage
    )

/*++

Routine Description:

    This routine returns resource usage information for the given thread.

Arguments:

    Thread - Supplies a pointer to the thread to get usage information for.

    Usage - Supplies a pointer where the usage information is returned.

Return Value:

    None.

--*/

{

    PspReadResourceUsage(Usage, &(Thread->ResourceUsage));
    Usage->MaxResidentSet = Thread->OwningProcess->AddressSpace->MaxResidentSet;
    return;
}

VOID
PspAddResourceUsages (
    PRESOURCE_USAGE Destination,
    PRESOURCE_USAGE Add
    )

/*++

Routine Description:

    This routine adds two resource usage structures together, returning the
    result in the destination. This routine assumes neither structure is going
    to change mid-copy.

Arguments:

    Destination - Supplies a pointer to the first structure to add, and the
        destination for the sum.

    Add - Supplies a pointer to the second structure to add.

Return Value:

    None. The sum of each structure element is returned in the destination.

--*/

{

    Destination->UserCycles += Add->UserCycles;
    Destination->KernelCycles += Add->KernelCycles;
    Destination->Preemptions += Add->Preemptions;
    Destination->Yields += Add->Yields;
    Destination->PageFaults += Add->PageFaults;
    Destination->HardPageFaults += Add->HardPageFaults;
    Destination->BytesRead += Add->BytesRead;
    Destination->BytesWritten += Add->BytesWritten;
    Destination->DeviceReads += Add->DeviceReads;
    Destination->DeviceWrites += Add->DeviceWrites;
    if (Add->MaxResidentSet > Destination->MaxResidentSet) {
        Destination->MaxResidentSet = Add->MaxResidentSet;
    }

    return;
}

VOID
PspRemoveProcessFromLists (
    PKPROCESS Process
    )

/*++

Routine Description:

    This routine removes the given process from its parent's list of children
    and from the global list of processes.

Arguments:

    Process - Supplies a pointer to the process to be removed.

Return Value:

    None.

--*/

{

    PKPROCESS Parent;
    PKPROCESS TracingProcess;

    //
    // Remove the process from the global list of processes so that it can no
    // longer be found.
    //

    KeAcquireQueuedLock(PsProcessListLock);
    if (Process->ListEntry.Next != NULL) {
        LIST_REMOVE(&(Process->ListEntry));
        Process->ListEntry.Next = NULL;
        PsProcessCount -= 1;
    }

    KeReleaseQueuedLock(PsProcessListLock);

    //
    // Remove the process from the parent's list. Acquire the process lock
    // to synchronize with the parent dying and trying to null out the parent
    // pointer. Also synchronize with the tracer and attempt to get a reference
    // on it.
    //

    TracingProcess = NULL;
    KeAcquireQueuedLock(Process->QueuedLock);
    Parent = Process->Parent;
    if (Parent != NULL) {
        ObAddReference(Parent);
    }

    if ((Process->DebugData != NULL) &&
        (Process->DebugData->TracingProcess != NULL)) {

        TracingProcess = Process->DebugData->TracingProcess;
        ObAddReference(TracingProcess);
    }

    KeReleaseQueuedLock(Process->QueuedLock);
    if (Parent != NULL) {
        KeAcquireQueuedLock(Parent->QueuedLock);
        if (Process->SiblingListEntry.Next != NULL) {
            LIST_REMOVE(&(Process->SiblingListEntry));
            Process->SiblingListEntry.Next = NULL;
            Process->Parent = NULL;

            //
            // Simulate the reparenting even though that's not done.
            //

            Process->Identifiers.ParentProcessId = 1;
        }

        KeReleaseQueuedLock(Parent->QueuedLock);

        //
        // Release the reference added above when the parent was grabbed.
        //

        ObReleaseReference(Parent);
    }

    //
    // Remove the process from the tracer's list. If the tracer is detaching
    // itself from the tracee, it will have set the tracee's tracing process
    // pointer to NULL and removed it from the list.
    //

    if (TracingProcess != NULL) {
        KeAcquireQueuedLock(TracingProcess->QueuedLock);
        if (Process->DebugData->TracingProcess != NULL) {

            ASSERT(Process->DebugData->TracerListEntry.Next != NULL);

            LIST_REMOVE(&(Process->DebugData->TracerListEntry));
            Process->DebugData->TracerListEntry.Next = NULL;
            Process->DebugData->TracingProcess = NULL;
        }

        KeReleaseQueuedLock(TracingProcess->QueuedLock);

        //
        // Release the reference added above when the tracer was grabbed.
        //

        ObReleaseReference(TracingProcess);
    }

    return;
}

KSTATUS
PspGetProcessIdentity (
    PKPROCESS Process,
    PTHREAD_IDENTITY Identity
    )

/*++

Routine Description:

    This routine gets the identity of the process, which is simply that of
    an arbitrary thread in the process.

Arguments:

    Process - Supplies a pointer to the process to get an identity for.

    Identity - Supplies a pointer where the process identity will be returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NO_SUCH_PROCESS if the given process ID does not correspond to any
    known process.

--*/

{

    KSTATUS Status;
    PKTHREAD Thread;

    KeAcquireQueuedLock(Process->QueuedLock);
    if (Process->ThreadCount != 0) {
        Thread = LIST_VALUE(Process->ThreadListHead.Next,
                            KTHREAD,
                            ProcessEntry);

        RtlCopyMemory(Identity, &(Thread->Identity), sizeof(THREAD_IDENTITY));
        Status = STATUS_SUCCESS;

    } else {
        Status = STATUS_NO_SUCH_PROCESS;
    }

    KeReleaseQueuedLock(Process->QueuedLock);
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
PspDestroyProcess (
    PVOID Object
    )

/*++

Routine Description:

    This routine cleans up a process that has exited. The pointer to the
    process must not be referenced after this routine is called, as it will
    be freed as part of this call.

Arguments:

    Object - Supplies a pointer to the object being destroyed.

Return Value:

    None.

--*/

{

    PKPROCESS Process;

    //
    // This routine must not touch paged objects (including freeing paged pool),
    // as it may be called from the paging thread (the paging thread releases
    // a reference on an image section, which may be the last reference of the
    // process).
    //

    Process = (PKPROCESS)Object;

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT(PsGetCurrentProcess() != Process);
    ASSERT(Process->Header.Type == ObjectProcess);

    //
    // Assert that everything was properly cleaned up.
    //

    ASSERT(LIST_EMPTY(&(Process->AddressSpace->SectionListHead)) != FALSE);
    ASSERT(LIST_EMPTY(&(Process->ImageListHead)) != FALSE);
    ASSERT(Process->ImageCount == 0);
    ASSERT(Process->ProcessGroup == NULL);
    ASSERT(Process->Parent == NULL);
    ASSERT(Process->SiblingListEntry.Next == NULL);
    ASSERT(Process->ListEntry.Next == NULL);

    //
    // There should be at most one remaining page mapped: the shared user data
    // page.
    //

    ASSERT(Process->AddressSpace->ResidentSet <= 1);

    //
    // Clean up the debug data if present.
    //

    if (Process->DebugData != NULL) {

        ASSERT(LIST_EMPTY(&(Process->DebugData->TraceeListHead)) != FALSE);
        ASSERT(Process->DebugData->TracingProcess == NULL);
        ASSERT(Process->DebugData->TracerListEntry.Next == NULL);

        PspDestroyDebugData(Process->DebugData);
        Process->DebugData = NULL;
    }

    ASSERT(LIST_EMPTY(&(Process->ChildListHead)) != FALSE);
    ASSERT(LIST_EMPTY(&(Process->SignalListHead)) != FALSE);
    ASSERT(LIST_EMPTY(&(Process->UnreapedChildList)) != FALSE);
    ASSERT(LIST_EMPTY(&(Process->TimerList)) != FALSE);
    ASSERT(LIST_EMPTY(&(Process->ThreadListHead)) != FALSE);
    ASSERT(Process->ThreadCount == 0);
    ASSERT(Process->DebugData == NULL);
    ASSERT(Process->Paths.CurrentDirectory.PathEntry == NULL);
    ASSERT(Process->Paths.CurrentDirectory.MountPoint == NULL);
    ASSERT(Process->Paths.Root.PathEntry == NULL);
    ASSERT(Process->Paths.Root.MountPoint == NULL);
    ASSERT(Process->Paths.SharedMemoryDirectory.PathEntry == NULL);
    ASSERT(Process->Paths.SharedMemoryDirectory.MountPoint == NULL);
    ASSERT(Process->Environment == NULL);
    ASSERT(Process->HandleTable == NULL);

    if (Process->AddressSpace != NULL) {
        MmDestroyAddressSpace(Process->AddressSpace);
    }

    if (Process->StopEvent != NULL) {
        KeDestroyEvent(Process->StopEvent);
        Process->StopEvent = NULL;
    }

    if (Process->QueuedLock != NULL) {
        KeDestroyQueuedLock(Process->QueuedLock);
    }

    if (Process->Paths.Lock != NULL) {
        KeDestroyQueuedLock(Process->Paths.Lock);
    }

    return;
}

VOID
PspProcessChildrenOfTerminatingProcess (
    PKPROCESS Process
    )

/*++

Routine Description:

    This routine is called as a process is being destroyed. It disassociates
    any children from the dying process.

Arguments:

    Process - Supplies a pointer to the dying parent process.

Return Value:

    None.

--*/

{

    PKPROCESS Child;
    PLIST_ENTRY CurrentEntry;
    PPROCESS_DEBUG_DATA DebugData;
    PROCESS_DEBUG_COMMAND TerminateCommand;
    PKPROCESS Tracee;

    //
    // Disassociate the children from their dying parent.
    //

    KeAcquireQueuedLock(Process->QueuedLock);
    CurrentEntry = Process->ChildListHead.Next;
    while (CurrentEntry != &(Process->ChildListHead)) {
        Child = LIST_VALUE(CurrentEntry, KPROCESS, SiblingListEntry);
        CurrentEntry = CurrentEntry->Next;
        KeAcquireQueuedLock(Child->QueuedLock);
        LIST_REMOVE(&(Child->SiblingListEntry));
        Child->SiblingListEntry.Next = NULL;
        Child->Parent = NULL;

        //
        // Simulate the reparenting even though that's not done.
        //

        Child->Identifiers.ParentProcessId = 1;
        KeReleaseQueuedLock(Child->QueuedLock);
    }

    KeReleaseQueuedLock(Process->QueuedLock);

    //
    // Disassociate the tracees from the dying tracer. The process should have
    // no threads, meaning that no new tracees should be added to the list. A
    // tracee may remove itself (under the protection of the tracer's lock), so
    // annoyingly grab the lock on each removal attempt. It should also be
    // noted that this lock dance is done because debug commands cannot be
    // issued while the tracer's process lock is held; the system may deadlock
    // between the process lock and the debug command completion event.
    //

    if (Process->DebugData != NULL) {

        ASSERT(Process->ThreadCount == 0);

        while (LIST_EMPTY(&(Process->DebugData->TraceeListHead)) == FALSE) {
            Tracee = NULL;
            KeAcquireQueuedLock(Process->QueuedLock);
            if (LIST_EMPTY(&(Process->DebugData->TraceeListHead)) == FALSE) {
                DebugData = LIST_VALUE(Process->DebugData->TraceeListHead.Next,
                                       PROCESS_DEBUG_DATA,
                                       TracerListEntry);

                Tracee = DebugData->Process;
                KeAcquireQueuedLock(Tracee->QueuedLock);

                //
                // The tracing process pointer should not be NULL.
                //

                ASSERT(DebugData->TracingProcess == Process);

                LIST_REMOVE(&(DebugData->TracerListEntry));
                DebugData->TracerListEntry.Next = NULL;
                DebugData->TracingProcess = NULL;

                //
                // Add a reference to the tracee so it does not disappear when
                // the lock is released.
                //

                ObAddReference(Tracee);
                KeReleaseQueuedLock(Tracee->QueuedLock);
            }

            KeReleaseQueuedLock(Process->QueuedLock);

            //
            // If there was a tracee, kill it. The owning tracer is dead and it
            // likely shouldn't be alive without the tracer.
            //

            if (Tracee != NULL) {
                PspSetProcessExitStatus(Tracee,
                                        CHILD_SIGNAL_REASON_KILLED,
                                        SIGNAL_ABORT);

                PsSignalProcess(Tracee, SIGNAL_KILL, NULL);

                //
                // If the tracee is already waiting on this tracer, then
                // continue it so it can run head first into the kill signal.
                // Cruel.
                //

                if (KeIsSpinLockHeld(&(DebugData->TracerLock)) != FALSE) {
                    RtlZeroMemory(&TerminateCommand,
                                  sizeof(PROCESS_DEBUG_COMMAND));

                    TerminateCommand.Command = DebugCommandContinue;
                    TerminateCommand.SignalToDeliver =
                               DebugData->TracerSignalInformation.SignalNumber;

                    PspDebugIssueCommand(Process, Tracee, &TerminateCommand);
                }

                ObReleaseReference(Tracee);
            }
        }
    }

    return;
}

VOID
PspLoaderThread (
    PVOID Context
    )

/*++

Routine Description:

    This routine begins a new process by loading the executable.

Arguments:

    Context - Supplies the context pointer, which is the name of the binary to
        load.

Return Value:

    None.

--*/

{

    PKPROCESS Process;
    PROCESS_START_DATA StartData;
    KSTATUS Status;
    PKTHREAD Thread;
    THREAD_CREATION_PARAMETERS ThreadParameters;

    Thread = KeGetCurrentThread();
    Process = Thread->OwningProcess;

    //
    // Map the user shared data page into the process' usermode address space.
    //

    Status = MmMapUserSharedData(Process->AddressSpace);
    if (!KSUCCESS(Status)) {
        goto LoaderThreadEnd;
    }

    //
    // Initialize the memory map limit, otherwise the image loads can't map
    // anything anywhere.
    //

    Process->AddressSpace->MaxMemoryMap = MAX_USER_ADDRESS -
                                  (Thread->Limits[ResourceLimitStack].Current +
                                   USER_STACK_HEADROOM) + 1;

    //
    // Load the executable image for the process.
    //

    Status = PspLoadExecutable(Process->Environment->ImageName,
                               NULL,
                               NULL,
                               &StartData);

    if (!KSUCCESS(Status)) {
        goto LoaderThreadEnd;
    }

    Process->Environment->StartData = &StartData;

    //
    // Kick off the primary usermode thread.
    //

    RtlZeroMemory(&ThreadParameters, sizeof(THREAD_CREATION_PARAMETERS));
    ThreadParameters.Name = "MainThread";
    ThreadParameters.NameSize = sizeof("MainThread");
    ThreadParameters.ThreadRoutine = StartData.EntryPoint;
    ThreadParameters.Environment = Process->Environment;
    ThreadParameters.Flags = THREAD_FLAG_USER_MODE;
    Status = PsCreateThread(&ThreadParameters);
    Process->Environment->StartData = NULL;
    if (!KSUCCESS(Status)) {
        goto LoaderThreadEnd;
    }

    Status = STATUS_SUCCESS;

LoaderThreadEnd:
    if (!KSUCCESS(Status)) {
        PspSetProcessExitStatus(Process,
                                CHILD_SIGNAL_REASON_KILLED,
                                SIGNAL_ABORT);
    }

    //
    // There's really no point in cleaning up as the process cleanup will
    // catch everything.
    //

    return;
}

KSTATUS
PspLoadExecutable (
    PCSTR BinaryName,
    PIMAGE_FILE_INFORMATION File,
    PIMAGE_BUFFER Buffer,
    PPROCESS_START_DATA StartData
    )

/*++

Routine Description:

    This routine loads a new executable image into memory.

Arguments:

    BinaryName - Supplies a pointer to the name of the image on disk. This
        memory is not used after this function call, and can be released by the
        caller.

    File - Supplies an optional pointer to the already retrieved file
        information, including a handle pointing at the beginning of the file.

    Buffer - Supplies an optional pointer to the loaded image buffer, or at
        least part of it.

    StartData - Supplies a pointer to the process start data, which will be
        initialized by this routine.

Return Value:

    Status code.

--*/

{

    PLOADED_IMAGE Executable;
    ULONG Flags;
    PLOADED_IMAGE Interpreter;
    PLOADED_IMAGE OsBaseLibrary;
    ULONG PageSize;
    PKPROCESS Process;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Executable = NULL;
    Process = PsGetCurrentProcess();

    ASSERT(Process != PsKernelProcess);

    PsAcquireImageListLock(Process);

    //
    // Always load the OS base library.
    //

    Flags = IMAGE_LOAD_FLAG_LOAD_ONLY;
    Status = ImLoad(&(Process->ImageListHead),
                    OS_BASE_LIBRARY,
                    NULL,
                    NULL,
                    Process,
                    Flags,
                    &OsBaseLibrary,
                    NULL);

    if (!KSUCCESS(Status)) {
        RtlDebugPrint("Failed to load %s: %d\n", OS_BASE_LIBRARY, Status);
        goto LoadExecutableEnd;
    }

    OsBaseLibrary->LibraryName = OsBaseLibrary->FileName;

    //
    // Load the image and maybe the interpreter, but do not load any imports
    // or perform any relocations.
    //

    Flags = IMAGE_LOAD_FLAG_LOAD_ONLY | IMAGE_LOAD_FLAG_PRIMARY_EXECUTABLE;
    Status = ImLoad(&(Process->ImageListHead),
                    BinaryName,
                    File,
                    Buffer,
                    Process,
                    Flags,
                    &Executable,
                    &Interpreter);

    if (!KSUCCESS(Status)) {
        RtlDebugPrint("Failed to load %s: %d\n", BinaryName, Status);
        goto LoadExecutableEnd;
    }

    //
    // Drop the extra reference on the OS base library if it was also loaded
    // as the executable directly or the interpreter.
    //

    if ((OsBaseLibrary == Interpreter) || (OsBaseLibrary == Executable)) {
        ImImageReleaseReference(OsBaseLibrary);
    }

    //
    // Save the address of the program break.
    //

    PageSize = MmPageSize();
    Process->AddressSpace->BreakStart =
           ALIGN_POINTER_UP(Executable->LoadedImageBuffer + Executable->Size,
                            PageSize);

    Process->AddressSpace->BreakEnd = Process->AddressSpace->BreakStart;
    PspInitializeProcessStartData(StartData,
                                  OsBaseLibrary,
                                  Executable,
                                  Interpreter);

LoadExecutableEnd:
    PsReleaseImageListLock(Process);
    return Status;
}

VOID
PspHandleTableLookupCallback (
    PHANDLE_TABLE HandleTable,
    HANDLE Descriptor,
    PVOID HandleValue
    )

/*++

Routine Description:

    This routine is called whenever a handle is looked up. It is called with
    the handle table lock still held.

Arguments:

    HandleTable - Supplies a pointer to the handle table being iterated through.

    Descriptor - Supplies the handle descriptor for the current handle.

    HandleValue - Supplies the handle value for the current handle.

Return Value:

    None.

--*/

{

    PIO_HANDLE IoHandle;

    ASSERT(HandleValue != NULL);

    IoHandle = (PIO_HANDLE)HandleValue;
    IoIoHandleAddReference(IoHandle);
    return;
}

KSTATUS
PspDebugEnable (
    PKPROCESS Process,
    PKPROCESS TracingProcess
    )

/*++

Routine Description:

    This routine enables debugging on the current process by its parent.

Arguments:

    Process - Supplies a pointer to the process to enable debugging on. This
        should only ever be the current process or a new process being cloned
        that has no active threads on it.

    TracingProcess - Supplies a pointer to the process that will trace this
        process.

Return Value:

    Status code.

--*/

{

    PPROCESS_DEBUG_DATA DebugData;
    BOOL LockHeld;
    KSTATUS Status;

    LockHeld = FALSE;

    ASSERT(Process != PsGetKernelProcess());

    Status = PspCreateDebugDataIfNeeded(Process);
    if (!KSUCCESS(Status)) {
        goto DebugEnableEnd;
    }

    Status = PspCreateDebugDataIfNeeded(TracingProcess);
    if (!KSUCCESS(Status)) {
        goto DebugEnableEnd;
    }

    DebugData = Process->DebugData;
    KeAcquireQueuedLock(TracingProcess->QueuedLock);
    LockHeld = TRUE;

    //
    // If the tracing process is actually dead (no threads), then do not add
    // another tracee to its list. The new tracee likely missed the kill
    // signals sent by the tracer.
    //

    if (TracingProcess->ThreadCount == 0) {
        Status = STATUS_TOO_LATE;
        goto DebugEnableEnd;
    }

    Status = STATUS_RESOURCE_IN_USE;
    KeAcquireQueuedLock(Process->QueuedLock);
    if (DebugData->TracingProcess == NULL) {

        ASSERT(Process->DebugData->TracerListEntry.Next == NULL);

        INSERT_BEFORE(&(Process->DebugData->TracerListEntry),
                      &(TracingProcess->DebugData->TraceeListHead));

        DebugData->TracingProcess = TracingProcess;
        Status = STATUS_SUCCESS;
    }

    KeReleaseQueuedLock(Process->QueuedLock);

DebugEnableEnd:
    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(TracingProcess->QueuedLock);
    }

    return Status;
}

KSTATUS
PspDebugPrint (
    PKPROCESS Process,
    PPROCESS_DEBUG_COMMAND Command
    )

/*++

Routine Description:

    This routine attempts to print output to the debug console.

Arguments:

    Process - Supplies a pointer to the process creating the debug statement.

    Command - Supplies a pointer to the print command.

Return Value:

    Status code.

--*/

{

    PSTR NonPagedCopy;
    PSTR PagedCopy;
    KSTATUS Status;

    ASSERT(Command->Command == DebugCommandPrint);

    PagedCopy = NULL;
    NonPagedCopy = NULL;
    if (Command->Size == 0) {
        Status = STATUS_SUCCESS;
        goto DebugPrintEnd;
    }

    //
    // Copy the string into paged pool.
    //

    Status = MmCreateCopyOfUserModeString(Command->Data,
                                          Command->Size,
                                          PS_ALLOCATION_TAG,
                                          &PagedCopy);

    if (!KSUCCESS(Status)) {
        goto DebugPrintEnd;
    }

    //
    // Copy the string into non-paged pool.
    //

    NonPagedCopy = MmAllocateNonPagedPool(Command->Size, PS_ALLOCATION_TAG);
    if (NonPagedCopy == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto DebugPrintEnd;
    }

    RtlCopyMemory(NonPagedCopy, PagedCopy, Command->Size);
    NonPagedCopy[Command->Size - 1] = '\0';

    //
    // Probably the more suitable thing would be to somehow forward this
    // on through a signal to the tracing process, and only if there is none
    // sending it to the kernel debugger. But for now, this is just fine.
    // Acquire the process queued lock to avoid racing with execute image while
    // reaching in to get the process name.
    //

    KeAcquireQueuedLock(Process->QueuedLock);
    RtlDebugPrint("%s: %s", Process->Header.Name, NonPagedCopy);
    KeReleaseQueuedLock(Process->QueuedLock);
    Status = STATUS_SUCCESS;

DebugPrintEnd:
    if (NonPagedCopy != NULL) {
        MmFreeNonPagedPool(NonPagedCopy);
    }

    if (PagedCopy != NULL) {
        MmFreePagedPool(PagedCopy);
    }

    return Status;
}

KSTATUS
PspDebugIssueCommand (
    PKPROCESS IssuingProcess,
    PKPROCESS TargetProcess,
    PPROCESS_DEBUG_COMMAND Command
    )

/*++

Routine Description:

    This routine issues a command to a child process.

Arguments:

    IssuingProcess - Supplies a pointer to the process issuing the command (ie
        the debugger process).

    TargetProcess - Supplies a pointer to the process to issue the command to.

    Command - Supplies a pointer to the command to issue.

Return Value:

    Status code indicating whether the command was successfully issued.

--*/

{

    PROCESS_DEBUG_COMMAND LocalCommand;
    BOOL LockHeld;
    ULONG MinSize;
    PVOID OriginalData;
    PPROCESS_DEBUG_COMMAND ProcessCommand;
    KSTATUS Status;

    LockHeld = FALSE;
    Status = STATUS_SUCCESS;
    RtlCopyMemory(&LocalCommand, Command, sizeof(PROCESS_DEBUG_COMMAND));
    LocalCommand.Status = STATUS_NOT_HANDLED;
    LocalCommand.Data = NULL;

    //
    // Fail if that process is not stopped at a tracer break, indicated by the
    // lock being held. If the target process just died, it should not be
    // holding this lock. If it does have the lock, it should not die before
    // the command completes.
    //

    if (KeIsSpinLockHeld(&(TargetProcess->DebugData->TracerLock)) == FALSE) {
        Command->Status = STATUS_NOT_READY;
        goto DebugIssueCommandEnd;
    }

    //
    // Validate the correct size.
    //

    if (((LocalCommand.Command == DebugCommandGetBreakInformation) ||
         (LocalCommand.Command == DebugCommandSetBreakInformation)) &&
        (LocalCommand.Size != sizeof(BREAK_NOTIFICATION))) {

        Command->Status = STATUS_DATA_LENGTH_MISMATCH;
        goto DebugIssueCommandEnd;
    }

    if (((LocalCommand.Command == DebugCommandGetSignalInformation) ||
         (LocalCommand.Command == DebugCommandSetSignalInformation)) &&
        (LocalCommand.Size != sizeof(SIGNAL_PARAMETERS))) {

        Command->Status = STATUS_DATA_LENGTH_MISMATCH;
        goto DebugIssueCommandEnd;
    }

    if ((LocalCommand.Command == DebugCommandRangeStep) &&
        (LocalCommand.Size != sizeof(PROCESS_DEBUG_BREAK_RANGE))) {

        Command->Status = STATUS_DATA_LENGTH_MISMATCH;
        goto DebugIssueCommandEnd;
    }

    //
    // Allocate a buffer if needed.
    //

    if (LocalCommand.Size != 0) {
        LocalCommand.Data = MmAllocatePagedPool(LocalCommand.Size,
                                                PS_ALLOCATION_TAG);

        if (LocalCommand.Data == NULL) {
            Command->Status = STATUS_INSUFFICIENT_RESOURCES;
            goto DebugIssueCommandEnd;
        }

        //
        // Copy the data into the buffer if needed.
        //

        if ((LocalCommand.Command == DebugCommandWriteMemory) ||
            (LocalCommand.Command == DebugCommandSetBreakInformation) ||
            (LocalCommand.Command == DebugCommandSetSignalInformation) ||
            (LocalCommand.Command == DebugCommandRangeStep)) {

            Status = MmCopyFromUserMode(LocalCommand.Data,
                                        Command->Data,
                                        LocalCommand.Size);

            if (!KSUCCESS(Status)) {
                Command->Status = Status;
                Status = STATUS_SUCCESS;
                goto DebugIssueCommandEnd;
            }
        }
    }

    //
    // Copy the command over and wait for it to return. Acquire this process'
    // tracer lock to prevent multiple threads in this process from copying the
    // structure over each other and potentially overwriting kernel memory.
    //

    ProcessCommand = &(TargetProcess->DebugData->DebugCommand);
    KeAcquireSpinLock(&(IssuingProcess->DebugData->TracerLock));
    LockHeld = TRUE;

    ASSERT(ProcessCommand->Command == DebugCommandInvalid);

    KeSignalEvent(TargetProcess->DebugData->DebugCommandCompleteEvent,
                  SignalOptionUnsignal);

    //
    // Copy the command backwards so that the last thing set is the command
    // itself.
    //

    ProcessCommand->Status = LocalCommand.Status;
    ProcessCommand->SignalToDeliver = LocalCommand.SignalToDeliver;
    ProcessCommand->Size = LocalCommand.Size;
    ProcessCommand->Data = LocalCommand.Data;
    ProcessCommand->U = LocalCommand.U;
    RtlMemoryBarrier();
    ProcessCommand->Command = LocalCommand.Command;

    //
    // Signal the stop event to let all the threads party on.
    //

    KeSignalEvent(TargetProcess->StopEvent, SignalOptionSignalAll);

    //
    // Wait for the command to complete.
    //

    KeWaitForEvent(TargetProcess->DebugData->DebugCommandCompleteEvent,
                   FALSE,
                   WAIT_TIME_INDEFINITE);

    //
    // For commands that let 'er rip, the process debug command structure is
    // no longer safe to read. Plus there's nothing to read out of there anyway.
    //

    if ((LocalCommand.Command == DebugCommandContinue) ||
        (LocalCommand.Command == DebugCommandSingleStep) ||
        (LocalCommand.Command == DebugCommandRangeStep)) {

        ProcessCommand->Data = NULL;
        ProcessCommand->Size = 0;
        Command->Status = STATUS_SUCCESS;
        goto DebugIssueCommandEnd;
    }

    ASSERT(ProcessCommand->Size <= LocalCommand.Size);
    ASSERT(ProcessCommand->Command == DebugCommandInvalid);
    ASSERT(ProcessCommand->Data == LocalCommand.Data);

    MinSize = ProcessCommand->Size;
    if (LocalCommand.Size < MinSize) {
        MinSize = LocalCommand.Size;
    }

    //
    // Copy the resulting data back over to the caller for certain events.
    //

    if (((LocalCommand.Command == DebugCommandReadMemory) ||
         (LocalCommand.Command == DebugCommandGetBreakInformation) ||
         (LocalCommand.Command == DebugCommandGetSignalInformation)) &&
        (MinSize != 0)) {

        Status = MmCopyToUserMode(Command->Data, LocalCommand.Data, MinSize);
        if (!KSUCCESS(Status)) {
            Command->Status = Status;
            Status = STATUS_SUCCESS;
        }
    }

    //
    // For commands where all threads are still spinning waiting for
    // instructions, copy the results.
    //

    OriginalData = Command->Data;
    RtlCopyMemory(Command, ProcessCommand, sizeof(PROCESS_DEBUG_COMMAND));
    Command->Data = OriginalData;
    ProcessCommand->Data = NULL;
    ProcessCommand->Size = 0;

DebugIssueCommandEnd:
    if (LocalCommand.Data != NULL) {
        MmFreePagedPool(LocalCommand.Data);
    }

    if (LockHeld != FALSE) {
        KeReleaseSpinLock(&(IssuingProcess->DebugData->TracerLock));
    }

    return Status;
}

KSTATUS
PspCreateDebugDataIfNeeded (
    PKPROCESS Process
    )

/*++

Routine Description:

    This routine creates the debug data structure if it does not already
    exist.

Arguments:

    Process - Supplies a pointer to the process whose debug data is needed.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES if the structure could not be allocated.

--*/

{

    PPROCESS_DEBUG_DATA DebugData;
    BOOL LockHeld;
    KSTATUS Status;

    DebugData = NULL;
    LockHeld = FALSE;

    //
    // Create the debug data structure if it's not there.
    //

    if (Process->DebugData == NULL) {
        KeAcquireQueuedLock(Process->QueuedLock);
        LockHeld = TRUE;
        if (Process->DebugData == NULL) {
            DebugData = MmAllocateNonPagedPool(sizeof(PROCESS_DEBUG_DATA),
                                               PS_ALLOCATION_TAG);

            if (DebugData == NULL) {
                KeReleaseQueuedLock(Process->QueuedLock);
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto CreateDebugDataIfNeededEnd;
            }

            RtlZeroMemory(DebugData, sizeof(PROCESS_DEBUG_DATA));
            INITIALIZE_LIST_HEAD(&(DebugData->TraceeListHead));
            KeInitializeSpinLock(&(DebugData->TracerLock));
            DebugData->Process = Process;
            DebugData->AllStoppedEvent = KeCreateEvent(NULL);
            if (DebugData->AllStoppedEvent == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto CreateDebugDataIfNeededEnd;
            }

            KeSignalEvent(DebugData->AllStoppedEvent, SignalOptionUnsignal);
            DebugData->DebugCommandCompleteEvent = KeCreateEvent(NULL);
            if (DebugData->DebugCommandCompleteEvent == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto CreateDebugDataIfNeededEnd;
            }

            KeSignalEvent(DebugData->DebugCommandCompleteEvent,
                          SignalOptionUnsignal);

            Process->DebugData = DebugData;
        }
    }

    Status = STATUS_SUCCESS;

CreateDebugDataIfNeededEnd:
    if (!KSUCCESS(Status)) {

        ASSERT(Process->DebugData == NULL);

        if (DebugData != NULL) {
            PspDestroyDebugData(DebugData);
        }
    }

    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(Process->QueuedLock);
    }

    return Status;
}

VOID
PspDestroyDebugData (
    PPROCESS_DEBUG_DATA DebugData
    )

/*++

Routine Description:

    This routine destroys the given process debug data structure.

Arguments:

    DebugData - Supplies a pointer to the debug data to destroy.

Return Value:

    None.

--*/

{

    if (DebugData->AllStoppedEvent != NULL) {
        KeDestroyEvent(DebugData->AllStoppedEvent);
    }

    if (DebugData->DebugCommandCompleteEvent != NULL) {
        KeDestroyEvent(DebugData->DebugCommandCompleteEvent);
    }

    MmFreeNonPagedPool(DebugData);
    return;
}

VOID
PspDebugGetLoadedModules (
    PSYSTEM_CALL_DEBUG Command
    )

/*++

Routine Description:

    This routine returns the list of loaded modules in the target debug process.

Arguments:

    Command - Supplies a pointer to the debug command.

Return Value:

    None. The command structure will contain the return status code.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PLOADED_MODULE_ENTRY CurrentModule;
    PKPROCESS CurrentProcess;
    PLOADED_IMAGE Image;
    PMODULE_LIST_HEADER List;
    BOOL LockHeld;
    ULONG ModuleCount;
    PSTR Name;
    ULONG NameSize;
    PKPROCESS Process;
    ULONGLONG Signature;
    ULONGLONG SizeNeeded;
    KSTATUS Status;
    ULONG UserSize;

    CurrentProcess = PsGetCurrentProcess();
    List = NULL;
    LockHeld = FALSE;

    //
    // First, look up the process.
    //

    Process = PspGetProcessById(Command->Process);
    if ((Process == NULL) ||
        (Process->DebugData == NULL) ||
        (Process->DebugData->TracingProcess != CurrentProcess)) {

        Status = STATUS_INVALID_PARAMETER;
        goto DebugGetLoadedModulesEnd;
    }

    //
    // Fail if that process is not stopped at a tracer break, indicated by the
    // lock being held. If the process just died, it should not be holding this
    // lock. If it does have the lock, it should not die before the calling
    // process continues it.
    //

    if (KeIsSpinLockHeld(&(Process->DebugData->TracerLock)) == FALSE) {
        Status = STATUS_NOT_READY;
        goto DebugGetLoadedModulesEnd;
    }

    PsAcquireImageListLock(Process);
    LockHeld = TRUE;

    //
    // Loop through once to find out how much space is needed to enumerate the
    // module list.
    //

    Signature = 0;
    ModuleCount = 0;
    SizeNeeded = sizeof(MODULE_LIST_HEADER);
    CurrentEntry = Process->ImageListHead.Next;
    while (CurrentEntry != &(Process->ImageListHead)) {
        Image = LIST_VALUE(CurrentEntry, LOADED_IMAGE, ListEntry);
        Name = RtlStringFindCharacterRight(Image->FileName, '/', -1);
        if (Name != NULL) {
            Name += 1;

        } else {
            Name = Image->FileName;
        }

        SizeNeeded += sizeof(LOADED_MODULE_ENTRY) +
                      ((RtlStringLength(Name) + 1 -
                       ANYSIZE_ARRAY) * sizeof(CHAR));

        Signature += Image->File.ModificationDate +
                (UINTN)(Image->PreferredLowestAddress + Image->BaseDifference);

        ModuleCount += 1;
        CurrentEntry = CurrentEntry->Next;
    }

    //
    // Watch out for overflows due to a billion modules or more likely some
    // very long nefarious names.
    //

    if (SizeNeeded > MAX_ULONG) {
        Status = STATUS_BUFFER_OVERRUN;
        goto DebugGetLoadedModulesEnd;
    }

    UserSize = Command->Command.Size;
    Command->Command.Size = SizeNeeded;

    //
    // If the user-mode buffer passed was too small, then just return the size
    // needed.
    //

    if (UserSize < SizeNeeded) {
        Status = STATUS_BUFFER_TOO_SMALL;
        goto DebugGetLoadedModulesEnd;
    }

    //
    // Allocate a buffer to hold all the information in kernel memory. In
    // addition to making the next loop easier on the eyes, it also prevents the
    // situation where two process locks are held at the same time (which could
    // be bad if it's in the wrong order).
    //

    List = MmAllocatePagedPool(SizeNeeded, PS_ALLOCATION_TAG);
    if (List == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto DebugGetLoadedModulesEnd;
    }

    RtlZeroMemory(List, SizeNeeded);
    List->ModuleCount = ModuleCount;
    List->Signature = Signature;
    CurrentModule = (PLOADED_MODULE_ENTRY)(List + 1);

    //
    // Loop through again and create the list.
    //

    CurrentEntry = Process->ImageListHead.Next;
    while (CurrentEntry != &(Process->ImageListHead)) {
        Image = LIST_VALUE(CurrentEntry, LOADED_IMAGE, ListEntry);
        Name = RtlStringFindCharacterRight(Image->FileName, '/', -1);
        if (Name != NULL) {
            Name += 1;

        } else {
            Name = Image->FileName;
        }

        NameSize = (RtlStringLength(Name) + 1) * sizeof(CHAR);
        CurrentModule->StructureSize = sizeof(LOADED_MODULE_ENTRY) + NameSize -
                                       (ANYSIZE_ARRAY * sizeof(CHAR));

        CurrentModule->Timestamp = Image->File.ModificationDate;
        CurrentModule->LowestAddress = (UINTN)Image->LoadedImageBuffer;
        CurrentModule->Size = Image->Size;
        CurrentModule->Process = Command->Process;
        RtlStringCopy(CurrentModule->BinaryName, Name, NameSize);

        //
        // Move on to the next image.
        //

        CurrentModule = (PVOID)CurrentModule + CurrentModule->StructureSize;
        CurrentEntry = CurrentEntry->Next;
    }

    ASSERT((UINTN)CurrentModule - (UINTN)List == SizeNeeded);

    PsReleaseImageListLock(Process);
    LockHeld = FALSE;

    //
    // Copy this assembled data over to user mode.
    //

    Status = MmCopyToUserMode(Command->Command.Data, List, (ULONG)SizeNeeded);
    if (!KSUCCESS(Status)) {
        goto DebugGetLoadedModulesEnd;
    }

    Status = STATUS_SUCCESS;

DebugGetLoadedModulesEnd:
    if (LockHeld != FALSE) {
        PsReleaseImageListLock(Process);
    }

    if (List != NULL) {
        MmFreePagedPool(List);
    }

    if (Process != NULL) {
        ObReleaseReference(Process);
    }

    Command->Command.Status = Status;
    return;
}

VOID
PspDebugGetThreadList (
    PSYSTEM_CALL_DEBUG Command
    )

/*++

Routine Description:

    This routine returns the list of active threads in the target process.

Arguments:

    Command - Supplies a pointer to the debug command.

Return Value:

    None. The command structure will contain the return status code.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PKPROCESS CurrentProcess;
    PTHREAD_ID CurrentThreadEntry;
    BOOL LockHeld;
    PKPROCESS Process;
    ULONGLONG SizeNeeded;
    KSTATUS Status;
    PKTHREAD Thread;
    PULONG ThreadList;

    CurrentProcess = PsGetCurrentProcess();
    LockHeld = FALSE;
    ThreadList = NULL;

    //
    // First, look up the process.
    //

    Process = PspGetProcessById(Command->Process);
    if ((Process == NULL) ||
        (Process->DebugData == NULL) ||
        (Process->DebugData->TracingProcess != CurrentProcess)) {

        Status = STATUS_INVALID_PARAMETER;
        goto DebugGetThreadListEnd;
    }

    //
    // Fail if that process is not stopped at a tracer break, indicated by the
    // lock being held. If the process just died, it should not be holding this
    // lock. If it does have the lock, it should not die before the calling
    // process continues it.
    //

    if (KeIsSpinLockHeld(&(Process->DebugData->TracerLock)) == FALSE) {
        Status = STATUS_NOT_READY;
        goto DebugGetThreadListEnd;
    }

    KeAcquireQueuedLock(Process->QueuedLock);
    LockHeld = TRUE;
    SizeNeeded = sizeof(ULONG) + (Process->ThreadCount * sizeof(THREAD_ID));
    if (SizeNeeded > MAX_ULONG) {
        Status = STATUS_BUFFER_OVERRUN;
        goto DebugGetThreadListEnd;
    }

    Command->Command.Size = SizeNeeded;

    //
    // If the user-mode buffer passed was too small, then just return the size
    // needed.
    //

    if (Command->Command.Size < SizeNeeded) {
        Status = STATUS_BUFFER_TOO_SMALL;
        goto DebugGetThreadListEnd;
    }

    //
    // Allocate a buffer to hold all the information in kernel memory. In
    // addition to making the next loop easier on the eyes, it also prevents the
    // situation where two process locks are held at the same time (which could
    // be bad if it's in the wrong order).
    //

    ThreadList = MmAllocatePagedPool(SizeNeeded, PS_ALLOCATION_TAG);
    if (ThreadList == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto DebugGetThreadListEnd;
    }

    RtlZeroMemory(ThreadList, SizeNeeded);
    *ThreadList = Process->ThreadCount;
    CurrentThreadEntry = (PTHREAD_ID)(ThreadList + 1);

    //
    // Loop through again and create the list.
    //

    CurrentEntry = Process->ThreadListHead.Next;
    while (CurrentEntry != &(Process->ThreadListHead)) {
        Thread = LIST_VALUE(CurrentEntry, KTHREAD, ProcessEntry);
        CurrentEntry = CurrentEntry->Next;
        *CurrentThreadEntry = Thread->ThreadId;
        CurrentThreadEntry += 1;
    }

    ASSERT((UINTN)CurrentThreadEntry - (UINTN)ThreadList == SizeNeeded);

    KeReleaseQueuedLock(Process->QueuedLock);
    LockHeld = FALSE;

    //
    // Copy this assembled data over to user mode.
    //

    Status = MmCopyToUserMode(Command->Command.Data,
                              ThreadList,
                              (ULONG)SizeNeeded);

    if (!KSUCCESS(Status)) {
        goto DebugGetThreadListEnd;
    }

    Status = STATUS_SUCCESS;

DebugGetThreadListEnd:
    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(Process->QueuedLock);
    }

    if (ThreadList != NULL) {
        MmFreePagedPool(ThreadList);
    }

    if (Process != NULL) {
        ObReleaseReference(Process);
    }

    Command->Command.Status = Status;
    return;
}

KSTATUS
PspGetAllProcessInformation (
    PVOID Buffer,
    PUINTN BufferSize
    )

/*++

Routine Description:

    This routine returns information about the active processes in the system.

Arguments:

    Buffer - Supplies an optional pointer to a buffer to write the data into.
        This buffer must be non-paged.

    BufferSize - Supplies a pointer that on input contains the size of the
        input buffer. On output, returns the size needed to contain the data.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_BUFFER_TOO_SMALL if a buffer was supplied but was not big enough to
    contain all the information.

--*/

{

    PKPROCESS Process;
    ULONG ProcessCount;
    PKPROCESS *Processes;
    ULONG ProcessIndex;
    PPROCESS_INFORMATION ProcessInformation;
    UINTN ProcessSize;
    KSTATUS ProcessStatus;
    ULONG RemainingSize;
    ULONG Size;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Processes = NULL;
    ProcessCount = 0;
    Status = PspGetProcessList(&Processes, &ProcessCount);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    Size = 0;
    RemainingSize = *BufferSize;
    Status = STATUS_SUCCESS;
    for (ProcessIndex = 0; ProcessIndex < ProcessCount; ProcessIndex += 1) {
        Process = Processes[ProcessIndex];
        ProcessSize = RemainingSize;
        if (ProcessSize >= sizeof(ULONG)) {
            ProcessInformation = Buffer;
            ProcessInformation->Version = PROCESS_INFORMATION_VERSION;
        }

        ProcessStatus = PspGetProcessInformation(Process, Buffer, &ProcessSize);
        if (!KSUCCESS(ProcessStatus)) {
            Status = ProcessStatus;

        } else if (RemainingSize >= ProcessSize) {
            Buffer += ProcessSize;
            RemainingSize -= ProcessSize;
        }

        Size += ProcessSize;
    }

    PspDestroyProcessList(Processes, ProcessCount);
    *BufferSize = Size;
    return Status;
}

KSTATUS
PspGetProcessInformation (
    PKPROCESS Process,
    PPROCESS_INFORMATION Buffer,
    PUINTN BufferSize
    )

/*++

Routine Description:

    This routine returns information about a given process.

Arguments:

    Process - Supplies a pointer to the process to get information about.

    Buffer - Supplies an optional pointer to a buffer to write the data into.
        This buffer must be non-paged.

    BufferSize - Supplies a pointer that on input contains the size of the
        input buffer. On output, returns the size needed to contain the data.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_BUFFER_TOO_SMALL if a buffer was supplied but was not big enough to
    contain all the information.

--*/

{

    PVOID Arguments;
    PLIST_ENTRY CurrentEntry;
    PLOADED_IMAGE Image;
    PSTR Name;
    UINTN Offset;
    ULONG ProcessSize;
    PROCESS_STATE State;
    KSTATUS Status;
    PKTHREAD Thread;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // Check the version number of the structure if the buffer is not null.
    //

    if ((Buffer != NULL) &&
        (*BufferSize >= sizeof(ULONG)) &&
        (Buffer->Version < PROCESS_INFORMATION_VERSION)) {

        return STATUS_VERSION_MISMATCH;
    }

    //
    // Collect the process information or determine the size of the information.
    //

    Status = STATUS_SUCCESS;
    KeAcquireQueuedLock(Process->QueuedLock);
    ProcessSize = sizeof(PROCESS_INFORMATION) + Process->BinaryNameSize;
    if (Process->Environment != NULL) {
        ProcessSize += Process->Environment->ArgumentsBufferLength;
    }

    ProcessSize = ALIGN_RANGE_UP(ProcessSize, sizeof(ULONG));
    if ((Buffer != NULL) && (*BufferSize >= ProcessSize)) {
        Buffer->StructureSize = ProcessSize;
        Buffer->ProcessId = Process->Identifiers.ProcessId;

        //
        // While the lock is held, the parent should not disappear.
        //

        if (Process->Parent != NULL) {
            Buffer->ParentProcessId = Process->Parent->Identifiers.ProcessId;

        } else {
            Buffer->ParentProcessId = -1;
        }

        if (Process->ProcessGroup != NULL) {
            Buffer->ProcessGroupId = Process->Identifiers.ProcessGroupId;
            Buffer->SessionId = Process->Identifiers.SessionId;

        } else {
            Buffer->ProcessGroupId = -1;
            Buffer->SessionId = -1;
        }

        Buffer->StartTime = Process->StartTime;
        Buffer->NameLength = Process->BinaryNameSize;
        Buffer->NameOffset = 0;
        Offset = sizeof(PROCESS_INFORMATION);
        if (Process->BinaryNameSize != 0) {
            Buffer->NameOffset = Offset;
            Name = (PSTR)((PVOID)Buffer + Offset);
            RtlStringCopy(Name, Process->BinaryName, Buffer->NameLength);
            Offset += Buffer->NameLength;
        }

        Buffer->ArgumentsBufferOffset = 0;
        Buffer->ArgumentsBufferSize = 0;
        if (Process->Environment != NULL) {
            Buffer->ArgumentsBufferOffset = Offset;
            Buffer->ArgumentsBufferSize =
                                   Process->Environment->ArgumentsBufferLength;

            Arguments = (PVOID)Buffer + Offset;
            RtlCopyMemory(Arguments,
                          Process->Environment->ArgumentsBuffer,
                          Buffer->ArgumentsBufferSize);

            Offset += Buffer->ArgumentsBufferSize;
        }

        //
        // Take a look at the threads to get a sense of the process state.
        //

        State = ProcessStateInvalid;
        CurrentEntry = Process->ThreadListHead.Next;
        Thread = NULL;
        while (CurrentEntry != &(Process->ThreadListHead)) {
            Thread = LIST_VALUE(CurrentEntry, KTHREAD, ProcessEntry);
            CurrentEntry = CurrentEntry->Next;
            if (Thread->State == ThreadStateRunning) {
                State = ProcessStateRunning;
                break;
            }

            if ((Thread->State == ThreadStateFirstTime) ||
                (Thread->State == ThreadStateReady)) {

                State = ProcessStateReady;

            } else if ((Thread->State == ThreadStateBlocking) ||
                       (Thread->State == ThreadStateBlocked)) {

                if (State != ProcessStateReady) {
                    State = ProcessStateBlocked;
                }

            } else if ((Thread->State == ThreadStateSuspending) ||
                       (Thread->State == ThreadStateSuspended)) {

                if ((State != ProcessStateReady) &&
                    (State != ProcessStateBlocked)) {

                    State = ProcessStateSuspended;
                }

            } else if (Thread->State == ThreadStateExited) {
                if ((State != ProcessStateReady) &&
                    (State != ProcessStateBlocked) &&
                    (State != ProcessStateSuspended)) {

                    State = ProcessStateExited;
                }
            }
        }

        Buffer->State = State;

        //
        // Use any thread to fill out the process credential information.
        //

        if (Thread != NULL) {
            Buffer->RealUserId = Thread->Identity.RealUserId;
            Buffer->EffectiveUserId = Thread->Identity.EffectiveUserId;
            Buffer->RealGroupId = Thread->Identity.RealGroupId;
            Buffer->EffectiveGroupId = Thread->Identity.EffectiveGroupId;

        } else {
            Buffer->RealUserId = -1;
            Buffer->EffectiveUserId = -1;
            Buffer->RealGroupId = -1;
            Buffer->EffectiveGroupId = -1;
        }

        //
        // TODO: Fill out the remaining process data (user ID, priority, etc).
        //

        Buffer->Priority = 0;
        Buffer->NiceValue = 0;
        Buffer->Flags = 0;

    } else {
        Status = STATUS_BUFFER_TOO_SMALL;
    }

    KeReleaseQueuedLock(Process->QueuedLock);
    if ((Buffer != NULL) && (*BufferSize >= sizeof(PROCESS_INFORMATION))) {
        PspGetProcessResourceUsage(Process,
                                   TRUE,
                                   FALSE,
                                   &(Buffer->ResourceUsage));

        PspGetProcessResourceUsage(Process,
                                   FALSE,
                                   TRUE,
                                   &(Buffer->ChildResourceUsage));

        Buffer->Frequency = HlQueryProcessorCounterFrequency();

        //
        // Get the size of the first image on the process's image list. This
        // should be the main image.
        //

        PsAcquireImageListLock(Process);
        if (LIST_EMPTY(&(Process->ImageListHead)) == FALSE) {
            Image = LIST_VALUE(Process->ImageListHead.Next,
                               LOADED_IMAGE,
                               ListEntry);

            Buffer->ImageSize = Image->Size;
        }

        PsReleaseImageListLock(Process);
    }

    *BufferSize = ProcessSize;
    return Status;
}

VOID
PspGetProcessTimes (
    PKPROCESS Process,
    PULONGLONG UserTime,
    PULONGLONG KernelTime,
    PULONGLONG ChildrenUserTime,
    PULONGLONG ChildrenKernelTime
    )

/*++

Routine Description:

    This routine returns the total user and kernel mode time this process has
    spent executing and the total user and kernel mode time any waited-on child
    processes have spent executing. This routine assumes that the process
    queued lock is held.

Arguments:

    Process - Supplies a pointer to a process.

    UserTime - Supplies a pointer that receives the total amount of time, in
        clock ticks, that the process has spent in user mode.

    KernelTime - Supplies a pointer that receives the total amount of time, in
        clock ticks, that the process has spent in kernel mode.

    ChildrenUserTime - Supplies a pointer that receives the total amount of
        time, in  clock ticks, that the process's waited-on children have spent
        in user mode.

    ChildrenKernelTime - Supplies a pointer that receives the total amount of
        time, in  clock ticks, that the process's waited-on children have spent
        in kernel mode.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PKTHREAD Thread;

    ASSERT(KeIsQueuedLockHeld(Process->QueuedLock) != FALSE);

    //
    // The process only holds the times of children that have been waited on
    // and threads that have exited. Collect the values from all the active
    // threads. And since the lock is held, just snap the other values as well
    // to avoid tears.
    //

    *UserTime = Process->ResourceUsage.UserCycles;
    *KernelTime = Process->ResourceUsage.KernelCycles;
    CurrentEntry = Process->ThreadListHead.Next;
    while (CurrentEntry != &(Process->ThreadListHead)) {
        Thread = LIST_VALUE(CurrentEntry, KTHREAD, ProcessEntry);
        *UserTime += Thread->ResourceUsage.UserCycles;
        *KernelTime += Thread->ResourceUsage.KernelCycles;
        CurrentEntry = CurrentEntry->Next;
    }

    *ChildrenUserTime = Process->ChildResourceUsage.UserCycles;
    *ChildrenKernelTime = Process->ChildResourceUsage.KernelCycles;
    return;
}

VOID
PspGetProcessResourceUsage (
    PKPROCESS Process,
    BOOL IncludeProcess,
    BOOL IncludeChildren,
    PRESOURCE_USAGE Usage
    )

/*++

Routine Description:

    This routine returns resource usage information for the given process.

Arguments:

    Process - Supplies the process to get resource usage information for.

    IncludeProcess - Supplies a boolean indicating whether resource usage
        information for the process itself (including all of its past and
        current threads) should be included in the results.

    IncludeChildren - Supplies a boolean indicating whether resource usage
        information for all terminated and waited-for children should be
        included in the results.

    Usage - Supplies a pointer where the usage information is returned.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    RESOURCE_USAGE SnappedUsage;
    PKTHREAD Thread;

    RtlZeroMemory(Usage, sizeof(RESOURCE_USAGE));

    //
    // To get the usage for the process, add the usage of all the threads
    // together.
    //

    if (IncludeProcess != FALSE) {
        KeAcquireQueuedLock(Process->QueuedLock);
        CurrentEntry = Process->ThreadListHead.Next;
        while (CurrentEntry != &(Process->ThreadListHead)) {
            Thread = LIST_VALUE(CurrentEntry, KTHREAD, ProcessEntry);
            CurrentEntry = CurrentEntry->Next;
            PspReadResourceUsage(&SnappedUsage, &(Thread->ResourceUsage));
            SnappedUsage.MaxResidentSet =
                           Thread->OwningProcess->AddressSpace->MaxResidentSet;

            PspAddResourceUsages(Usage, &SnappedUsage);
        }

        //
        // Also add the accumulated value of previously exited threads.
        //

        PspReadResourceUsage(&SnappedUsage, &(Process->ResourceUsage));
        SnappedUsage.MaxResidentSet = Process->AddressSpace->MaxResidentSet;
        KeReleaseQueuedLock(Process->QueuedLock);
        PspAddResourceUsages(Usage, &SnappedUsage);
    }

    if (IncludeChildren != FALSE) {
        PspReadResourceUsage(&SnappedUsage, &(Process->ChildResourceUsage));
        PspAddResourceUsages(Usage, &SnappedUsage);
    }

    return;
}

VOID
PspReadResourceUsage (
    PRESOURCE_USAGE Destination,
    PRESOURCE_USAGE Source
    )

/*++

Routine Description:

    This routine takes a snapshot of resource usage.

Arguments:

    Destination - Supplies a pointer where the resource usage is returned.

    Source - Supplies the resource usage structure to read.

Return Value:

    None.

--*/

{

    RESOURCE_USAGE Copy;

    do {
        RtlCopyMemory(Destination, Source, sizeof(RESOURCE_USAGE));
        RtlCopyMemory(&Copy, Source, sizeof(RESOURCE_USAGE));

    } while (RtlCompareMemory(Destination, &Copy, sizeof(RESOURCE_USAGE)) ==
             FALSE);

    return;
}

